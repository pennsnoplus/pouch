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

#include "pouch.h"

// Miscellaneous helper functions
char *url_escape(CURL *curl, char *str){
	/*
	   URL escapes a string. Use this to 
	   escape database names.
	 */
	return curl_easy_escape(curl, str, strlen(str));
}
char *combine(char **out, char *f, char *s, char *sep){
	/*
	   Appends the strings f, sep, and s, in that order,
	   and copies the result to *out. If a separator is
	   unnecessary, call the function with sep=NULL. *out
	   must have been malloced() or initiated as NULL.
	 */
	size_t length = 0;
	length += strlen(f);
	length += strlen(s);
	if(sep)
		length += strlen(sep);
	length++; // must have room for terminating \0
	char buf[length];
	if(sep)
		sprintf(buf, "%s%s%s", f, sep, s);
	else
		sprintf(buf, "%s%s", f, s);
	buf[length-1] = '\0'; // null terminate
	if(*out){
		free(*out);
	}
	*out = (char *)malloc(length);
	memcpy(*out, buf, length);
	return *out;
}
char *doc_get_cur_rev(PouchReq * pr, char *server, char *db, char *id){
	/*
	   Stores the current revision of the document in pr->resp.data.
	   If you want to do anything with that revision string, make sure
	   to copy it to another place in memory before reusing the request.
	 */
	pr = doc_get_info(pr, server, db, id);
	pr_do(pr);
	// at this point, pr->resp.data has all of the header stuff.
	char *etag_begin = strchr(pr->resp.data, '\"');
	char *etag_end = strrchr(pr->resp.data, '\"');

	size_t length = (size_t) (etag_end - etag_begin) - 1;

	char *buf = (char *)malloc(length + 1);
	memset(buf, 0, length + 1);
	strncpy(buf, etag_begin + 1, length);

	if (pr->resp.data){
		free(pr->resp.data);
	}
	pr->resp.data = (char *)malloc(length + 1);
	memset(pr->resp.data, 0, length + 1);
	strncpy(pr->resp.data, buf, length + 1);

	return buf;
}

