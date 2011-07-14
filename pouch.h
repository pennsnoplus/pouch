#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>

#define USE_SYS_FILE 0

#define GET "GET"
#define PUT "PUT"
#define POST "POST"
#define HEAD "HEAD"
#define COPY "COPY"
#define DELETE "DELETE"

typedef struct pouch_pkt {
	/*
	   Holds data to be sent to
	   or received from a CouchDB
	   server
	 */
	char *data;
	char *offset;
	size_t size;
} pouch_pkt;

typedef struct pouch_request {
	/*
	   A structure to be used
	   to send a request to 
	   a CouchDB server and 
	   save the response, as
	   well as any error codes.
	 */
	struct curl_slist *headers;	// Custom headers for uploading
	char *method;		// HTTP method
	char *url;			// Destination (e.g., "http://127.0.0.1:5984/test");
	char *usrpwd;		// Holds a user:password authentication string
	CURLcode curlcode;	// CURL request status holder
	long httpresponse;	// holds the http response of a request
	pouch_pkt req;		// holds data to be sent
	pouch_pkt resp;		// holds response
	// int error;		// holds an error code
} pouch_request;

// Utility Functions
char *url_escape(CURL *curl, char *str);
char *combine(char **out, char *f, char *s, char *sep);

// Pouch Request Functions

pouch_request *pr_init(void);
pouch_request *pr_set_url(pouch_request *pr, char *url);
pouch_request *pr_set_data(pouch_request *pr, char *str);
pouch_request *pr_set_bdata(pouch_request *pr, void *dat, size_t length);
pouch_request *pr_set_method(pouch_request *pr, char *method);
pouch_request *pr_add_header(pouch_request *pr, char *h);
pouch_request *pr_add_param(pouch_request *pr, char *key, char *value);
pouch_request *pr_add_usrpwd(pouch_request *pr, char *usrpwd, size_t length);
pouch_request *pr_clear_data(pouch_request *pr);
pouch_request *pr_clear_params(pouch_request *pr);
void pr_free(pouch_request *pr);

// Request Helper Functions
size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data);
size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data);
pouch_request *pr_do(pouch_request *pr);

// Database Wrapper Functions
pouch_request *get_all_dbs(pouch_request *p_req, char *server);
pouch_request *db_delete(pouch_request *p_req, char *server, char *db);
pouch_request *db_create(pouch_request *p_req, char *server, char *db);
pouch_request *db_get(pouch_request *p_req, char *server, char *db);
pouch_request *db_get_changes(pouch_request *pr, char *server, char *db);
pouch_request *db_get_revs_limit(pouch_request *pr, char *server, char *db);
pouch_request *db_set_revs_limit(pouch_request *pr, char *server, char *db, char *revs);
pouch_request *db_compact(pouch_request *pr, char *server, char *db);
	
// Document Wrapper Functions
pouch_request *doc_get(pouch_request *pr, char *server, char *db, char *id);
pouch_request *doc_get_rev(pouch_request *pr, char *server, char *db, char *id, char *rev);
pouch_request *doc_get_revs(pouch_request *pr, char *server, char *db, char *id);
pouch_request *doc_get_info(pouch_request *pr, char *server, char *db, char *id);
pouch_request *doc_create_id(pouch_request *pr, char *server, char *db, char *id, char *data);
pouch_request *doc_create(pouch_request *pr, char *server, char *db, char *data);
pouch_request *get_all_docs(pouch_request *pr, char *server, char *db);
pouch_request *get_all_docs_by_seq(pouch_request *pr, char *server, char *db);
pouch_request *doc_get_attachment(pouch_request *pr, char *server, char *db, char *id, char *name);
pouch_request *doc_copy(pouch_request *pr, char *server, char *db, char *id, char *newid, char *revision);
pouch_request *doc_delete(pouch_request *pr, char *server, char *db, char *id, char *rev);
char *doc_get_cur_rev(pouch_request *pr, char *server, char *db, char *id);
pouch_request *doc_add_attachment(pouch_request *pr, char *server, char *db, char *doc, char *filename);
