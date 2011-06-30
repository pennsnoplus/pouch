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

//typedef struct pouch_request {
//	pouch_pkt pkt;
//	CURLcode curlcode;
//} pouch_request;

typedef struct pouch_request {
	char *method;		// HTTP method
	char *url;			// Destination (e.g., "http://127.0.0.1:5984/test");
	CURLcode curlcode;	// CURL request status holder
	pouch_pkt req;		// holds data to be sent
	pouch_pkt resp;		// holds response
} pouch_request;

pouch_request *pouch_init_request(void){
	pouch_request *pr = calloc(1, sizeof(pouch_request));
	pr->resp.data = NULL;	// empty the response buffer
	pr->resp.size = 0;

	// initialize the data to be sent as nothing
	//printf("mallocing 1 byte for data\n");
	//pr->req.data = (char *)malloc(sizeof(char));
	//printf("FINISHED\n");
	//memcpy(pr->req.data, '0', 1);
	//printf("DONE\n");
	pr->req.offset = pr->req.data;
	pr->req.size = 0;
	return pr;
}

pouch_request *pouch_request_set_method(pouch_request *pr, char *method){
	size_t length = strlen(method)+1; // include '\0' terminator
	if(pr->method)
		free(pr->method);
	pr->method = (char *)malloc(length); // allocate space
	memcpy(pr->method, method, length);	 // copy the method
	return pr;
}
pouch_request *pouch_request_set_url(pouch_request *pr, char *url){
	size_t length = strlen(url)+1; // include '\0' terminator
	if(pr->url)
		free(pr->url);
	pr->url = (char *)malloc(length);
	memcpy(pr->url, url, length);
	return pr;
}
pouch_request *pouch_request_set_data(pouch_request *pr, char *str){
	size_t length = strlen(str)+1; // include the '\0' terminator
	if(pr->req.data)
		free(pr->req.data);
	pr->req.data = (char *)malloc(length);
	memcpy(pr->req.data, str, length);
	pr->req.offset = pr->req.data;
	return pr;
}

void pouch_free_request(pouch_request *pr){
	if (pr->resp.data){
		free(pr->resp.data);
	}if (pr->resp.offset){
		free(pr->resp.offset);
	}if (pr->req.data){
		free(pr->req.data);
	}if (pr->req.offset){
		free(pr->req.offset);
	}if (pr->method){
		free(pr->method);
	}if (pr->url){
		free(pr->url);
	}
	free(pr);
}

size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	size_t ptrsize = nmemb*size;
	//pouch_pkt *pkt = (pouch_pkt *)data;
	pouch_request *pr = (pouch_request *)data;
	//pkt->data = (char *) realloc(pkt->data, pkt->size + ptrsize + 1);
	pr->resp.data = (char *)realloc(pr->resp.data, pr->resp.size + ptrsize +1);
	//if (pkt->data){ // realloc was successful
	if (pr->resp.data){
		//memcpy(&(pkt->data[pkt->size]), ptr, ptrsize); // append new data
		memcpy(&(pr->resp.data[pr->resp.size]), ptr, ptrsize);
		//pkt->size += ptrsize;
		pr->resp.size += ptrsize;
		//pkt->data[pkt->size] = '\0';
		pr->resp.data[pr->resp.size] = '\0';
	}
	else {
		fprintf(stderr, "recv_data_callback: realloc failed\n");
	}
	return ptrsize;
}

size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data){
	size_t maxcopysize = nmemb*size;
	//pouch_pkt *pkt = (pouch_pkt *)data;
	pouch_request *pr = (pouch_request *)data;
	//if (pkt->size > 0){ // there must be data to send
	if (pr->req.size > 0){
		//size_t tocopy = (pkt->size > maxcopysize) ? maxcopysize : pkt->size;
		size_t tocopy = (pr->req.size > maxcopysize) ? maxcopysize : pr->req.size;
		//memcpy(ptr, pkt->offset, tocopy);
		memcpy(ptr, pr->req.offset, tocopy);
		//pkt->offset += tocopy; // advance our offset by the number of bytes already sent, so that if this function is called again, the same data isn't sent multiple times
		pr->req.offset += tocopy;
		//pkt->size -= tocopy; // we already copied tocopy number bytes, so next time we have to copy fewer.
		pr->req.size -= tocopy;
		return tocopy;
	}
	return 0;
}

//pouch_request pouch_curl(char *url, char *method, char *data){
pouch_request *pouch_do_request(pouch_request *pr){
	CURL *curl;
	//CURLcode ret;
	struct curl_slist *headers = NULL;

	//pouch_pkt recv_pkt;
	//recv_pkt.data=NULL; // realloc should fix this during callback
	//recv_pkt.size=0;

	//pouch_pkt send_pkt = {0,0,0}; // nothing to see here

	curl = curl_easy_init();

	if(curl){
		// setup the CURL object/request
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "pouch/0.1");				// add user-agent
		curl_easy_setopt(curl, CURLOPT_URL, pr->url);						// where to send this request
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, pr->method);			// choose a method
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);	// where to store the data
		//curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pr->resp);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pr);

		// print the request
		printf("libcurl request:\n");
		printf("\t%s : %s\n", pr->method, pr->url);
		//if(data && strlen(data) > 0){
		//send_pkt.data = (char *)malloc(strlen(data)+1);
		//memcpy(send_pkt.data, data, strlen(data)+1);
		//send_pkt.offset = send_pkt.data;
		//send_pkt.size = strlen(data)+1;
		if(pr->req.data && strlen(pr->req.data) > 0){
			if(!strncmp(pr->method, "PUT", 3)){
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			}
			else if(!strncmp(pr->method, "POST", 4)){
				curl_easy_setopt(curl, CURLOPT_POST, 1);
				headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);	// add the custom headers
			}
			// let CURL know what data to send
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, send_data_callback);
			//curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&send_pkt);
			curl_easy_setopt(curl, CURLOPT_READDATA, (void *)pr);

			//printf("\t%s\n", send_pkt.data);
			printf("\t%s\n", pr->req.data);
		}

		//ret = curl_easy_perform(curl); 	// make request
		pr->curlcode = curl_easy_perform(curl);
		curl_slist_free_all(headers);	// free headers
		curl_easy_cleanup(curl);		// clean up the curl object
		//if(send_pkt.data){
		//	free(send_pkt.data);
		//}
	}
	else{
		// if we were unable to initialize a CURL object
		//ret = 2;
		pr->curlcode = 2;
	}
	return pr;
	//pouch_request out = {recv_pkt, ret};
	//return out;
}

int main(int argc, char* argv[]){
	//char *data = NULL;
	pouch_request *helloworld = pouch_init_request();
	helloworld = pouch_request_set_method(helloworld, "GET");
	helloworld = pouch_request_set_url(helloworld, "http://127.0.0.1:5984");

	//pouch_request couchdata = pouch_curl("http://127.0.0.1:5984", "GET", data);
	helloworld = pouch_do_request(helloworld);

	//printf("curlcode: %d\n", couchdata.curlcode);
	printf("curlcode: %d\n", helloworld->curlcode);
	//printf("Response length: %d\n", (int)couchdata.pkt.size);
	printf("Response length: %d\n", (int)helloworld->resp.size);
	//printf("Response data: %s\n", couchdata.pkt.data);
	printf("Response data: %s\n", helloworld->resp.data);

	//free(couchdata.pkt.data);
	//free(couchdata.pkt.offset);
	pouch_free_request(helloworld);

	return 0;
}