// PouchReq functions
PouchReq *pr_init(void){
	/*
	   Initializes a new PouchReq
	   object.
	 */
	PouchReq *pr = calloc(1, sizeof(PouchReq));

	// initializes the request buffer
	pr->req.offset = pr->req.data = NULL;
	pr->req.size = 0;

	// initializes the response buffer
	pr->resp.offset = pr->resp.data = NULL;
	pr->resp.size = 0;

	return pr;
}
PouchReq *pr_add_header(PouchReq *pr, char *h){
	/*
	   Add a custom header to a request.
	 */
	pr->headers = curl_slist_append(pr->headers, h);
	return pr;
}
PouchReq *pr_add_usrpwd(PouchReq *pr, char *usrpwd, size_t length){
	if (pr->usrpwd){
		free(pr->usrpwd);
	}
	pr->usrpwd = (char *)malloc(length);
	memcpy(pr->usrpwd, usrpwd, length);
	return pr;
}
PouchReq *pr_add_param(PouchReq *pr, char *key, char *value){
	/*
	   Adds a parameter to a request's URL string,
	   regardless of whether or not other parameters already exist.
	 */
	pr->url = (char *)realloc(pr->url, // 3: new ? or &, new =, new '\0'
			strlen(pr->url) + 3 + sizeof(char)*(strlen(key)+strlen(value)));
	if (strchr(pr->url, '?') == NULL){
		strcat(pr->url, "?");
	}
	else{
		strcat(pr->url, "&");
	}
	strcat(pr->url, key);
	strcat(pr->url, "=");
	strcat(pr->url, value);
	strcat(pr->url, "\0");
	return pr;
}
PouchReq *pr_clear_params(PouchReq *pr){
	/*
	   Removes all parameters from a request's URL string,
	   if they exist. Otherwise, the URL string is left alone.
	 */
	char *div;
	if ( (div = strchr(pr->url, '?')) != NULL){ // if there are any params
		char *temp = &pr->url[strlen(pr->url)]; // end of the string
		while (*temp != '?'){
			*temp = '\0'; // wipe out the old character
			temp--;	// move back another character
		}
		*temp = '\0'; // get rid of the ?
	}
	return pr;
}
PouchReq *pr_set_method(PouchReq *pr, char *method){
	/*
	   Sets the HTTP method of
	   a specific request.
	 */
	size_t length = strlen(method)+1; // include '\0' terminator
	if (pr->method)
		free(pr->method);
	pr->method = (char *)malloc(length); // allocate space
	memcpy(pr->method, method, length);	 // copy the method
	return pr;
}
PouchReq *pr_set_url(PouchReq *pr, char *url){
	/*
	   Sets the target URL of
	   a CouchDB request.
	 */
	size_t length = strlen(url)+1; // include '\0' terminator
	if (pr->url)	// if there is an older url, get rid of it
		free(pr->url);
	pr->url = (char *)malloc(length); // allocate space
	memcpy(pr->url, url, length);	  // copy the new url

	return pr;
}
PouchReq *pr_set_data(PouchReq *pr, char *str){
	/*
	   Sets the data that a request
	   sends. If the request does not
	   need to send data, do NOT call
	   this function with an empty string,
	   just refrain from calling the function.
	 */
	size_t length = strlen(str);
	if (pr->req.data){	// free older data
		free(pr->req.data);
	}
	// TODO: use strdup?
	pr->req.data = (char *)malloc(length+1);	// allocate space, include '\0'
	memset(pr->req.data, '\0', length+1);		// write nulls to the new space
	memcpy(pr->req.data, str, length);	// copy over the data

	// Because of the way CURL sends data,
	// before sending the PouchPkt's
	// offset must point to the same address
	// in memory as the data pointer does.
	pr->req.offset = pr->req.data;
	pr->req.size = length; // do not send the last '\0' - JSON is not null terminated
	return pr;
}
PouchReq *pr_set_prdata(PouchReq *pr, char *str, size_t len){
	if(pr->req.data){
		free(pr->req.data);
	}
	pr->req.data = str;
	pr->req.offset = pr->req.data;
	pr->req.size = len;
	return pr;
}
PouchReq *pr_set_bdata(PouchReq *pr, void *dat, size_t length){
	if (pr->req.data){
		free(pr->req.data);
	}
	pr->req.data = (char *)malloc(length);
	memcpy(pr->req.data, dat, length);
	pr->req.offset = pr->req.data;
	pr->req.size = length;
	return pr;
}
PouchReq *pr_clear_data(PouchReq *pr){
	/*
	   Removes all data from a request's
	   data buffer, if it exists.
	 */
	if (pr->req.data){
		free(pr->req.data);
		pr->req.data = NULL;
	}
	pr->req.size = 0;
	return pr;
}
PouchReq *pr_do(PouchReq * pr){
	CURL *curl;		// CURL object to make the requests
	//pr->headers= NULL;    // Custom headers for uploading

	// empty the response buffer
	if (pr->resp.data){
		free(pr->resp.data);
	}
	pr->resp.data = NULL;
	pr->resp.size = 0;

	// initialize the CURL object
	curl = curl_easy_init();
	if (curl){
		// Print the request
		//printf("%s : %s\n", pr->method, pr->url);

		// setup the CURL object/request
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "pouch/0.1");	// add user-agent
		curl_easy_setopt(curl, CURLOPT_URL, pr->url);	// where to send this request
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2);	// maximum amount of time to create connection
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);	// maximum amount of time to send data = 1 minute
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // TODO: why? multithreading?
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);	// where to store the response
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pr);
		if (pr->usrpwd){	// if there's a valid auth string, use it
			curl_easy_setopt(curl, CURLOPT_USERPWD, pr->usrpwd);
		}

		if (pr->req.data && pr->req.size > 0){	// check for data upload
			//printf("--> %s\n", pr->req.data);
			// let CURL know what data to send
			curl_easy_setopt(curl, CURLOPT_READFUNCTION,
					send_data_callback);
			curl_easy_setopt(curl, CURLOPT_READDATA, (void *)pr);
		}

		if (!strncmp(pr->method, PUT, 3)){	// PUT-specific option
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			// Note: Content-Type: application/json is automatically assumed
		} else if (!strncmp(pr->method, POST, 4)){	// POST-specific options
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			pr_add_header(pr, "Content-Type: application/json");
		}

		if (!strncmp(pr->method, HEAD, 4)){	// HEAD-specific options
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
			curl_easy_setopt(curl, CURLOPT_HEADER, 1);
		} else {
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,
					pr->method);
		}		// THIS FIXED HEAD REQUESTS

		// add the custom headers
		pr_add_header(pr, "Transfer-Encoding: chunked");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pr->headers);

		// make the request and store the response
		pr->curlcode = curl_easy_perform(curl);
	} else {
		// if we were unable to initialize a CURL object
		pr->curlcode = 2;
	}
	// clean up
	if (pr->headers){
		curl_slist_free_all(pr->headers);	// free headers
		pr->headers = NULL;
	}
	if (!pr->curlcode){
		pr->curlcode =
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
					&pr->httpresponse);
		if (pr->curlcode != CURLE_OK)
			pr->httpresponse = 500;
	}
	curl_easy_cleanup(curl);	// clean up the curl object

	// Print the response
	//printf("Received %d bytes, status = %d\n",
	//              (int)pr->resp.size, pr->curlcode);
	//printf("--> %s\n", pr->resp.data);
	return pr;
}
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
void pr_free(PouchReq *pr){
	/*
	   Frees any memory that may have been
	   allocated during the creation / 
	   processing of a request. Although it
	   is ok to reuse requests, this
	   MUST be called at the end of your program
	   in order to not leak memory like Assange
	   leaks secret documents.
	 */
	if (pr->easy){	// free request and remove it from multi
		int ret;
		if (pr->multi){
			ret = curl_multi_remove_handle(pr->multi, pr->easy);
			printf("(%d) remd easy %p from multi %p\n", ret, pr->easy, pr->multi);
		}
		curl_easy_cleanup(pr->easy);
		printf("(?) clnd easy %p\n", pr->easy);
	}
	if (pr->resp.data){			// free response data
		free(pr->resp.data);
	}if (pr->req.data){
		free(pr->req.data);		// free request data
	}if (pr->method){			// free method string
		free(pr->method);
	}if (pr->url){				// free URL string
		free(pr->url);
	}if (pr->headers){
		curl_slist_free_all(pr->headers);	// free headers
	}if (pr->usrpwd){
		free(pr->usrpwd);
	}
	free(pr);				// free structure
}

