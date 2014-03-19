#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <mysql/mysql.h>

#include "wikkafs.h"

MYSQL *bdd = NULL;

char *bdd_host = NULL;
char *bdd_user = NULL;
char *bdd_pass = NULL;
char *bdd_name = NULL;
int   bdd_port = -1;
int   bdd_ssl  = 0;

char *wikka_user = NULL;
char *wikka_prefix = NULL;


static MYSQL_RES* run_query( const char *sql ) {

	int r = mysql_real_query( bdd, sql, strlen( sql ) );
	if ( r ) {
		printf( "mysql_real_query() failed : %s\n", mysql_error( bdd ) );
		return NULL;
	}

	MYSQL_RES *res = mysql_store_result( bdd );
	if ( !res ) {
		printf( "mysql_store_result() failed : %s\n", mysql_error( bdd ) );
		return NULL;
	}

	return res;
}

//////////////////
// SQL Functions
int SQL_connect() {
	bdd = mysql_init( bdd );
	if ( bdd == NULL ) {
		printf( "mysql_init() failed\n" );
		return 1;
	}

	unsigned long flags = 0;
	if ( bdd_ssl ) {
		flags |= CLIENT_SSL;

		// should call http://dev.mysql.com/doc/refman/5.1/en/mysql-ssl-set.html
		// and should use MYSQL_OPT_SSL_VERIFY_SERVER_CERT
		printf("using SSL\n");
	}

	bdd = mysql_real_connect( bdd, bdd_host, bdd_user, bdd_pass, bdd_name, bdd_port, NULL, flags );
	if ( bdd == NULL ) {
		printf( "mysql_real_connect() failed\n" );
		return 1;
	}

	return 0;
}

int SQL_disconnect() {
	mysql_close( bdd );
	return 0;
}

//////////////////
//

char *get_content( char *tag, int rev ) {
	char sql[1024];
	if ( rev == -1 ) {
		sprintf( sql,
		"SELECT body "
		"FROM %spages "
		"WHERE latest = 'Y' AND tag = '%s' ",
		wikka_prefix,
		tag );
	} else {
		sprintf( sql,
		"SELECT body "
		"FROM %spages "
		"WHERE latest = 'N' AND tag = '%s' AND id = %d ",
		wikka_prefix,
		tag,
		rev );
	}

	MYSQL_RES *res = run_query( sql );
	if ( res == NULL ) {
		return NULL;
	}

	char *result = NULL;

	MYSQL_ROW row = mysql_fetch_row(res);
	if ( row )
		result =strdup( row[0] );

	mysql_free_result( res );
	return result;
}

int get_revisions( w_page *page ) {
	char sql[1024];
	sprintf( sql, 
		"SELECT id FROM %spages"
		"WHERE latest = 'N' AND tag = '%s'"
		"ORDER BY id", 
		wikka_prefix,
		page->tag );
	
	
	MYSQL_RES *res = run_query( sql );
	if ( res == NULL ) {
		return -1;
	}

	if ( page->revisions ) {
		free( page->revisions );
		page->revisions = NULL;
		page->nb_revisions = 0;
	}
	long nb = mysql_num_rows( res );
	if ( nb == 0 ) {
		mysql_free_result( res );
		return 0;
	}
	page->revisions = malloc( sizeof(int) * (nb+1) );	

	MYSQL_ROW row;
	int i=0;
	while ( ( row=mysql_fetch_row(res) ) ) {
		page->revisions[ i++ ] = atoi( row[0] );
	}
	page->revisions[ i ] = -1;
	page->nb_revisions = i;

	mysql_free_result( res );
	return 0;
}

int update_content( const char *tag, const char *body ) {
	char *tag_s = malloc( strlen(tag)*2 + 1 );
	char *body_s = malloc( strlen(body)*2 + 1 );
	mysql_real_escape_string( bdd, tag_s, tag, strlen(tag) );
	mysql_real_escape_string( bdd, body_s, body, strlen(body) );

	// FIXME: if new body is same as previous body, do nothing
	int len = 256 + strlen(tag)*2 + 1 + strlen(body)*2 + 1 + strlen(wikka_prefix);
	char *sql = malloc( len );
	snprintf( sql, len, "UPDATE %spages SET latest = 'N' WHERE tag = '%s'", wikka_prefix, tag_s );
	save_log( LOG_ERR, "=>'%s'", sql );
	int r = mysql_real_query( bdd, sql, strlen( sql ) );
	if ( r ) {
		save_log( LOG_ERR, "mysql_real_query() failed : %s\n", mysql_error( bdd ) );
	}

	// insert new revision
	snprintf( sql, len, 
		"INSERT INTO %spages "
			"SET tag = '%s', "
			" time = now(), "
			" note = 'inserted by WikkaFS', "
			" latest = 'Y',"
			" body = '%s', "
			" owner = '%s', "
			" user = '%s' ",
				wikka_prefix,
				tag_s,
				body_s,
				wikka_user,
				wikka_user
		);

	save_log( LOG_ERR, "=>'%s'", sql );
	r = mysql_real_query( bdd, sql, strlen( sql ) );
	if ( r ) {
		save_log( LOG_ERR, "mysql_real_query() failed : %s\n", mysql_error( bdd ) );
	}

	free( tag_s );
	free( body_s );
	return 0;		
}


/////////////////
// Fill w_page
w_page* fill() {
	// List all Pages

	char sql[256];
	snprintf( sql, sizeof(sql),
		"SELECT DISTINCT tag, owner, LENGTH(body), UNIX_TIMESTAMP( time ) created "
		"FROM %spages "
		"WHERE latest = 'Y' "
		"ORDER BY tag",
		wikka_prefix );

	MYSQL_RES *res = run_query( sql );
	if ( res == NULL ) {
		return NULL;
	}

	MYSQL_ROW row;

	w_page *root_ptr = NULL;
	w_page *current_ptr = NULL, *last_ptr = NULL;

	while ( ( row=mysql_fetch_row(res) ) ) {
		current_ptr = calloc( 1, sizeof(*current_ptr) );
		current_ptr->tag     = strdup( row[0] );
		current_ptr->owner   = strdup( row[1] );
		current_ptr->size    = atoi( row[2] );
		current_ptr->created = atoi( row[3] );

		if ( last_ptr )	last_ptr->next = current_ptr;

		if ( !root_ptr )	root_ptr = current_ptr;
		last_ptr = current_ptr;
	}

	mysql_free_result( res );
	return root_ptr;
}

int sql_init() {
	if ( SQL_connect() ) {
		return (-1);
	}
	return 0;
}

int sql_close() {
	if ( SQL_disconnect() ) {
		return (-1);
	}
	return 0;
}
