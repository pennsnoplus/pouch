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

#include "multi_pouch.h"

// PouchReq functions
PouchReq *pr_domulti(PouchReq *pr, CURLM *multi){
	// empty the response buffer
	if (pr->resp.data){
		free(pr->resp.data);
	}
	pr->resp.data = NULL;
	pr->resp.size = 0;

	// initialize the CURL object
	if (pr->easy){ // get rid of the old one?
		printf("EXISTING EASY\n");
		curl_multi_remove_handle(multi, pr->easy);
		curl_easy_cleanup(pr->easy);
	}
	pr->easy = curl_easy_init();
	pr->multi = multi;
	
	// setup the CURL object/request
	curl_easy_setopt(pr->easy, CURLOPT_USERAGENT, "pouch/0.1");				// add user-agent
	curl_easy_setopt(pr->easy, CURLOPT_URL, pr->url);						// where to send this request
	curl_easy_setopt(pr->easy, CURLOPT_CONNECTTIMEOUT, 2);					// Timeouts
	curl_easy_setopt(pr->easy, CURLOPT_TIMEOUT, 2);
	curl_easy_setopt(pr->easy, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(pr->easy, CURLOPT_WRITEFUNCTION, recv_data_callback);	// where to store the response
	curl_easy_setopt(pr->easy, CURLOPT_WRITEDATA, (void *)pr);
	curl_easy_setopt(pr->easy, CURLOPT_PRIVATE, (void *)pr);				// associate this request with the PouchReq holding it
	curl_easy_setopt(pr->easy, CURLOPT_NOPROGRESS, 1L);						// Don't use a progress function to watch this request
	curl_easy_setopt(pr->easy, CURLOPT_ERRORBUFFER, pr->errorstr);			// Store multi error descriptions in pr->errorstr
	
	if(pr->usrpwd){	// if there's a valid auth string, use it
		curl_easy_setopt(pr->easy, CURLOPT_USERPWD, pr->usrpwd);
	}

	if (pr->req.data && pr->req.size > 0){ // check for data upload
		//printf("--> %s\n", pr->req.data);
		// let CURL know what data to send
		curl_easy_setopt(pr->easy, CURLOPT_READFUNCTION, send_data_callback);
		curl_easy_setopt(pr->easy, CURLOPT_READDATA, (void *)pr);
	}

	if (!strncmp(pr->method, PUT, 3)){ // PUT-specific option
		curl_easy_setopt(pr->easy, CURLOPT_UPLOAD, 1);
		// Note: Content-Type: application/json is automatically assumed
	}
	else if (!strncmp(pr->method, POST, 4)){ // POST-specific options
		curl_easy_setopt(pr->easy, CURLOPT_POST, 1);
		pr_add_header(pr, "Content-Type: application/json");
	}

	if (!strncmp(pr->method, HEAD, 4)){ // HEAD-specific options
		curl_easy_setopt(pr->easy, CURLOPT_NOBODY, 1); // no body to this request, just a head
		curl_easy_setopt(pr->easy, CURLOPT_HEADER, 1); // includes header in body output (for debugging)
	} // THIS FIXED HEAD REQUESTS
	else {
		curl_easy_setopt(pr->easy, CURLOPT_CUSTOMREQUEST, pr->method);
	} 

	// add the custom headers
	pr_add_header(pr, "Transfer-Encoding: chunked");
	curl_easy_setopt(pr->easy, CURLOPT_HTTPHEADER, pr->headers);

	// start the request by adding it to the multi handle
	pr->curlmcode = curl_multi_add_handle(pr->multi, pr->easy);
	//printf("pr->curlmcode = %d\n", pr->curlmcode);
	debug_mcode("pr_domulti: ", pr->curlmcode);
	
	return pr;
	/*
	if (pr->headers){
		curl_slist_free_all(pr->headers);	// free headers
		pr->headers = NULL;
	}
	if (!pr->curlcode){
		pr->curlcode = curl_easy_getinfo(pr->easy, CURLINFO_RESPONSE_CODE, &pr->httpresponse);
		if (pr->curlcode != CURLE_OK)
			pr->httpresponse = 500; //  Internal Server Error (no specific error)
	}
	curl_easy_cleanup(curl);		// clean up the curl object

	// Print the response
	//printf("Received %d bytes, status = %d\n",
	//		(int)pr->resp.size, pr->curlcode);
	//printf("--> %s\n", pr->resp.data);
	*/
}


// libevent/libcurl multi interface helpers and callbacks
void debug_mcode(const char *desc, CURLMcode code){
	if ((code != CURLM_OK) && (code != CURLM_CALL_MULTI_PERFORM)){
		const char *s;
		switch (code){
			//case CURLM_CALL_MULTI_PERFORM:	s="CURLM_CALL_MULTI_PERFORM";break; //ignore
			case CURLM_BAD_HANDLE:			s="CURLM_BAD_HANDLE";break;
			case CURLM_BAD_EASY_HANDLE:		s="CURLM_BAD_EASY_HANDLE";break;
			case CURLM_OUT_OF_MEMORY:		s="CURLM_OUT_OF_MEMORY";break;
			case CURLM_INTERNAL_ERROR:		s="CURLM_INTERNAL_ERROR";break;
			case CURLM_UNKNOWN_OPTION:		s="CURLM_UNKNOWN_OPTION";break;
			case CURLM_LAST:				s="CURLM_LAST";break;
			case CURLM_BAD_SOCKET:			s="CURLM_BAD_SOCKET";break;
			default:						s="CURLM_unknown";
		}
		fprintf(stderr, "ERROR: %s returns %s\n", desc, s);
	}
}
void check_multi_info(PouchMInfo *pmi /*, function pointer process_func*/){
	CURLMsg *msg;
	CURL *easy;
	CURLcode res;
	
	int msgs_left;
	PouchReq *pr;

	while ((msg = curl_multi_info_read(pmi->multi, &msgs_left))){
		if(msg->msg == CURLMSG_DONE){ // if this action is done
			// unpack the message
			easy = msg->easy_handle;
			res = msg->data.result;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &pr);
			//printf("Finished request (easy=%p, url=%s)\n", easy, pr->url);
			// process the result
			if(pmi->has_cb){
				pmi->cb(pr, pmi);
			}
			else {
				pr_free(pr);
			}
		}
	}
}
int multi_timer_cb(CURLM *multi, long timeout_ms, void *data){
	/*
		Update the event timer after curl_multi
		library calls.
	*/
	PouchMInfo *pmi = (PouchMInfo *)data;
	struct timeval timeout;
	timeout.tv_sec = timeout_ms/1000;
	timeout.tv_usec = (timeout_ms%1000)*1000;
	//fprintf(stderr, "multi_timer_cb: Setting timeout to %ld ms\n", timeout_ms);
	if (evtimer_pending(&pmi->timer_event, NULL)){
		evtimer_del(&pmi->timer_event);
	}
	evtimer_add(&pmi->timer_event, &timeout);
	return 0;
}
void event_cb(int fd, short kind, void *userp){
	/*
		Called by libevent when there is any type
		of action on a socket being watched.
	 */
	PouchMInfo *pmi = (PouchMInfo *)userp;
	CURLMcode rc; // result from curl_multi_socket_action
	int action = 
		(kind & EV_READ ? CURL_CSELECT_IN : 0) |
		(kind & EV_WRITE ? CURL_CSELECT_OUT : 0);
	rc = curl_multi_socket_action(pmi->multi, fd, action, &pmi->still_running);
	
	debug_mcode("event_cb: curl_multi_socket_action", rc);
	
	check_multi_info(pmi);
	if (pmi->still_running <= 0){ // last transfer is done
		if (evtimer_pending(&pmi->timer_event, NULL)){
			evtimer_del(&pmi->timer_event); // get rid of the libevent timer
		}
	}
}
void timer_cb(int fd, short kind, void *userp){
	/*
		Called by libevent when the global timeout
		that is used to take action on different easy_handles
		expires.
	*/
	PouchMInfo *pmi = (PouchMInfo *)userp;
	CURLMcode rc; // result from curl_multi_socket_action
	rc = curl_multi_socket_action(pmi->multi, CURL_SOCKET_TIMEOUT, 0, &pmi->still_running);
	debug_mcode("timer_cb: curl_multi_socket_action", rc);
	check_multi_info(pmi);
}
void setsock(SockInfo *fdp, curl_socket_t s, int action, PouchMInfo *pmi){
	/*
		Sets up a SockInfo structure and starts libevent
		monitoring on a socket.
	*/
	int kind =
		(action&CURL_POLL_IN ? EV_READ:0)|
		(action&CURL_POLL_OUT ? EV_WRITE:0)|
		EV_PERSIST; // always want persist
	fdp->action = action;
	fdp->sockfd = s;
	if (fdp->ev_is_set){
		event_del(&fdp->ev);
		fdp->ev_is_set = 0;
	}
	event_set(&fdp->ev, fdp->sockfd, kind, event_cb, pmi);
	event_base_set(pmi->base, &fdp->ev); // set the event to use the global event base
	fdp->ev_is_set = 1; // mark the event as set
	event_add(&fdp->ev, NULL); // add the event with no timeout (NULL)
}
int sock_cb(CURL *e, curl_socket_t s, int action, void *cbp, void *sockp){
	/*
		The CURLMOPT_SOCKETFUNCTION. This is what tells libevent to start
		or stop watching for events on different sockets.

		e       = easy handle that the callback happened on,
		s       = actual socket that is involved,
		action  = what to check for / do (?) (nothing, IN, OUT, INOUT, REMOVE)
		cbp     = private data set by curl_multi_setop(CURLMOPT_SOCKETDATA)
		sockp	= private data set by curl_multi_assign(multi, socket, sockp)
	*/
	PouchMInfo *pmi = (PouchMInfo *)cbp;
	SockInfo *fdp = (SockInfo *)sockp;
	if (action == CURL_POLL_REMOVE){
		// stop watching this socket for events
		if(fdp){
			if(fdp->ev_is_set){
				event_del(&fdp->ev);
			}
			free(fdp);
		}
	}
	else {
		if (!fdp){
			// start watching this socket for events
			SockInfo *fdp = calloc(1, sizeof(SockInfo));
			setsock(fdp, s, action, pmi);
			curl_multi_assign(pmi->multi, s, fdp);
		}
		else {
			// reset the sock with the new type of action
			setsock(fdp, s, action, pmi);
		}
	}
	return 0;
}
PouchMInfo *pr_mk_pmi(struct event_base *base, struct evdns_base *dns_base, pr_proc_cb callback, void *custom){
	/*
		Initialize a PouchMInfo structure for use with libevent.
		This creates and initializes the CURLM handle pointer,
		as well as the libevent timer_event which is used to deal
		with the multi interface, but takes in a pointer to a 
		libevent event_base for use as a sort of "global" base throughout
		all of the callbacks, so that the user can define their own base.
	*/
	PouchMInfo *pmi = (PouchMInfo *)malloc(sizeof(PouchMInfo));
	if(!pmi){
		return NULL;
	}
	memset(pmi, 0, sizeof(*pmi));
	pmi->base = base;
	pmi->dnsbase = dns_base;
	pmi->still_running = 0;
	pmi->has_cb = 0;
	if (callback){
		pmi->cb = callback; // set the callback function
		pmi->has_cb = 1;
	}
	pmi->custom = custom;
	pmi->multi = curl_multi_init();
	evtimer_set(&pmi->timer_event, timer_cb, (void *)pmi);
	event_base_set(pmi->base, &pmi->timer_event);
	// setup the generic multi interface options we want
	curl_multi_setopt(pmi->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
	curl_multi_setopt(pmi->multi, CURLMOPT_SOCKETDATA, pmi);
	curl_multi_setopt(pmi->multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
	curl_multi_setopt(pmi->multi, CURLMOPT_TIMERDATA, pmi);
	//curl_multi_setopt(pmi->multi, CURLMOPT_MAXCONNECTS, 20); // arbitrary
	return pmi;
}
/*
void pmi_multi_cleanup(CURLM *multi){
	printf("inside pmi_multi_cleanup\n");
	printf("multi = %p\n", multi);
	CURLMsg *msg;
	CURL *easy;
	PouchReq *pr;

	// messages left <- num_easies
	int messages_left;
	while ((msg = curl_multi_info_read(multi, &messages_left))){
		easy = msg->easy_handle;
		printf("got easy = %p\n", easy);
		curl_easy_getinfo(easy, CURLINFO_PRIVATE, &pr);
		printf("pr  easy = %p\n", easy);
		pr_free(pr);
	}
}
*/
void pmi_multi_cleanup(PouchMInfo *pmi){
	printf("INSIDE pmi_multi_cleanup with pmi = %p\n", pmi);
	CURLMsg *msg;
	CURL *easy;
	CURLcode res;
	
	int msgs_left;
	PouchReq *pr;
	fprintf(stderr, "remaining easy_handles: %d\n", pmi->still_running);
	while ((msg = curl_multi_info_read(pmi->multi, &msgs_left))){
		printf("msgs_left = %d\n", msgs_left);
		printf("woo unpacked a message\n");
		if(msg){ // if this action is done
			printf("woo message existed\n");
			// unpack the message
			easy = msg->easy_handle;
			res = msg->data.result;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &pr);
			pr_free(pr);
			printf("FREED !\n");
		}
		else {
			printf("boo message did NOT exist\n");
		}
	}
	//printf("msg->data.result = %d\n", msg->data.result);
	printf("msgs_left = %d\n", msgs_left);
}
void pr_del_pmi(PouchMInfo *pmi){
	/*
		Cleans up and deletes a PouchMInfo struct.
		It gets rid of the timer event, frees the event base
		(don't do this manually after calling pr_del_pmi!)
		and cleans up the CURLM handle. Afterwards, it frees
		the object. Don't try to free it again.
	*/
	if(pmi){
		printf("pmi %p exists!\n", pmi);
		event_del(&pmi->timer_event); // TODO: figure out how to check if this is valid
		if(pmi->multi){
			printf("pmi %p multi %p exists!\n", pmi, pmi->multi);
			//pmi_multi_cleanup(pmi);
			curl_multi_cleanup(pmi->multi);
		}
		if(pmi->dnsbase){
			evdns_base_free(pmi->dnsbase, 0);
		} if (pmi->base){
			event_base_free(pmi->base);
		}
		free(pmi);
	}
}
