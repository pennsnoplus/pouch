#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"

int main(int argc, char* argv[]){
	// create a request object
	pouch_request *probj = pouch_init_request();

	// setup a PUT request to create a database
	pouch_request_set_method(probj, PUT);
	pouch_request_set_url(probj, "http://127.0.0.1:5984/newdb/");

	// make the request
	pouch_do_request(probj);

	// create a new document within the new database
	pouch_request_set_url(probj, "http://127.0.0.1:5984/newdb/testdoc");
	//TODO: probj = pouch_request_set_data(probj, "{}");

	// make the request
	pouch_do_request(probj);

	// finished making requests; free the request object
	pouch_free_request(probj);

	return 0;
}

pouch_request *pouch_request_add_param(pouch_request *pr, char *key, char *value){
	pr->url = (char *)realloc(pr->url, // 3: new ? or &, new =, new '\0'
			strlen(pr->url) + 3 + sizeof(char)*(strlen(key)+strlen(value)));
	if (strchr(pr->url, "?") == NULL){
		strcat(pr->url, "?");
	}
	else{
		strcat(pr->url, "&");
	}
}//TODO: FINISH THIS FUNCTION
