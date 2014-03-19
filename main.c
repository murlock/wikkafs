#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


#include <syslog.h>
#include <stdarg.h>

#include "wikkafs.h"

int log_level = LOG_ERR;

void save_log( int level, const char *format, ...) {
	if ( level>log_level ) return;

	va_list ap;
	va_start( ap, format );
	
	if ( fuse_debug || fuse_foreground )
		vfprintf( stderr, format, ap );
	else
		vsyslog( level, format, ap );

	va_end( ap );
}


void set_or_replace( char **var, char *value ) {
	if ( *var )
		free( *var );
	if ( value )
		*var = strdup( value );
	else
		*var = NULL;
}

w_page *Pages = NULL;

w_page* search( const char *name ) {
	w_page *ptr = Pages;
	while ( ptr ) {
		if ( strcmp( ptr->tag, name ) == 0 ) 
			return ptr;
		ptr = ptr->next;
	}
	return NULL;
}

int check_wikiname( const char *name ) {
	if ( strlen(name)== 0 )
		return -1;

	char part1[80], part2[80], part3[80], part4[80];
	int r =	sscanf( name, "%[A-Z]%[a-z]%[A-Z]%[a-z]",  part1, part2, part3, part4 );

	if ( r!= 4 )
		return -1;

	if ( strlen(part1)>1 || strlen(part3)>1 )
		return -1;
	
	char fullname[80];
	snprintf( fullname, sizeof(fullname), "%s%s%s%s", part1, part2, part3, part4 );

	return strcmp( name, fullname );
}


int usage() {
	printf( "WikkaFS: \n"
		"usage: wikkafs [OPTS] mountpoint\n"
		"  -d : fuse debug (message on stderr)\n"
		"  -f : fuse foreground (message on stderr)\n"
		"  -w : wiki user (should be set otherwise updated/created will be as orphans !)\n"
		"  -t : mysql prefix for table\n"
		"  -u : sql username (default: wiki)\n"
		"  -p : sql password (default: wiki)\n"
		"  -b : sql database to use (default: wiki)\n"
		"  -h : host (default: 127.0.0.1)\n"
		"  -l : port to use (default: 3306)\n"
		"  -s : use SSL with connect to SQL\n"
		"\n"
		"You can also use WIKKAFS as env variable\n"
	);
	return 0;
}

void init_options() {
	fuse_debug = 0;
	fuse_foreground = 0;

	wikka_user   = strdup( "WikkaFS" );
	wikka_prefix = strdup( "wiki_" );

	bdd_user = strdup( "wiki" );
	bdd_pass = strdup( "wiki" );
	bdd_name = strdup( "wiki" );
	bdd_host = strdup( "127.0.0.1" );
	bdd_port = 3306;
	bdd_ssl  = 0;
}

int parse_options( int argc, char *argv[] ) {
	int c=0;
	opterr = 0;
	optind = 1; // restart parse

	while ( (c = getopt( argc, argv, "dfw:t:u:p:b:h:l:s")) != -1 ) {
		switch (c) {
			case 'd': /* debug mode */
				fuse_debug = !fuse_debug;
				break;
			case 'f': /* foreground mode */
				fuse_foreground = !fuse_foreground;
				break;
			case 'w':
				set_or_replace( &wikka_user, optarg );
				break;
			case 't':
				set_or_replace( &wikka_prefix, optarg );
				break;
			case 'u':
				set_or_replace( &bdd_user, optarg );
				break;
			case 'p':
				set_or_replace( &bdd_pass, optarg );
				break;
			case 'b':
				set_or_replace( &bdd_name, optarg );
				break;
			case 'h':
				set_or_replace( &bdd_host, optarg );
				break;
			case 'l':
				bdd_port = atoi( optarg );
				break;
			case 's':
				bdd_ssl = !bdd_ssl;
				break;
			default:
				usage();
				exit(1);
		}
	}

	if ( fuse_debug || fuse_foreground) {
		log_level = LOG_DEBUG;
	}

	if ( optind+1 == argc ) {
		set_or_replace( &fuse_mountpoint, argv[optind] );
	} 
	return 0;
}

int main(int argc, char *argv[])
{
	// set default options
	init_options();

	// parse WIKKAFS env if set
	char *opt_env = getenv( "WIKKAFS" );
	if ( opt_env ) {
		char *tmp = strdup( opt_env );
		char **env_argv = malloc( sizeof(char*) );
		int env_argc = 1;
		env_argv[0] = "Toto";

		char *cur_ptr = strtok( tmp, " " );	
		while ( cur_ptr ) {
			printf(" env[%d] = %s\n", env_argc, cur_ptr );
			env_argc++;
			env_argv = realloc( env_argv, sizeof(char*) * env_argc );
			env_argv[ env_argc - 1 ] = cur_ptr;

			cur_ptr = strtok( NULL, " " );	
		}
		parse_options( env_argc, env_argv );

		free( tmp );
	}

	// parse args
	parse_options( argc, argv );

	if ( fuse_mountpoint == NULL ) {
		printf("Mountpoint is empty or doesn't exists\n");
		return -2;
	}

	int r = sql_init();
	if ( r ) {
		printf("SQL connect has failed, exiting...");
		return -1;
	}

	Pages = fill();

	/* Build argv & argc for FUSE */

	/* -s
	   -d if fuse_debug
		 -f if fuse_foreground 
		 fuse_mountpoint */

	r = start_fuse_v2();

	sql_close();

	return r;
}

