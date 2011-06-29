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
	char *offset;		// for sends, the amount of data already sent
	// 	 (multiple send calls can be made to one file,
	// 	 but we don't want to resend the same data twice)
	size_t size;		// that amount, exactly
} pouch_pkt;

pouch_pkt new_pouch_pkt() {
	/*
	   Creates, zeroes, and returns
	   a new pouch_pkt.
	 */
	pouch_pkt out;
	out.json_data = malloc(1);
	memcpy(&out.json_data, "\0", 1);
	out.offset = out.json_data;
	out.size = 1;
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
	char *db;		// database (optional)
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
	out.server = "";
	out.db = "";
	out.id = "";
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

size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
	size_t ptrsize = nmemb*size;
	pouch_query *query = (pouch_query *)data;
	query->resp.json_data = (char *) realloc(query->resp.json_data, (size_t)(query->resp.size + ptrsize + 1));
	if (query->resp.json_data) { // realloc was successful
		memcpy(&(query->resp.json_data), ptr, ptrsize); // append the received data
		query->resp.size += ptrsize; // keep track of how much data we have
		query->resp.json_data[query->resp.size] = '\0'; // null append the string
	}
	else {
		fprintf(stderr, "recv_data_callback: unable to successfully realloc %d bytes", (int) ptrsize);
		//return 0; // this triggers errors within the CURL library
	}
	return ptrsize;
}

size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data){
	size_t maxcopysize = nmemb*size;
	pouch_query *query = (pouch_query *)data;
	if (query->req.size > 0) { // there must be data to send
		size_t tocopy = (query->req.size > maxcopysize) ? maxcopysize : query->req.size; // at most, copy over as much as possible. At least, copy over only as much data as we have.
		memcpy(ptr, query->req.offset, tocopy);
		//TODO: before send, make sure that query->req.offset = query->req.json_data, otherwise we'll get *weird* results. Random data being sent, weird. That freaking crazy.
		query->req.offset += tocopy; // advance our offset by the number of bytes already sent, so that if this function is called again, the same data isn't sent multiple times
		query->req.size -= tocopy; // we already copied tocopy number bytes, so next time we have to copy fewer.
		return tocopy;
	}
	return 0;
}

char *build_urlstring(pouch_query *query){
	char *urlstring;
	size_t size = strlen(query->server);
	urlstring = (char *)malloc(size);
	sprintf(urlstring, "%s", query->server);
	// TODO: there has to be a better way to do this
	if (strlen(query->db) > 0){ // if there's a database
		size += (1 + strlen(query->db));
		urlstring = (char *) realloc(urlstring, size);
		sprintf(urlstring, "%s/%s", urlstring, query->db);
		if (strlen(query->id) > 0){ // if there's an id
			size += (1 + strlen(query->id));
			urlstring = (char *) realloc(urlstring, size);
			sprintf(urlstring, "%s/%s", urlstring, query->id);
		}
	}
	return urlstring; // MAKE SURE TO FREE THIS LATER
}

void pouch_curl(CURL *curl, char *method, pouch_query *query){
	if(curl){
		char *urlstring = "http://127.0.0.1:5984/test/_all_docs";//build_urlstring(query);
		// setup the CURL object/request
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "pouch/0.1");
		curl_easy_setopt(curl, CURLOPT_URL, urlstring);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)query);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, send_data_callback);
		curl_easy_setopt(curl, CURLOPT_READDATA, (void *)query);
		
		// setup timeouts on the request
		// curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1);
		// curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

		// print the request
		printf("libcurl request:\n");
		printf("\t%s : %s\n", method, urlstring);
		//printf("\t%s\n", query->req.json_data);
		
		// send the request
		query->curl_resp = curl_easy_perform(curl);

		// clean up afterwards
		curl_easy_cleanup(curl);
		//free(urlstring);
	}
	else{
		// if we were unable to initialize a CURL object
		query->curl_resp = -1;
	}
}

void pouch_set_server(pouch_query *query, char *srv){
	query->server = (char *)malloc(strlen(srv));
	memcpy(query->server, srv, strlen(srv));
}

void pouch_set_db(pouch_query *query, char *db){
	query->db = (char *)malloc(strlen(db));
	memcpy(query->db, db, strlen(db));
}

void pouch_set_id(pouch_query *query, char *id){
	query->id = (char *)malloc(strlen(id));
	memcpy(query->id, id, strlen(id));
}

void pouch_set_req(pouch_query *query, char *json){
	query->req.json_data = (char *)malloc(strlen(json));
	memcpy(query->req.json_data, json, strlen(json));
	query->req.offset = query->req.json_data;
	query->req.size = strlen(json);
}

int main(int argc, char* argv[]){
	// initialize CURL object
	CURL *curl;
	curl = curl_easy_init(); // TODO: make this nonblocking, don't use easy_init()
	// create a new query
	pouch_query all_test_docs = new_pouch_query();

	// setup where the request points to
	pouch_set_server(&all_test_docs, "http://127.0.0.1:5984");
	pouch_set_db(&all_test_docs, "test");
	pouch_set_id(&all_test_docs, "_all_docs");
	pouch_set_req(&all_test_docs, "\0");
	
	// make the request
	pouch_curl(curl, "GET", &all_test_docs);
	
	// show the result
	printf("libcurl result\n");
	printf("\terror_code: %d\n", all_test_docs.curl_resp);
	printf("\tsize: %d\n", (int)all_test_docs.resp.size);
	printf("\tdata: %s\n", all_test_docs.resp.json_data);

	// clean up
	// TODO: turn this into a function ("pouch_free_all_test_docs")
	free(all_test_docs.server);	//NOTE: if these aren't explicitly set,
	free(all_test_docs.db);		//		do they need to be freed?
	free(all_test_docs.id);		//TODO: answer this question!
	free(all_test_docs.req.json_data);
	//free(all_test_docs.resp.json_data);
	//TODO: do I need to free offsets? how do I keep track?

	return 0;
}
