#ifndef __POUCH_H
#define __POUCH_H
// Standard libraries
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

// Libevent and Libcurl
#include <event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/dns.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <curl/curl.h>

// Defines
#define USE_SYS_FILE 0
#define GET "GET"
#define PUT "PUT"
#define POST "POST"
#define HEAD "HEAD"
#define COPY "COPY"
#define DELETE "DELETE"

// Structs
typedef struct _SockInfo SockInfo;
typedef struct _PouchPkt PouchPkt;
typedef struct _PouchReq PouchReq;
typedef struct _PouchMInfo PouchMInfo;
/*
	If a pr_proc_cb is set by the user, that function
	becomes responsible for pr_free()'ing the received
	PouchReq.
*/
typedef void (*pr_proc_cb)(PouchReq *, PouchMInfo *); // callback function for processing finished PouchReqs
struct _SockInfo {
	/*
		Used in the multi interface only.
		Holds information on a socket used
		for performing different easy_requests.
		Stolen from http://curl.haxx.se/libcurl/c/hiperfifo.html
	 */
	curl_socket_t sockfd;	// socket to be monitored
	struct event ev;		// event on the socket
	int ev_is_set;			// whether or not ev is set and being monitored
	int action;				// what action libcurl wants done
};
struct _PouchPkt {
	/*
	   Holds data to be sent to
	   or received from a CouchDB
	   server
	 */
	char *data;
	char *offset;
	size_t size;
};
struct _PouchReq {
	/*
	   A structure to be used
	   to send a request to 
	   a CouchDB server and 
	   save the response, as
	   well as any error codes.
	 */
	CURL *easy;			// CURL easy request
	CURLcode curlcode;	// CURL easy interface error code
	CURLM *multi;		// CURL multi object
	CURLMcode curlmcode; // CURLM multi interface error code
	char errorstr[CURL_ERROR_SIZE]; // holds an error description
	struct curl_slist *headers;	// Custom headers for uploading
	char *method;		// HTTP method
	char *url;			// Destination (e.g., "http://127.0.0.1:5984/test");
	char *usrpwd;		// Holds a user:password authentication string
	long httpresponse;	// holds the http response of a request
	PouchPkt req;		// holds data to be sent
	PouchPkt resp;		// holds response
};
struct _PouchMInfo {
	/*
		Used in the multi interface only.
		Holds values necessary for using
		libevent with libcurl; used for
		the multi interface. This is the
		struct that gets passed around
		to all of the libevent and libcurl
		callback functions.
	*/
	CURLM *multi;
	struct event timer_event;	// event necessary for libevent to work with libcurl
	struct event_base *base;	// libevent event_base for creating events
	struct evdns_base *dnsbase ;// libevent dns_base for creating connections
	int still_running;			// whether or not there are any running libcurl handles
	pr_proc_cb cb;		// USER DEFINED pointer to a callback function for processing finished PouchReqs
	int has_cb;			// ... tests for existence of callback function
	void *custom;				// USER DEFINED pointer to some data. 
};


// Miscellaneous helper functions
char *url_escape(CURL *curl, char *str);
char *combine(char **out, char *f, char *s, char *sep);

// PouchReq functions
PouchReq *pr_init(void);
PouchReq *pr_add_header(PouchReq *pr, char *h);
PouchReq *pr_add_usrpwd(PouchReq *pr, char *usrpwd, size_t length);
PouchReq *pr_add_param(PouchReq *pr, char *key, char *value);
PouchReq *pr_clear_params(PouchReq *pr);
PouchReq *pr_set_method(PouchReq *pr, char *method);
PouchReq *pr_set_url(PouchReq *pr, char *url);
PouchReq *pr_set_data(PouchReq *pr, char *str);
PouchReq *pr_set_prdata(PouchReq *pr, char *str, size_t len);
PouchReq *pr_set_bdata(PouchReq *pr, void *dat, size_t length);
PouchReq *pr_clear_data(PouchReq *pr);
PouchReq *pr_do(PouchReq *pr);
PouchReq *pr_domulti(PouchReq *pr, CURLM *multi);
void pr_free(PouchReq *pr);

// Database Wrapper Functions
PouchReq *get_all_dbs(PouchReq *p_req, char *server);
PouchReq *db_delete(PouchReq *p_req, char *server, char *db);
PouchReq *db_create(PouchReq *p_req, char *server, char *db);
PouchReq *db_get(PouchReq *p_req, char *server, char *db);
PouchReq *db_get_changes(PouchReq *pr, char *server, char *db);
PouchReq *db_get_revs_limit(PouchReq *pr, char *server, char *db);
PouchReq *db_set_revs_limit(PouchReq *pr, char *server, char *db, char *revs);
PouchReq *db_compact(PouchReq *pr, char *server, char *db);
	
// Document Wrapper Functions
PouchReq *doc_get(PouchReq *pr, char *server, char *db, char *id);
PouchReq *doc_get_rev(PouchReq *pr, char *server, char *db, char *id, char *rev);
PouchReq *doc_get_revs(PouchReq *pr, char *server, char *db, char *id);
PouchReq *doc_get_info(PouchReq *pr, char *server, char *db, char *id);
PouchReq *doc_create_id(PouchReq *pr, char *server, char *db, char *id, char *data);
PouchReq *doc_create(PouchReq *pr, char *server, char *db, char *data);
PouchReq *doc_prcreate(PouchReq *pr, char *server, char *db, char *data);
PouchReq *get_all_docs(PouchReq *pr, char *server, char *db);
PouchReq *get_all_docs_by_seq(PouchReq *pr, char *server, char *db);
PouchReq *doc_get_attachment(PouchReq *pr, char *server, char *db, char *id, char *name);
PouchReq *doc_copy(PouchReq *pr, char *server, char *db, char *id, char *newid, char *revision);
PouchReq *doc_delete(PouchReq *pr, char *server, char *db, char *id, char *rev);
PouchReq *doc_add_attachment(PouchReq *pr, char *server, char *db, char *doc, char *filename);

// Generic libevent callback functions
size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data);
size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data);

// libevent/libcurl multi interface helpers and callbacks
void debug_mcode(const char *desc, CURLMcode code);
void check_multi_info(PouchMInfo *pmi /*, function pointer process_func*/);
int multi_timer_cb(CURLM *multi, long timeout_ms, void *data);
void event_cb(int fd, short kind, void *userp);
void timer_cb(int fd, short kind, void *userp);
void setsock(SockInfo *fdp, curl_socket_t s, int action, PouchMInfo *pmi);
int sock_cb(CURL *e, curl_socket_t s, int action, void *cbp, void *sockp);
PouchMInfo *pr_mk_pmi(struct event_base *base, struct evdns_base *dns_base, pr_proc_cb callback, void *custom);
void pmi_multi_cleanup(PouchMInfo *pmi);
void pr_del_pmi(PouchMInfo *pmi);

#endif
