#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <syslog.h>

#define FUSE_USE_VERSION 28
#define UNUSED __attribute__((unused))

#include <fuse.h>

#include "wikkafs.h"


static int start_fd = 1;


/* Other way to show files */

/* All pages are present in /
   Each previous revisions for a page will be present in .PageName/r{i}
 */

w_page* path_to_page_v2( const char *path, char **sub ) {

	/* path is like /MaPage or /.MaPage/r155 */
	if ( strlen(path)<1 || path[0] != '/' )
		return NULL;

	char *name = strdup(path);
	char *ptr = name+1;


	*sub = NULL;

	if ( ptr[0] == '.' ) { // we have a request to a revision ?
		// TODO
		//save_log( LOG_ERR, "TODO" );
		return NULL;
	}

	// we check if asked page is present in Page
	w_page *page = search( ptr );
	if ( !ptr ) {
		free( name );
		return NULL;
	}

	free( name );
	return page;
}

static int wikka_getattr_v2(const char *path, struct stat *stbuf)
{
	int res = -ENOENT;

	if(strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	char *sub_page;
	w_page *cur_page = path_to_page_v2( path, &sub_page );
	if ( cur_page && sub_page == NULL && cur_page->deleted == 0 ) {
		stbuf->st_mode = S_IFREG | 0755;
		stbuf->st_mtime = cur_page->created;
		stbuf->st_size = cur_page->size;
		stbuf->st_nlink = 1;
		return 0;
	}
	return res;
}


static int wikka_readdir_v2( const char *path, 
		void *buf, fuse_fill_dir_t filler, UNUSED off_t offset, UNUSED struct fuse_file_info *fi ) {

	save_log( LOG_INFO, "%s with %s\n", __FUNCTION__, path );
	if ( strcmp( path, "/" ) == 0 ) {
		filler( buf, "." , NULL, 0 );
		filler( buf, ".." , NULL, 0 );

		w_page *cur = Pages;
		while( cur ) {
			struct stat *stbuf = NULL;
			/* FIXME try to understand why we can fill a stbuf but doesn't seems to be use
			   malloc( sizeof(struct stat) );
			   memset(stbuf, 0, sizeof(struct stat));
			   stbuf->st_mode = S_IFDIR | 0755;
			   stbuf->st_nlink = 2; */

			if ( !cur->deleted ) /* don't show deleted file */
				filler( buf,  cur->tag ,stbuf, 0 );
			cur=cur->next;
		}
		return 0;
	}
	if ( strlen(path)>1 && path[0]=='/' && path[1]=='.' ) {
		// is a .MaPage directory name ?
		// check if MaPage exists and if hold revision
		//	return 0;
		return -ENOENT;
	}

	save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
	return -ENOENT;
}


static int wikka_open_v2(const char *path, struct fuse_file_info *fi)
{
	char *sub_page = NULL;
	int f = fi->flags;

	save_log( LOG_ERR, "Try to open %s with fi->flags : %d\n", path, fi->flags );

	if ( f & O_APPEND )
		save_log( LOG_ERR, "  O_APPEND\n" );
	if ( f & O_ASYNC )
		save_log( LOG_ERR, "  O_ASYNC\n" );
#ifdef _GNU_SOURCE		
	if ( f & O_CLOEXEC )
		save_log( LOG_ERR, "  O_CLOEXEC\n" );
	if ( f & O_DIRECT )	
		save_log( LOG_ERR, "  O_DIRECT\n" );
	if ( f & O_DIRECTORY )	
		save_log( LOG_ERR, "  O_DIRECTORY\n" );
	if ( f & O_NOATIME )	
		save_log( LOG_ERR, "  O_NOATIME\n" );
	if ( f & O_NOFOLLOW )	
		save_log( LOG_ERR, "  O_NOFOLLOW\n" );
#endif		
	if ( f & O_CREAT )
		save_log( LOG_ERR, "  O_CREAT\n" );
	if ( f & O_EXCL )	
		save_log( LOG_ERR, "  O_EXCL\n" );
#ifdef _LARGEFILE64_SOURCE		
	if ( f & O_LARGEFILE )	
		save_log( LOG_ERR, "  O_LARGEFILE\n" );
#endif		
	if ( f & O_NOCTTY )	
		save_log( LOG_ERR, "  O_NOCTTY\n" );
	if ( f &  O_NONBLOCK )	
		save_log( LOG_ERR, "  O_NONBLOCK\n" );
	if ( f & O_SYNC )	
		save_log( LOG_ERR, "  O_SYNC\n" );
	if ( f & O_TRUNC )	
		save_log( LOG_ERR, "  O_TRUNC\n" );
	

	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( cur_page && sub_page == NULL )  {
		fi->fh = start_fd++;
		save_log( LOG_ERR, "%s with %s : (fh set to %llu)\n", __FUNCTION__, path, fi->fh );
		//free(sub_page);
		return 0;
	}
	save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
	return -ENOENT;
}

static int wikka_read_v2(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {

	save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( !cur_page || sub_page != NULL ) {
		save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
		return -ENOENT;
	}

	size_t len;
	if ( cur_page->page_current == NULL ) {
		cur_page->page_current = get_content( cur_page->tag, -1 );
	}

	len = strlen( cur_page->page_current );
	if (offset < (long int) len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, cur_page->page_current + offset, size);
	} else
		size = 0;

	return size;
}


int wikka_write_v2 (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( !cur_page || sub_page ) { // no write other than on current page
		save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
		return -ENOENT;
	}

	if ( cur_page->fh == 0 ) {
		cur_page->fh = fi->fh;
		if ( cur_page->page_current == NULL ) {
			cur_page->page_current = get_content( cur_page->tag, -1 );
		}
		cur_page->page_changed = strdup( cur_page->page_current );
	}

	if ( cur_page->fh == (long int) fi->fh && cur_page->page_changed != NULL ) {
		if ( offset + size +1 > strlen( cur_page->page_changed ) ) {
			cur_page->page_changed = realloc( cur_page->page_changed, offset + size + 1 );
		}
		memcpy( cur_page->page_changed + offset, buf, size );
		cur_page->page_changed[ offset + size ] = '\0';
		return size;
	} else {
		save_log( LOG_ERR, "Other process want to write on %s\n", cur_page->tag );
	}


	// FIXME: is it the real error code to return ?
	return -ENOENT;
}

int wikka_release_v2(const char *path, struct fuse_file_info *fi) {
	save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( !cur_page || sub_page ) {
		save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
		if ( sub_page )
			free( sub_page );
		return -ENOENT;
	}

	int need_refresh = 0;
	if ( cur_page->fh == (long int) fi->fh && cur_page->page_changed != NULL ) {

		// we discard changes since we disallow empty Wiki Page
		if ( strlen( cur_page->page_changed ) > 0 ) {
			save_log( LOG_ERR, "Save %s\n", path );
			// we need to 'commit' the updated page
			update_content( cur_page->tag, cur_page->page_changed );
		}

		free( cur_page->page_changed );
		cur_page->page_changed = NULL;
		cur_page->fh = 0;

		need_refresh = 1;
	}

	if ( cur_page->page_current )
		free( cur_page->page_current );
	cur_page->page_current = NULL;
	if ( need_refresh ) {
		w_page *ptr = Pages, *tmp;
		while ( ptr ) {
			tmp=ptr->next;
			if ( ptr->tag )	free( ptr->tag );
			if ( ptr->owner )	free( ptr->owner );
			if ( ptr->page_current )	free( ptr->page_current );
			if ( ptr->revisions )	free( ptr->revisions );
			if ( ptr->page_changed )	free( ptr->page_changed );
			free(ptr);
			ptr=tmp;
		}
		// refresh all pages
		Pages = fill();
	}
	return 0;
}


int wikka_ftruncate_v2( const char *path, off_t offset, struct fuse_file_info *fi ) {
	save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( !cur_page || sub_page ) {
		save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
		return -ENOENT;
	}

	if ( cur_page->fh == 0 ) {
		cur_page->fh = fi->fh;
		cur_page->page_changed = strdup( cur_page->page_current );
	}

	if ( cur_page->fh == (long int) fi->fh && cur_page->page_changed != NULL ) {
		if ( offset + 1 > (long int) strlen( cur_page->page_changed ) ) {
			save_log( LOG_ERR, "Truncate to extend page not implemented (offset : %llu / strlen : %d)", offset, strlen( cur_page->page_changed ) );
			return -ENOENT;
		}
		cur_page->page_changed[ offset ] = '\0';

		return 0;
	}

	// FIXME: is it the real error code to return ?
	return -ENOENT;
}

/** Truncate a file */
int wikka_truncate_v2(const char *path, off_t offset) {
	save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );
	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( !cur_page || sub_page ) {
		save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
		return -ENOENT;
	}

	if ( cur_page->page_changed == NULL ) {
		if ( cur_page->page_current == NULL )
			cur_page->page_current = get_content( cur_page->tag, -1 );
		cur_page->page_changed = strdup( cur_page->page_current );
		save_log( LOG_ERR, "%s with %s : %d\n", __FUNCTION__, path, strlen(cur_page->page_changed) );
	}

	if ( cur_page->page_changed != NULL ) {
		if ( offset + 1 > (long int) strlen( cur_page->page_changed ) ) {
			save_log( LOG_ERR, "Truncate to extend page not implemented (offset : %llu / strlen : %d)", offset, strlen( cur_page->page_changed ) );
			return -ENOENT;
		}
		cur_page->page_changed[ offset ] = '\0';

		return 0;
	}

	save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
	// FIXME: is it the real error code to return ?
	return -ENOENT;
}

