#include <curl/curl.h>

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
	char *method;		// HTTP method
	char *url;			// Destination (e.g., "http://127.0.0.1:5984/test");
	CURLcode curlcode;	// CURL request status holder
	pouch_pkt req;		// holds data to be sent
	pouch_pkt resp;		// holds response
} pouch_request;

pouch_request *pr_init(void){
	/*
	   Initializes a new pouch_request
	   object.
	 */
	pouch_request *pr = calloc(1, sizeof(pouch_request));

	// initializes the request buffer
	pr->req.offset = pr->req.data;
	pr->req.size = 0;
	return pr;
}

pouch_request *pr_set_method(pouch_request *pr, char *method){
	/*
	   Sets the HTTP method of
	   a specific request.
	 */
	size_t length = strlen(method)+1; // include '\0' terminator
	if(pr->method)
		free(pr->method);
	pr->method = (char *)malloc(length); // allocate space
	memcpy(pr->method, method, length);	 // copy the method
	return pr;
}
pouch_request *pr_set_url(pouch_request *pr, char *url){
	/*
	   Sets the target URL of
	   a CouchDB request.
	 */
	size_t length = strlen(url)+1; // include '\0' terminator
	if(pr->url)	// if there is an older url, get rid of it
		free(pr->url);
	pr->url = (char *)malloc(length); // allocate space
	memcpy(pr->url, url, length);	  // copy the new url

	return pr;
}
pouch_request *pr_set_data(pouch_request *pr, char *str){
	/*
	   Sets the data that a request
	   sends. If the request does not
	   need to send data, do NOT call
	   this function with an empty string,
	   just refrain from calling the function.
	 */
	size_t length = strlen(str)+1; // include the '\0' terminator
	if(pr->req.data)	// free older data
		free(pr->req.data);
	pr->req.data = (char *)malloc(length);	// allocate space
	memcpy(pr->req.data, str, length);	// copy it over
	pr->req.data[strlen(pr->req.data)] = '\0';
	// Because of the way CURL sends data,
	// before sending the pouch_pkt's
	// offset must point to the same address
	// in memory as the data pointer does.
	pr->req.offset = pr->req.data;
	pr->req.size = length;
	return pr;
}

void pr_free(pouch_request *pr){
	/*
	   Frees any memory that may have been
	   allocated during the creation / 
	   processing of a request. Although it
	   is ok to reuse requests, this
	   MUST be called at the end of your program
	   in order to not leak memory like Assange
	   leaks secret documents.
	 */
	if (pr->resp.data){			// free response
		free(pr->resp.data);
	//}if (pr->resp.offset){
		//free(pr->resp.offset);
	}if (pr->req.data){
		printf("Freeing data\n");
		free(pr->req.data);		// free request
pouch_request *pr_clear_data(pouch_request *pr){
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
	//}if (pr->req.offset){
		//free(pr->req.offset);
	}if (pr->method){			// free method string
		free(pr->method);
	}if (pr->url){				// free URL string
		free(pr->url);
	}
	free(pr);				// free structure
}

pouch_request *pr_clear_data(pouch_request *pr){
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
size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	/*
	   This callback is used to save responses from CURL requests.
	   It loads the response into the pouch_request pointed to by
	   data.
	 */
	size_t ptrsize = nmemb*size; // this is the size of the data pointed to by ptr
	pouch_request *pr = (pouch_request *)data;
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
	   string stored in the pouch_request pointed to by data is read out
	   and sent, piece by piece.
	 */
	size_t maxcopysize = nmemb*size;
	pouch_request *pr = (pouch_request *)data;
	if (pr->req.size > 0){ // only send data if there's data to send
		size_t tocopy = (pr->req.size > maxcopysize) ? maxcopysize : pr->req.size;
		memcpy(ptr, pr->req.offset, tocopy);
		pr->req.offset += tocopy;	// advance our offset by the number of bytes already sent
		pr->req.size -= tocopy;	//next time there are tocopy fewer bytes to copy
		return tocopy;
	}
	return 0;
}

pouch_request *pr_do(pouch_request *pr){
	CURL *curl;	// CURL object to make the requests
	struct curl_slist *headers = NULL;	// Custom headers for uploading
	//TODO: put headers in the pouch_request object,
	//		so that uploads and stuff work correctly

	// initialize the CURL object
	curl = curl_easy_init();

	if(curl){
		// setup the CURL object/request
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "pouch/0.1");				// add user-agent
		curl_easy_setopt(curl, CURLOPT_URL, pr->url);						// where to send this request
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, pr->method);			// choose a method
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);					// Timeout after 3 seconds
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);	// where to store the response
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pr);

		if(pr->req.data && pr->req.size > 0){ // check for data upload
			if(!strncmp(pr->method, PUT, 3)){ // PUT-specific option
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
				headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
			}
			else if(!strncmp(pr->method, POST, 4)){ // POST-specific options
				curl_easy_setopt(curl, CURLOPT_POST, 1);
				headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
				headers = curl_slist_append(headers, "Content-Type: application/json");
			}
			// add the custom headers
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

			// let CURL know what data to send
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, send_data_callback);
			curl_easy_setopt(curl, CURLOPT_READDATA, (void *)pr);

			// print the data being sent
			printf("\t%s\n", pr->req.data);
		}

		// empty the response buffer
		if (pr->resp.data){
			free(pr->resp.data);
		}
		pr->resp.data = NULL;

		pr->resp.size = 0;

		// make the request and store the response
		pr->curlcode = curl_easy_perform(curl);
	}
	else{
		// if we were unable to initialize a CURL object
		pr->curlcode = 2;
	}
	// clean up
	curl_slist_free_all(headers);	// free headers
	curl_easy_cleanup(curl);		// clean up the curl object

	// Print the request
	printf("Sent %s : %s\n", pr->method, pr->url);
	if(pr->req.data && pr->req.size > 0){ // check for data upload
		printf("\t%s\n", pr->req.data);
		pr_clear_data(pr);
	}
	printf("Received %d bytes, status = %d\n",
			(int)pr->resp.size, pr->curlcode);
	printf("\t%s\n", pr->resp.data);
	return pr;
}

