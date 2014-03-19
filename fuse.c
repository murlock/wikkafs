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

int fuse_debug = 0;
int fuse_foreground = 0;

char *fuse_mountpoint = NULL;

static int start_fd = 1;

w_page* path_to_page( const char *path, char **sub ) {
    *sub = NULL;
    if ( strncmp( path, "/PageIndex/", strlen("/PageIndex/") ) == 0 ) {
        // we have to check if page really exist and if we have others info
        char *name = strdup(path+strlen("/PageIndex/"));
        char *part_sub = strchr( name, '/' );
        if (part_sub) {
            *part_sub='\0'; 
            part_sub++;
        }

        // we check if asked page is present in Page
        w_page *ptr = search( name );
        if ( !ptr ) {
            free( name );
            return NULL;
        }
        if ( part_sub ) {
            *sub = strdup( part_sub );
        }
        free( name );
        return ptr;
    }
    return NULL;
}


static int wikka_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );
    w_page *cur_page = NULL;
    char *sub_page = NULL;
    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if ( strcmp( path, "/PageIndex" ) == 0 ) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if ( (cur_page = path_to_page( path, &sub_page )) ) {
        if ( sub_page == NULL ) {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            stbuf->st_mtime = cur_page->created;
        } else if ( strcmp(sub_page, "content" )== 0 ) {
            stbuf->st_mode = S_IFREG | 0755;
            stbuf->st_mtime = cur_page->created;
            stbuf->st_size = cur_page->size;
            stbuf->st_nlink = 1;
        } else if ( strcmp(sub_page, "revisions" ) == 0 ) {
            if ( cur_page->revisions == NULL )
                get_revisions( cur_page );
            if ( cur_page->nb_revisions > 0 ) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                stbuf->st_mtime = cur_page->created;
            }
        } else {
            save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
            res = -ENOENT;
        }
        if ( sub_page )
            free( sub_page );

    } else {
        save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
        res = -ENOENT;
    }
    return res;
}


static int wikka_readdir( const char *path, 
        void *buf, 
        fuse_fill_dir_t filler, 
        UNUSED off_t offset, 
        UNUSED struct fuse_file_info *fi ) {

    save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );
    if ( strcmp( path, "/" ) == 0 ) {
        filler( buf, "." , NULL, 0 );
        filler( buf, ".." , NULL, 0 );

        // we must show /Homepage as symbolink link to PageIndex/HomePage
        filler( buf, "PageIndex" , NULL, 0 );
        return 0;
    }
    if ( strcmp( path, "/PageIndex" ) == 0 ) {
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

            filler( buf,  cur->tag ,stbuf, 0 );
            cur=cur->next;
        }
        return 0;
    }
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );
    if ( cur_page && sub_page == NULL ) {
        filler( buf, "." , NULL, 0 );
        filler( buf, ".." , NULL, 0 );
        filler( buf, "content" , NULL, 0 );
        if ( cur_page->revisions == NULL )
            get_revisions( cur_page );
        if ( cur_page->nb_revisions > 0 )
            filler( buf, "revisions" , NULL, 0 );
        return 0;
    }

    if ( cur_page && strcmp( sub_page, "revisions" )==0 ) {
        filler( buf, "." , NULL, 0 );
        filler( buf, ".." , NULL, 0 );
        int i=0;
        char tmp[64];
        while ( i < cur_page->nb_revisions ) {
            snprintf( tmp, sizeof(tmp), "r%d", cur_page->revisions[i] );
            filler( buf, tmp, NULL, 0 );
            i++;
        }
        if ( sub_page )
            free( sub_page );
        return 0;
    }

    if ( sub_page ) {
        save_log( LOG_ERR, "=> sub_page = '%s'", sub_page );
        free( sub_page );
    }

    save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
    return -ENOENT;
}


static int wikka_open(const char *path, struct fuse_file_info *fi)
{
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );

    if ( cur_page && strcmp( sub_page, "content" ) ==  0 )  {
        fi->fh = start_fd++;
        save_log( LOG_ERR, "%s with %s : (fh set to %llu)\n", __FUNCTION__, path, fi->fh );
        free(sub_page);
        return 0;
    }
    if (sub_page)
        free(sub_page);
    save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
    return -ENOENT;
}