/** Remove a file: mark page as deleted */
int wikka_unlink_v2(const char *path) {
	save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );

	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	if ( !cur_page || sub_page ) {
		save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
		return -ENOENT;
	}
	
	cur_page->deleted = 1;
	return 0;
}

/** Create a file: at this time, allow only if page already exists with a size of 0 */
int wikka_create_v2( const char *path, UNUSED mode_t mode, struct fuse_file_info *fi ) {

	save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );

	char *sub_page = NULL;
	w_page *cur_page = path_to_page_v2( path, &sub_page );

	// FIXME: allow creating new page here
	if ( !cur_page || sub_page ) {
		if ( sub_page )
			return -EACCES;

		// No WebPage here, only create with a WikiName :
		if ( check_wikiname( path+1 )==0 ) {
			// Ok, insert it
			start_fd++;

			save_log( LOG_ERR, "creating %s with fd=%d\n", path, start_fd );
			w_page* new_page = calloc( sizeof(w_page), 1 );
			new_page->tag = strdup( path+1 );
			new_page->page_changed = calloc(1,1);
			fi->fh = start_fd;
			new_page->fh = fi->fh;
			new_page->next = Pages;
			Pages = new_page;
			return 0;
		}
		
		return -EACCES;
	}

	if ( ( cur_page->page_changed && strlen(cur_page->page_changed) == 0) || cur_page->deleted ) {
		start_fd++;

		save_log( LOG_ERR, "creating %s with fd=%d\n", path, start_fd );
		fi->fh = start_fd;
		cur_page->fh = fi->fh;
		cur_page->deleted = 0;
		if ( cur_page->page_changed == NULL )
			cur_page->page_changed = calloc(1,1);
		return 0;
	}

	return -EACCES;
}

