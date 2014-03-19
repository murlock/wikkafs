/*
 *
 *
 */


/* Structure that represent a visible page (with last_revision = 'Y') */
typedef struct _page {
	struct _page *next;
	char *tag;
	char *owner;
	int size;
	int revision;
	char *page_current;

	int *revisions;
	int nb_revisions;

	int fh;
	char *page_changed;
	int deleted;

	time_t created;
} w_page;

/* usefull functions to use page */
w_page* search( const char *name );
extern w_page *Pages;
	

/* SQL */
int sql_init();
int sql_close();
w_page* fill();
char *get_content( char *tag, int rev );
int get_revisions( w_page *page );
int update_content( const char *targ, const char *body );

/* Logger */
void save_log( int level, const char *format, ...);
int check_wikiname( const char *name );


/* MySQL config */
extern char *bdd_host;
extern char *bdd_user;
extern char *bdd_pass;
extern char *bdd_name;
extern int   bdd_port;
extern int	 bdd_ssl;

/* Wikka configuration */
extern char *wikka_prefix;
extern char *wikka_user;

/* FUSE configuration */
extern int fuse_debug;
extern int fuse_foreground;
extern char *fuse_mountpoint;

int start_fuse();
int start_fuse_v2();


