#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

pouch_request *pouch_init_request(void){
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

pouch_request *pouch_request_set_method(pouch_request *pr, char *method){
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
pouch_request *pouch_request_set_url(pouch_request *pr, char *url){
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
pouch_request *pouch_request_set_data(pouch_request *pr, char *str){
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

	// Because of the way CURL sends data,
	// before sending the pouch_pkt's
	// offset must point to the same address
	// in memory as the data pointer does.
	pr->req.offset = pr->req.data;
	return pr;
}

void pouch_free_request(pouch_request *pr){
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
	}if (pr->resp.offset){
		free(pr->resp.offset);
	}if (pr->req.data){
		free(pr->req.data);		// free request
	}if (pr->req.offset){
		free(pr->req.offset);
	}if (pr->method){			// free method string
		free(pr->method);
	}if (pr->url){				// free URL string
		free(pr->url);
	}
	free(pr);					// free structure
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

pouch_request *pouch_do_request(pouch_request *pr){
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
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);	// where to store the response
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pr);

		// print the request
		printf("%s : %s\n", pr->method, pr->url);
		if(pr->req.data && strlen(pr->req.data) > 0){ // check for data upload
			if(!strncmp(pr->method, PUT, 3)){ // PUT-specific option
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			}
			else if(!strncmp(pr->method, POST, 4)){ // POST-specific options
				curl_easy_setopt(curl, CURLOPT_POST, 1);
				headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);	// add the custom headers
			}
			// let CURL know what data to send
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, send_data_callback);
			curl_easy_setopt(curl, CURLOPT_READDATA, (void *)pr);

			// print the data being sent
			printf("\t%s\n", pr->req.data);
		}
		
		// empty the response buffer
		pr->resp.data = NULL;
		pr->resp.size = 0;

		// make the request and store the response
		pr->curlcode = curl_easy_perform(curl);

		// clean up
		curl_slist_free_all(headers);	// free headers
		curl_easy_cleanup(curl);		// clean up the curl object
	}
	else{
		// if we were unable to initialize a CURL object
		pr->curlcode = 2;
	}
	return pr;
}

void pouch_request_display(pouch_request *pr){
	printf("curlcode: %d\n", pr->curlcode);
	printf("length:   %d\n", (int)pr->resp.size);
	printf("message:  %s\n", pr->resp.data);
}

int main(int argc, char* argv[]){
	// create a request object
	pouch_request *probj = pouch_init_request();

	// setup a PUT request to create a database
	probj = pouch_request_set_method(probj, PUT);
	probj = pouch_request_set_url(probj, "http://127.0.0.1:5984/newdb/");

	// make the request
	probj = pouch_do_request(probj);

	// show the result
	pouch_request_display(probj);

	// create a new document within the new database
	probj = pouch_request_set_url(probj, "http://127.0.0.1:5984/newdb/testdoc");
	//TODO: probj = pouch_request_set_data(probj, "{}");

	// make the request
	probj = pouch_do_request(probj);

	// show the result
	pouch_request_display(probj);

	// finished making requests; free the request object
	pouch_free_request(probj);

	return 0;
}
