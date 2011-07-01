#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"

int main(int argc, char* argv[]){
	char *server = "http://127.0.0.1:5984";
	char *newdb = "petersdb";
	char *maxrevs = "500000";
	
	// create a pouch_request* object
	// 		to hold request responses
	pouch_request *pr = pouch_init_request();

	// create a new database
	pr = db_create(pr, server, newdb);
	// get info on the new datase
	pr = db_get(pr, server, newdb);
	// show all DBs
	pr = db_get_all(pr, server);
	// delete the new database
	pr = db_delete(pr, server, newdb);
	// show all DBs
	pr = db_get_all(pr, server);

	// cleanup
	pouch_free_request(pr);

	return 0;
}
