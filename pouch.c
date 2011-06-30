#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

typedef struct pouch_pkt {
	char *data;
	char *offset;
	size_t size;
} pouch_pkt;

typedef struct pouch_resp {
	pouch_pkt pkt;
	CURLcode curlcode;
} pouch_resp;

size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	size_t ptrsize = nmemb*size;
	pouch_pkt *pkt = (pouch_pkt *)data;

	pkt->data = (char *) realloc(pkt->data, pkt->size + ptrsize + 1);
	if (pkt->data) { // realloc was successful
		memcpy(&(pkt->data[pkt->size]), ptr, ptrsize); // append new data
		pkt->size += ptrsize;
		pkt->data[pkt->size] = '\0'; //TODO: why not '\0'?
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

pouch_resp pouch_curl(char *urlstring, char *method, char *data){
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
		curl_easy_setopt(curl, CURLOPT_URL, urlstring);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_pkt);

		// print the request
		printf("libcurl request:\n");
		printf("\t%s : %s\n", method, urlstring);
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
	pouch_resp couchdata = pouch_curl("http://127.0.0.1:5984/test/", "GET", data);
	free(couchdata.pkt.data);
	free(couchdata.pkt.offset);
	return 0;
}
