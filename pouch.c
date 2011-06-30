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
	char *data;
	char *offset;
	size_t size;
} pouch_pkt;

typedef struct pouch_resp {
	pouch_pkt pkt;
	CURLcode curlcode;
} pouch_resp;

typedef struct pouch_request {
	char *method;		// HTTP method
	char *url;	// Destination (e.g., "http://127.0.0.1:5984/test");
	CURLcode curlcode;	// CURL request status holder
	pouch_pkt req;		// holds data to be sent
	pouch_pkt resp;		// holds response
} pouch_request;

pouch_request *pouch_request_new(void){
	pouch_request *pr = calloc(1, sizeof(pouch_request));
	pr->resp.data = NULL;	// empty the response buffer
	pr->resp.size = 0;
	
	// initialize the data to be sent as nothing
	pr->req.data = (char *)malloc(1);
	memcpy(pr->req.data, 0, 1);
	pr->req.offset = pr->req.data;
	pr->req.size = 0;
	return pr;
}
/*
		if(data && strlen(data) > 0) {
			send_pkt.data = (char *)malloc(strlen(data)+1);
			memcpy(send_pkt.data, data, strlen(data)+1);
			send_pkt.offset = send_pkt.data;
			send_pkt.size = strlen(data)+1;
*/
pouch_request *pouch_request_set_method(pouch_request *pr, char *method) {
	size_t length = strlen(method)+1; // include '\0' terminator
	if(pr->method)
		free(pr->method);
	pr->method = (char *)malloc(length); // allocate space
	memcpy(pr->method, method, length);	 // copy the method
	return pr;
}
pouch_request *pouch_request_set_url(pouch_request *pr, char *url) {
	size_t length = strlen(url)+1; // include '\0' terminator
	if(pr->url)
		free(pr->url);
	pr->url = (char *)malloc(length);
	memcpy(pr->url, url, length);
	return pr;
}
pouch_request *pouch_request_set_data(pouch_request *pr, char *str) {
	size_t length = strlen(str)+1; // include the '\0' terminator
	if(pr->req.data)
		free(pr->req.data);
	pr->req.data = (char *)malloc(length);
	memcpy(pr->req.data, str, length);
	pr->req.offset = pr->req.data;
	return pr;
}

void pouch_free_request(pouch_request *pr) {
	if (pr->resp.data)
		free(pr->resp.data);
	if (pr->resp.offset)
		free(pr->resp.offset);
	if (pr->req.data)
		free(pr->req.data);
	if (pr->req.offset)
		free(pr->req.offset);
}

size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	size_t ptrsize = nmemb*size;
	pouch_pkt *pkt = (pouch_pkt *)data;

	pkt->data = (char *) realloc(pkt->data, pkt->size + ptrsize + 1);
	if (pkt->data) { // realloc was successful
		memcpy(&(pkt->data[pkt->size]), ptr, ptrsize); // append new data
		pkt->size += ptrsize;
		pkt->data[pkt->size] = '\0';
	}
	else {
		fprintf(stderr, "recv_data_callback: realloc failed\n");
	}
	return ptrsize;
}

size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data){
	size_t maxcopysize = nmemb*size;
	pouch_pkt *pkt = (pouch_pkt *)data;
	if (pkt->size > 0) { // there must be data to send
		size_t tocopy = (pkt->size > maxcopysize) ? maxcopysize : pkt->size;
		memcpy(ptr, pkt->offset, tocopy);
		pkt->offset += tocopy; // advance our offset by the number of bytes already sent, so that if this function is called again, the same data isn't sent multiple times
		pkt->size -= tocopy; // we already copied tocopy number bytes, so next time we have to copy fewer.
		return tocopy;
	}
	return 0;
}

pouch_resp pouch_curl(char *url, char *method, char *data){
	CURL *curl;
	CURLcode ret;
	struct curl_slist *headers = NULL;

	pouch_pkt recv_pkt;
	recv_pkt.data=NULL; // realloc should fix this during callback
	recv_pkt.size=0;

	pouch_pkt send_pkt = {0,0,0}; // nothing to see here

	curl = curl_easy_init();

	if(curl){
		// setup the CURL object/request
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "pouch/0.1");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_pkt);

		// print the request
		printf("libcurl request:\n");
		printf("\t%s : %s\n", method, url);
		if(data && strlen(data) > 0) {
			send_pkt.data = (char *)malloc(strlen(data)+1);
			memcpy(send_pkt.data, data, strlen(data)+1);
			send_pkt.offset = send_pkt.data;
			send_pkt.size = strlen(data)+1;
			
			if(!strncmp(method, "PUT", 3)){
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			}
			else if(!strncmp(method, "POST", 4)){
				curl_easy_setopt(curl, CURLOPT_POST, 1);
				headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			}

			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, send_data_callback);
			curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&send_pkt);

			printf("\t%s\n", send_pkt.data);
		}

		ret = curl_easy_perform(curl); 	// make request
		curl_slist_free_all(headers);	// free headers
		curl_easy_cleanup(curl);		// clean up the curl object
		if(send_pkt.data){
			free(send_pkt.data);
		}
	}
	else{
		// if we were unable to initialize a CURL object
		ret = 2;
	}
	pouch_resp out = {recv_pkt, ret};
	return out;
}

int main(int argc, char* argv[]){
	char *data = NULL;
	pouch_resp couchdata = pouch_curl("http://127.0.0.1:5984", "GET", data);
	printf("CURL_code: %d\n", couchdata.curlcode);
	printf("Response length: %d\n", (int)couchdata.pkt.size);
	printf("Response data: %s\n", couchdata.pkt.data);

	free(couchdata.pkt.data);
	free(couchdata.pkt.offset);
	
	return 0;
}