pouch_request *pr_add_param(pouch_request *pr, char *key, char *value){
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
pouch_request *pr_clear_params(pouch_request *pr){
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

char *combine(char *f, char *s, char *sep){
	/*
	   Appends the string s to the string f,
	   with an optional separator sep. If a
	   seperator is not required, pass NULL
	   to the sep argument.
	 */
	size_t length = strlen(f)+strlen(s);
	if(sep != NULL){
		length += strlen(sep);
	}
	length++;
	char buf[length];
	if(sep){
		sprintf(buf, "%s%s%s", f, sep, s);
	}
	else{
		sprintf(buf, "%s%s", f, s);
	}
	buf[length-1]='\0'; // null terminate
	if(*f){
		free(f);
	}
	f = (char *)malloc(length);
	memset(f, '\0', length); // TODO: find out if this is necessary anywhere else
	memcpy(f, buf, length);
	return f;
}

pouch_request *db_get_all(pouch_request *p_req, char *server){
	/*
	   Return a list of all databases on a
	   CouchDB server.
	 */
	pr_set_method(p_req, GET);
	pr_set_url(p_req, server);
	p_req->url = combine(p_req->url, "_all_dbs", "/");
	pr_do(p_req);
	return p_req;
}

pouch_request *db_delete(pouch_request *p_req, char *server, char *name){
	/*
	   Delete the database /name/ on the CouchDB
	   server /server/
	 */
	pr_set_method(p_req, DELETE);
	pr_set_url(p_req, server);
	p_req->url = combine(p_req->url, name, "/");
	pr_do(p_req);
	return p_req;
}

pouch_request *db_create(pouch_request *p_req, char *server, char *name){
	/*
	   Create the database /name/ on the CouchDB
	   server /server/
	 */
	pr_set_method(p_req, PUT);
	pr_set_url(p_req, server);
	p_req->url = combine(p_req->url, name, "/");
	pr_do(p_req);
	return p_req;
}

pouch_request *db_get(pouch_request *p_req, char *server, char *name){
	/*
	   Get information about the database /name/
	   on the CouchDB server /server/
	 */
	pr_set_method(p_req, GET);
	pr_set_url(p_req, server);
	p_req->url = combine(p_req->url, name, "/");
	pr_do(p_req);
	return p_req;
}
pouch_request *db_get_changes(pouch_request *pr, char *server, char *name, int since, int limit, char *feed, int heartbeat, int timeout, char *filter, char *include_docs){
	/*
	   Return a list of changes to a document
	   in a CouchDB database.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, name, "/");
	pr->url = combine(pr->url, "_changes", "/");
	char tmp[400]; // temporary buffer, 400 was arbitrary
	if (since != (int)NULL){
		memset(&tmp, '\0', 400);
		sprintf(tmp, "%d", since);
		pr_add_param(pr, "since", tmp);
	}
	if (limit != (int)NULL){
		memset(&tmp, '\0', 400);
		sprintf(tmp, "%d", limit);
		pr_add_param(pr, "limit", tmp);
	}
	if (feed != (char *)NULL){
		pr_add_param(pr, "feed", feed);
	}
	if (heartbeat != (int)NULL){
		memset(&tmp, '\0', 400);
		sprintf(tmp, "%d", heartbeat);
		pr_add_param(pr, "heartbeat", tmp);
	}
	if (timeout != (int)NULL){
		memset(&tmp, '\0', 400);
		sprintf(tmp, "%d", timeout);
		pr_add_param(pr, "timeout", tmp);
	}
	if (filter != (char *)NULL){
		pr_add_param(pr, "filter", filter);
	}
	if (include_docs != (char *)NULL){
		pr_add_param(pr, "include_docs", include_docs);
	}
	pr_do(pr);
	return pr;
}