// Database Wrapper Functions
PouchReq *get_all_dbs(PouchReq * p_req, char *server){
	/*
	   Return a list of all databases on a
	   CouchDB server.
	 */
	pr_set_method(p_req, GET);
	pr_set_url(p_req, server);
	p_req->url = combine(&(p_req->url), p_req->url, "_all_dbs", "/");
	return p_req;
}
PouchReq *db_delete(PouchReq * p_req, char *server, char *db){
	/*
	   Delete the database /db/ on the CouchDB
	   server /server/
	 */
	pr_set_method(p_req, DELETE);
	pr_set_url(p_req, server);
	p_req->url = combine(&(p_req->url), p_req->url, db, "/");
	return p_req;
}
PouchReq *db_create(PouchReq * p_req, char *server, char *db){
	/*
	   Create the database /db/ on the CouchDB
	   server /server/
	 */
	pr_set_method(p_req, PUT);
	pr_set_url(p_req, server);
	p_req->url = combine(&(p_req->url), p_req->url, db, "/");
	return p_req;
}
PouchReq *db_get(PouchReq * p_req, char *server, char *db){
	/*
	   Get information about the database /db/
	   on the CouchDB server /server/
	 */
	pr_set_method(p_req, GET);
	pr_set_url(p_req, server);
	p_req->url = combine(&(p_req->url), p_req->url, db, "/");
	return p_req;
}
PouchReq *db_get_changes(PouchReq * pr, char *server, char *db){
	/*
	   Return a list of changes to a document
	   in a CouchDB database. Add custom params with
	   pr_add_param();
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, "_changes", "/");
	return pr;
}
PouchReq *db_get_revs_limit(PouchReq * pr, char *server, char *db){
	/*
	   Returns the current maximum number of revisions
	   allowed for a database.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, "_revs_limit", "/");
	return pr;
}
PouchReq *db_set_revs_limit(PouchReq * pr, char *server, char *db,char *revs){
	/*
	   Sets the maximum number of revisions a database
	   is allowed to have.
	 */
	pr_set_method(pr, PUT);
	pr_set_data(pr, revs);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, "_revs_limit", "/");
	return pr;
}
PouchReq *db_compact(PouchReq * pr, char *server, char *db){
	/*
	   Initiates compaction on a database.
	 */
	pr_set_method(pr, POST);
	pr_set_url(pr, server);
	pr_set_data(pr, "{}");
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, "_compact", "/");
	return pr;
}

