// vi: ts=4 sw=4 noet:
/*
==================================================================================
	Copyright (c) 2020 Nokia
	Copyright (c) 2020 AT&T Intellectual Property.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
==================================================================================
*/

/*
	Mnemonic:	jwrapper.c
	Abstract:	A wrapper interface to the jsmn library which makes it a bit easier
				to use.  Parses a json string capturing the contents in a symtab.

				This code is based on the AT&T VFd open source library available
				on github.com/att/vfd.  The changes are mostly to port to the
				RMR version of symtab from VFd's version, however the parsing loop
				was considerably refactored to eliminate the bad practices in the
				original code, and to squelch the sonar complaint about a potential,
				though not valid, NPE.

	Author:		E. Scott Daniels
	Date:		26 June 2020

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef DEBUG
	#define DEBUG 0
#endif

#define JSMN_STATIC 1		// jsmn no longer builds into a library; this pulls as static functions
#include <jsmn.h>

#include <rmr/rmr_symtab.h>

extern void jw_nuke( void* st );

#define JSON_SYM_NAME	"_jw_json_string"
#define MAX_THINGS		1024 * 4	// max objects/elements

#define PT_UNKNOWN		0			// primative types; unknown for non prim
#define PT_VALUE		1
#define PT_BOOL			2
#define PT_NULL			3
#define PT_STRING		4

#define OBJ_SPACE		1			// space in the symbol table where json bits are stashed
#define MGT_SPACE		2			// non-json objects in the hash (management things)


// ---------------------------------------------------------------------------------------

/*
	This is what we will manage in the symtab. Right now we store all values (primatives)
	as double, but we could be smarter about it and look for a decimal. Unsigned and
	differences between long, long long etc are tough.
*/
typedef struct jthing {
	int jsmn_type;				// propigated type from jsmn (jsmn constants)
	int prim_type;				// finer grained primative type (bool, null, value)
	int	nele;					// number of elements if applies
	union {
		double fv;
		void *pv;
	} v;
} jthing_t;


/*
	Given the json token, 'extract' the element by marking the end with a
	nil character, and returning a pointer to the start.  We do this so that
	we don't create a bunch of small buffers that must be found and freed; we
	can just release the json string and we'll be done (read won't leak).
*/
static char* extract( char* buf, const jsmntok_t *jtoken ) {
	buf[jtoken->end] = 0;
	return &buf[jtoken->start];
}

#if DEBUG > 0
/*
	For debugging we should NOT extract as that disrupts the "flow" by
	adding a nil before the parser gets a chance to actually parse an
	object.
*/
static void pull( const char* dest, char* src, jsmntok_t* jtoken ) {
	memcpy( dest, &src[jtoken->start], jtoken->end - jtoken->start );
	dest[jtoken->end - jtoken->start] = 0;
}
#endif

/*
	create a new jthing and add a reference to it in the symbol table st.
	sets the number of elements to 1 by default.
*/
static jthing_t *mk_thing( void *st, char *name, int jsmn_type ) {
	jthing_t	*jtp = NULL;

	if( st != NULL && name != NULL ) {
		if( (jtp = (jthing_t *) malloc( sizeof( *jtp ) )) != NULL ) {

			if( DEBUG > 1 ) fprintf( stderr, "<DBUG> jwrapper adding: %s type=%d\n", name, jsmn_type );

			jtp->jsmn_type = jsmn_type;
			jtp->prim_type = PT_UNKNOWN;			// caller must set this
			jtp->nele = 1;
			jtp->v.fv = 0;

			rmr_sym_put( st, name, OBJ_SPACE, jtp );
		} else {
			fprintf( stderr, "[WARN] jwrapper: unable to create '%s' type=%d\n", name, jsmn_type );
		}
	}

	return jtp;
}


/*
	Find the named array. Returns a pointer to the jthing that represents
	the array (type, size and pointer to actual array of jthings).
	Returns nil pointer if the named thing isn't there or isn't an array.
*/
static jthing_t* suss_array( void* st, const char* name ) {
	jthing_t* jtp = NULL;							// thing that is referenced by the symtab

	if( st != NULL && name != NULL ) {
		if( (jtp = (jthing_t *) rmr_sym_get( st, name, OBJ_SPACE )) != NULL ) {
			jtp =  jtp->jsmn_type == JSMN_ARRAY  ? jtp : NULL;
		}
	}

	return jtp;
}

/*
	Suss an array from the hash and return the ith element.
*/
static jthing_t* suss_element( void* st, const char* name, int idx ) {
	jthing_t* jtp;									// thing that is referenced by the symtab
	jthing_t* jarray;
	jthing_t* rv = NULL;

	if(    (jtp = suss_array( st, name )) != NULL		// have pointer
		&& idx >= 0										// and in range
		&& idx < jtp->nele
		&& jtp->v.pv != NULL ) {						// and exists
			jarray = jtp->v.pv;
			rv = &jarray[idx];
	}

	return rv;
}


/*
	Invoked for each thing in the symtab; we free the things that actually point to
	allocated data (e.g. arrays) and recurse to handle objects.

	Only the element passed is used, but this is a prototype which is required by
	the RMR symtab implementaion, so we play games in the code to keep sonar quiet.


	Sonar will grumble aobut the parms needing to be marked const. Until RMR changes
	the signature we can't and sonar will just have to unbunch its knickers.
*/
static void nix_things( void* st, void* se, const char* name, void* ele, void *data ) {
	jthing_t*	j;
	jthing_t*	jarray;
	int i;

	if( (j = (jthing_t *) ele) == NULL )  {
		if( st == NULL && name == NULL && se == NULL && data == NULL ) {	// these are ignored, but this keeps sonar from screaming bug
			fprintf( stderr, "jwrapper: nix_thigs: all params were nil\n" );
		}
		return;
	}

	switch( j->jsmn_type ) {
		case JS
