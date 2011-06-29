#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

typedef struct pouch_pkt {
	/*
	   A data structure to hold
	   JSON-data to be sent to/from
	   a couchDB server.
	 */
	char *json_data; 	// some amount of json-ified data
	size_t size;		// that amount, exactly
} pouch_pkt;

pouch_pkt new_pouch_pkt() {
	/*
	   Creates, zeroes, and returns
	   a new pouch_pkt.
	 */
	pouch_pkt out;
	out.json_data = (char *)NULL;
	out.size = 0;
	return out;
}

typedef struct pouch_query {
	/*
	   A data structure to be used
	   to keep track of requests /
	   responses to and from a couchDB
	   server.
	 */ 
	char *server;		// server address
	char *database;		// database (optional)
	char *id;			// id (optional)
	pouch_pkt req;			// the request sent to the server
	pouch_pkt resp;		// the server's response
	CURLcode curl_resp; // CURL response code
} pouch_query;

pouch_query new_pouch_query(){
	/*
	   Creates, zeroes, and returns
	   a new pouch_query.
	 */
	pouch_query out;
	out.server = (char *)NULL;
	out.database = (char *)NULL;
	out.id = (char *)NULL;
	out.req = new_pouch_pkt();
	out.resp = new_pouch_pkt();
	return out;
}

void pouch_init(){
	/*
	   A wrapper for CURL initialization.
	   Must be run at the beginning of
	   any program that tries to send/
	   receive using CURL.
	   */
	curl_global_init(CURL_GLOBAL_ALL);
}



typedef struct couch_pkt {
	char *memory; // use malloc for size
	size_t size;
} couch_pkt;

couch_pkt new_couch_pkt(void){
	couch_pkt out;
	out.memory=NULL;
	out.size=0;
	return out;
}

size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	/*
	   Stores received data into a couch_pkt.

	   ptr = actual data
	   size*nmemb = size of ptr
	   data = pointer to couch_pkt
	 */
	size_t realsize = nmemb*size; // size of ptr
	couch_pkt *pkt = (couch_pkt *)data;
	pkt->memory = (char *) realloc(pkt->memory, pkt->size + realsize + 1);

	if (pkt->memory) { // realloc was successful
		memcpy(&(pkt->memory[pkt->size]), ptr, realsize); // append the new data pointed to by ptr
		pkt->size += realsize;
		pkt->memory[pkt->size] = '\0'; // null append the string
	}
	return realsize;
}

char* build_request(char *server, char *db, char *id){
	// build the request string
	size_t size = (size_t)strlen(server) + (size_t)strlen(db) + (size_t)strlen(id) + 2;
	char *url_string;
	url_string = (char *)malloc(size);
	sprintf(url_string, "%s/%s/%s", server, db, id);
	return url_string;
}

CURLcode couchRequest(char *method, char *url_string, couch_pkt *data, couch_pkt *resp){
	// initialize CURL object
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl){

		// setup the CURL object/request
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "soncoucher/0.1");
		curl_easy_setopt(curl, CURLOPT_URL, url_string);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
		//TODO: if method is PUT/POST/DELETE/COPY, use the upload method.
		if ( !strncmp(method, "PUT", 3) ||
				!strncmp(method, "POST", 4) ||
				!strncmp(method, "DELETE", 6) ||
				!strncmp(method, "COPY", 4) ) {
			// do nothing for the moment
		}

		//curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1);
		//curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)resp);

		// make the request
		res = curl_easy_perform(curl);

		// clean up afterwards
		curl_easy_cleanup(curl);
		return res;
	}
	// if we were unable to initialize a CURL object
	return CURLE_FAILED_INIT;
}

int main(int argc, char* argv[]){
	couch_pkt data = new_couch_pkt(); // holds data to be sent out
	couch_pkt resp = new_couch_pkt(); // holds data returned from CouchDB server
	CURLcode result = couchRequest("GET", "http://127.0.0.1:5984/test/_all_docs", &data, &resp);
	printf("--Result--\nError Code: %d\nSize: %d\nData: %s\n",
			result, (int)resp.size, (char *)resp.memory);

	// you have to free your responses
	free(data.memory);
	free(resp.memory);
	return (int)result;	
}