static int wikka_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {

    save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );

    if ( !cur_page || (cur_page && strcmp( sub_page, "content" )  ) ) { 
        save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
        if ( sub_page ) {
            free( sub_page );
        }
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

    if ( sub_page ) {
        free( sub_page );
    }
    return size;
}

int wikka_release(const char *path, struct fuse_file_info *fi) {
    save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );

    if ( !cur_page || (cur_page && strcmp( sub_page, "content" )  ) ) { 
        save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
        if ( sub_page )
            free( sub_page );
        return -ENOENT;
    }

    if ( cur_page->fh == (long int) fi->fh && cur_page->page_changed != NULL ) {

        // we discard changes since we disallow empty Wiki Page
        if ( strlen( cur_page->page_changed ) != 0 ) {
            save_log( LOG_ERR, "Save %s\n", path );
            // we need to 'commit' the updated page
            update_content( cur_page->tag, cur_page->page_changed );
        }

        free( cur_page->page_changed );
        cur_page->page_changed = NULL;
        cur_page->fh = 0;
    }

    if ( cur_page->page_current )
        free( cur_page->page_current );
    cur_page->page_current = NULL;
    free( sub_page );
    return 0;
}

int wikka_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );

    if ( !cur_page || (cur_page && strcmp( sub_page, "content" )  ) ) { 
        save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
        if ( sub_page )
            free( sub_page );
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

        if ( sub_page )
            free( sub_page );
        return size;
    }
    if ( sub_page )
        free( sub_page );

    // FIXME: is it the real error code to return ?
    return -ENOENT;
}


int wikka_ftruncate( const char *path, off_t offset, struct fuse_file_info *fi ) {
    save_log( LOG_ERR, "%s with %s (fh=%llu)\n", __FUNCTION__, path, fi->fh );
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );

    if ( !cur_page || (cur_page && strcmp( sub_page, "content" )  ) ) { 
        save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
        if ( sub_page )
            free( sub_page );
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

        if ( sub_page )
            free( sub_page );

        return 0;
    }

    if ( sub_page )
        free( sub_page );

    // FIXME: is it the real error code to return ?
    return -ENOENT;
}

/** Truncate a file */
int wikka_truncate(const char *path, off_t offset) {
    save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );
    char *sub_page = NULL;
    w_page *cur_page = path_to_page( path, &sub_page );

    if ( !cur_page || (cur_page && strcmp( sub_page, "content" )  ) ) { 
        save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
        if ( sub_page )
            free( sub_page );
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

        if ( sub_page )
            free( sub_page );
        return 0;
    }

    if ( sub_page )
        free( sub_page );

    save_log( LOG_ERR, "Failed in %s with %s\n", __FUNCTION__, path );
    // FIXME: is it the real error code to return ?
    return -ENOENT;
}

/** Remove a file: fallback to truncate to 0 */
int wikka_unlink(const char *path) {
    save_log( LOG_ERR, "%s with %s\n", __FUNCTION__, path );
    return wikka_truncate( path, 0 );
}


int wikka_create(const char *path, UNUSED mode_t mode, UNUSED struct fuse_file_info *fi ) {
    save_log( LOG_ERR, "unsupported %s with %s\n", __FUNCTION__, path );
    return -ENOENT;
}


int wikka_mknod(const char *path, UNUSED mode_t mode, UNUSED dev_t dev) {
    save_log( LOG_ERR, "unsupported %s with %s\n", __FUNCTION__, path );
    return -ENOENT;
}

static struct fuse_operations wikka_oper = {
    .getattr    = wikka_getattr,
    .readdir    = wikka_readdir,
    .open   = wikka_open,
    .read   = wikka_read,
    .write  = wikka_write,
    .release = wikka_release,
    .ftruncate = wikka_ftruncate,
    .truncate = wikka_truncate,
    .unlink = wikka_unlink,
    .mknod = wikka_mknod,
    //.create = wikka_create,

};

int start_fuse() {
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


    int r = fuse_main( fuse_argc , fuse_argv, &wikka_oper, NULL);
    //int r = fuse_main( fuse_argc , fuse_argv, &wikka_oper, NULL);

    free( fuse_argv );
    return r;
}
