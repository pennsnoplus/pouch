#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"

int main(int argc, char* argv[]){
	// create a request object
	pouch_request *probj = pouch_init_request();

	pouch_request_set_method(probj, GET);
	pouch_request_set_url(probj, "http://127.0.0.1:5984/test/first");
	pouch_request_add_param(probj, "revs", "true");

	// make the request
	pouch_do_request(probj);

	// add a new param
	pouch_request_clear_params(probj);
	pouch_request_add_param(probj, "revs_info", "true");

	// create a new document within the new database
	//pouch_request_set_url(probj, "http://127.0.0.1:5984/newdb/testdoc");
	//TODO: probj = pouch_request_set_data(probj, "{}");

	// make the request
	pouch_do_request(probj);

	// finished making requests; free the request object
	pouch_free_request(probj);

	return 0;
}