// Document Wrapper Functions
PouchReq *doc_get(PouchReq * pr, char *server, char *db, char *id){
	/*
	   Retrieves a document.
	 */
	/*
TODO: URL escape database and document names (/'s become %2F's)
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	return pr;
}
PouchReq *doc_get_rev(PouchReq * pr, char *server, char *db, char *id,char *rev){
	/*
	   Get a specific revision of a document.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	pr_add_param(pr, "rev", rev);
	return pr;
}
PouchReq *doc_get_revs(PouchReq * pr, char *server, char *db,char *id){
	/*
	   Finds out what revisions are available for a document.
	   Returns the current revision of the document, but with
	   an additional field, _revisions, the value being a list
	   of the available revision IDs.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	pr_add_param(pr, "revs", "true");
	return pr;
}
PouchReq *doc_get_info(PouchReq * pr, char *server, char *db,char *id){
	/*
	   A HEAD request returns basic information about the document, including its current revision.
	 */
	pr_set_method(pr, HEAD);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	return pr;
}
PouchReq *doc_create_id(PouchReq * pr, char *server, char *db, char *id, char *data){
	/*
	   Creates a new document with an automatically generated
	   revision ID. The JSON body must include a _id property
	   which contains a unique id. If the document already exists,
	   and the JSON data body includes a _rev property, then
	   the document is updated.
	 */
	pr_set_method(pr, PUT);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	pr_set_data(pr, data);
	return pr;
}
PouchReq *doc_create(PouchReq * pr, char *server, char *db,char *data){
	/*
	   Creates a new document with a server generated DocID.
	 */
	pr_set_method(pr, POST);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr_set_data(pr, data);

	return pr;
}
PouchReq *doc_prcreate(PouchReq *pr, char *server, char *db, char *data){
	pr_set_method(pr, POST);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr_set_prdata(pr, data, strlen(data));
	return pr;
}
PouchReq *get_all_docs(PouchReq * pr, char *server, char *db){
	/*
	   Returns all of the docs in a database.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, "_all_docs", "/");
	return pr;
}
PouchReq *get_all_docs_by_seq(PouchReq * pr, char *server, char *db){
	/*
	   Returns all the documents that have been updated or deleted, in the
	   order that they were modified.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, "_all_docs_by_seq", "/");
	return pr;
}
PouchReq *doc_get_attachment(PouchReq * pr, char *server, char *db,char *id, char *name){
	/*
	   Gets an attachment on a document.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	pr->url = combine(&(pr->url), pr->url, name, "/");
	return pr;
}
PouchReq *doc_copy(PouchReq * pr, char *server, char *db, char *id,char *newid, char *revision){
	/*
	   Copies a document from one id to another,
	   all server side.
	 */
	pr_set_method(pr, COPY);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	// TODO: add support for document overwrite on copy
	char *headerstr = NULL;
	headerstr = combine(&headerstr, "Destination: ", newid, NULL);
	if (revision != NULL){
		headerstr = combine(&headerstr, headerstr, revision, "?rev=");
	}
	pr_add_header(pr, headerstr);
	free(headerstr);
	return pr;
}
PouchReq *doc_delete(PouchReq * pr, char *server, char *db, char *id,char *rev){
	/*
	   Delete a document and all of its attachments.
	   Must include the revision of the document you
	   want to delete.
	 */
	pr_set_method(pr, DELETE);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, id, "/");
	pr_add_param(pr, "rev", rev);
	return pr;
}
PouchReq *doc_add_attachment(PouchReq * pr, char *server, char *db,char *doc, char *filename){
	/*
	   Given a filename, try to read that file and upload it as an attachment to a document.
	 */
	// load the file into memory
	struct stat file_info;
	int fd = open(filename, O_RDONLY);
	if (!fd){
		fprintf(stderr,
				"doc_upload_attachment: could not open file %s\n",
				filename);
	}
	if (lstat(filename, &file_info) != 0){
		fprintf(stderr,
				"doc_upload_attachment: could not lstat file %s\n",
				filename);
		return pr;
		// TODO: include an "error" integer in each PouchReq, to be set
		//               by different wrapper functions
	}
	// read file into buffer
	size_t fd_len = file_info.st_size;
	char fd_buf[fd_len];
	int numbytes = read(fd, fd_buf, fd_len);
	pr_set_bdata(pr, (void *)fd_buf, fd_len);
	close(fd);
	// just in case the actual mime-type is weird or broken, add a default
	// mime-type of application/octet-stream, which is used for binary files.
	// this way, even if something goes horribly wrong, we'll be able to download
	// and view the data we've uploaded.
	pr = pr_add_header(pr, "Content-Type: application/octet-stream");
	// get mime type
	if (USE_SYS_FILE){
		FILE *comres;
		char combuf[strlen("file --mime-type ") + strlen(filename) + 1];
		sprintf(combuf, "file --mime-type %s", filename);
		comres = popen(combuf, "r");
		char comdet[10000];
		fgets(comdet, 10000, comres);
		fclose(comres);
		// store the mime type to a buffer
		char *mtype;
		if ((mtype = strchr(comdet, ' ')) == NULL){
			fprintf(stderr, "could not get mimetype\n");
		}
		mtype++;
		char *endmtype;
		if ((endmtype = strchr(mtype, '\n')) == NULL){
			fprintf(stderr, "could not get end of mimetype\n");
		}
		char ct[strlen("Content-Type: ") + (endmtype - mtype) + 1];
		snprintf(ct,
				strlen("Content-Type: ") + (size_t) (endmtype -
					mtype) + 1,
				"Content-Type: %s", mtype);
		// add the actual mime-type
		pr = pr_add_header(pr, ct);
	} else {
		char *dot;
		if ((dot = strchr(filename, '.')) != NULL){	// if null, then binary file
			char lowercase[strlen(dot) + 1];
			strcpy(lowercase, dot);
			int i;
			for (i = 0; i < strlen(dot); i++){
				lowercase[i] = tolower(lowercase[i]);
			}
			if (!strcmp(lowercase, ".jpg")
					|| !strcmp(lowercase, ".jpeg")){
				pr_add_header(pr, "Content-Type: image/jpeg");
			} else if (!strcmp(lowercase, ".png")){
				pr_add_header(pr, "Content-Type: image/png");
			} else if (!strcmp(lowercase, ".gif")){
				pr_add_header(pr, "Content-Type: image/gif");
			} else if (!strcmp(lowercase, ".tif")){
				pr_add_header(pr, "Content-Type: image/tiff");
			} else if (!strcmp(lowercase, ".c")
					|| !strcmp(lowercase, ".h")
					|| !strcmp(lowercase, ".cpp")
					|| !strcmp(lowercase, ".cxx")
					|| !strcmp(lowercase, ".py")
					|| !strcmp(lowercase, ".md")
					|| !strcmp(lowercase, ".text")
					|| !strcmp(lowercase, ".txt")){
				pr_add_header(pr, "Content-Type: text/plain");
			} else if (!strcmp(lowercase, ".pdf")){
				pr_add_header(pr,
						"Content-Type: application/pdf");
			}
		}
	}
	// finish setting request
	pr_set_method(pr, PUT);
	pr_set_url(pr, server);
	pr->url = combine(&(pr->url), pr->url, db, "/");
	pr->url = combine(&(pr->url), pr->url, doc, "/");
	pr->url = combine(&(pr->url), pr->url, filename, "/");
	// TODO: add support for adding to existing documents by auto-fetching the rev parameter
	//pr_add_param(pr, "rev", rev);
	return pr;
}