int wikka_access_v2( const char *path, int mode ) {
	save_log( LOG_ERR, "%s with %s : mode : %o\n", __FUNCTION__, path, mode );
	// FIXME: allow everyone
	return 0;
}


static struct fuse_operations wikka_oper_v2 = {
	.getattr	= wikka_getattr_v2,
	.readdir	= wikka_readdir_v2,
	.open	= wikka_open_v2,
	.read	= wikka_read_v2,
	.write	= wikka_write_v2,
	.release = wikka_release_v2,
	.ftruncate = wikka_ftruncate_v2,
	.truncate = wikka_truncate_v2,
	.unlink = wikka_unlink_v2,
	.create = wikka_create_v2,
	.access = wikka_access_v2,
};

int start_fuse_v2() {
	int fuse_argc =  2;
	char **fuse_argv = malloc( sizeof(char*) * fuse_argc );
	fuse_argv[0] = "wikkafs";
	fuse_argv[1] = "-s";
	if ( fuse_debug ) {
		fuse_argv = realloc( fuse_argv, sizeof(char*) * (fuse_argc+1)  );
		fuse_argv[ fuse_argc++ ] = "-d";
	}
	if ( fuse_foreground ) {
		fuse_argv = realloc( fuse_argv, sizeof(char*) * (fuse_argc+1)  );
		fuse_argv[ fuse_argc++ ] = "-f";
	}

	// add mountpoint
	fuse_argv = realloc( fuse_argv, sizeof(char*) * (fuse_argc+1)  );
	fuse_argv[ fuse_argc++ ] = fuse_mountpoint;

	//if ( fuse_debug ) {
	int i=0;
	for( i=0; i<fuse_argc; i++ ) {
		printf("fuse_argv[ %d ] = '%s'\n", i, fuse_argv[ i ] );
	}
	//}


	int r = fuse_main( fuse_argc , fuse_argv, &wikka_oper_v2, NULL);
	//int r = fuse_main( fuse_argc , fuse_argv, &wikka_oper, NULL);
	return r;
}
