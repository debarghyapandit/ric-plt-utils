// vi: ts=4 sw=4 noet:
/*
==================================================================================
    Copyright (c) 2020 AT&T Intellectual Property.
    Copyright (c) 2020 Nokia

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
    Mnemonic:	config_test.cpp
    Abstract:	Unit test for config odule

    Date:       13 July 2020
    Author:     E. Scott Daniels
*/

#include <memory>


#include "config.h"			// things under test to build with coverage options
#include "config.cpp"

#include "ut_support.cpp"

/*
	Drive the tests across a single config file.
*/
static int drive_config( std::string fname, bool expect_port ) {
	int errors = 0;
	std::shared_ptr<xapp::Jhash>	jh;
	std::string svalue;
	std::string url_def;
	bool	bvalue;

	jh = munchkin::Conf_parse( fname );
	errors += fail_if( jh == NULL, "did not allocate json hash for valid file (some tests being skipped as a result)" );

	if( jh != NULL ) {

		// --- test digging from controls section -----------------------------------------
		url_def = "https://localhost:29080";
		svalue = munchkin::Conf_get_cvstr( jh, "collector_url", url_def );				// override default expected
		errors += fail_if( svalue.compare( "" ) == 0, "cvstr for good value returned empty string" );
		errors += fail_if( svalue.compare( url_def ) == 0, "cvstr for good value returned the default" );

		bvalue = munchkin::Conf_get_cvbool( jh, "hr_logging", false );
		errors += fail_if( !bvalue, "hr_logging reported false" );

		bvalue = munchkin::Conf_get_cvbool( jh, "hr_logging" );
		errors += fail_if( !bvalue, "hr_logging with no default reported false" );

		bvalue = munchkin::Conf_get_cvbool( jh, "novar", false );
		errors += fail_if( bvalue, "no bool reported true" );

		svalue = munchkin::Conf_get_cvstr( jh, "Xcollector_url", url_def );				// default expected
		errors += fail_if( svalue.compare( "" ) == 0, "cvstr for missing value returned empty string" );
		errors += fail_if( svalue.compare( url_def ) != 0, "cvstr for missing value did not return the default" );

		svalue = munchkin::Conf_get_cvstr( jh, "Xcollector_url" );						// coverage w/o defaut; expect ""
		errors += fail_if( svalue.compare( "" ) != 0, "cvstr for missing value didn't return the  empty string" );

		// ---- test ability to dig port string out ---------------------------------------
		svalue = munchkin::Conf_get_port( jh, "rmr-data" );
		if( expect_port ) {
			errors += fail_if( svalue.compare( "4560" ) != 0, "didn't get port" );
		} else {
			errors += fail_if( svalue.compare( "45l0" ) == 0, "got port when not expected" );
		}

	}

	return errors;
}

int main( int argc, char** argv ) {
	std::string svalue;
	std::shared_ptr<xapp::Jhash>	jh;
	int errors = 0;

	set_test_name( "config_test" );


	fprintf( stderr, "<INFO> ---- warning messages expected below this and are OK\n" );

	jh = munchkin::Conf_parse( "data/no_such_file" );
	errors += fail_if( jh != NULL, "allocated json hash for a bad file" );

	errors += drive_config( "data/config.json", true );		// "normal" config
	errors += drive_config( "data/config2.json", true );		// "additional ports and ordering to drive coverage
	errors += drive_config( "data/config3.json", false );

	svalue = munchkin::Conf_get_cvstr( NULL, "Xcollector_url" );
	errors += fail_if( svalue.compare( "" ) != 0, "given a nil pointer didn't return empty value" );

	svalue = munchkin::Conf_get_port( NULL, "rmr-data" );
	errors += fail_if( svalue.compare( "" ) != 0, "get port given a nil pointer didn't return empty value" );

	announce_results( errors );
	exit( !! errors );
}