// Generic libevent callback functions
size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	/*
	   This callback is used to save responses from CURL requests.
	   It loads the response into the PouchReq pointed to by
	   data.
	 */
	size_t ptrsize = nmemb*size; // this is the size of the data pointed to by ptr
	PouchReq *pr = (PouchReq *)data;
	pr->resp.data = (char *)realloc(pr->resp.data, pr->resp.size + ptrsize +1);
	if (pr->resp.data){	// realloc was successful
		memcpy(&(pr->resp.data[pr->resp.size]), ptr, ptrsize); // append new data
		pr->resp.size += ptrsize;
		pr->resp.data[pr->resp.size] = '\0'; // null terminate the new data
	}
	else { // realloc was NOT successful
		fprintf(stderr, "recv_data_callback: realloc failed\n");
	}
	return ptrsize; // theoretically, this is the amount of processed data
}
size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data){
	/*
	   This callback is used to send data for a CURL request. The JSON
	   string stored in the PouchReq pointed to by data is read out
	   and sent, piece by piece.
	 */
	size_t maxcopysize = nmemb*size;
	if (maxcopysize < 1){
		return 0;
	}
	PouchReq *pr = (PouchReq *)data;
	if (pr->req.size > 0){ // only send data if there's data to send
		size_t tocopy = (pr->req.size > maxcopysize) ? maxcopysize : pr->req.size;
		memcpy(ptr, pr->req.offset, tocopy);
		pr->req.offset += tocopy;	// advance our offset by the number of bytes already sent
		pr->req.size -= tocopy;	//next time there are tocopy fewer bytes to copy
		return tocopy;
	}
	return 0;
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

