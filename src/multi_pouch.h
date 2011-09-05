#ifndef __MULTI_POUCH_H
#define __MULTI_POUCH_H

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

// Pouch helpers
#include "pouch.h"

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
