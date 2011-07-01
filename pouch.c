#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
/*
	Database API:
	-------------------------------
	pouch_request *db_get_changes(pouch_request *pr, char *server, char *name,
								  int since, int limit, char *feed, int heartbeat,
								  int timeout, char *filter, char *include_docs)
	pouch_request *db_get(pouch_request *p_req, char *server, char *name)
	pouch_request *db_create(pouch_request *p_req, char *server, char *name);
	pouch_request *db_delete(pouch_request *p_req, char *server, char *name);
	pouch_request *db_get_all(pouch_request *p_req, char *server);
*/

int main(int argc, char* argv[]){
	char *server = "http://127.0.0.1:5984";
	char *newdb = "petersdb";
	char *maxrevs = "500000";
	
	// create a pouch_request* object
	// 		to hold request responses
	pouch_request *pr = pr_init();

	// create a new database
	pr = db_create(pr, server, newdb);
	// get info on the new datase
	pr = db_get(pr, server, newdb);
	// show changes
	pr = db_get_changes(pr, server, newdb, 0, (int)NULL, "normal", 60000, 60000, (char *)NULL, "false");
	// show all DBs
	pr = db_get_all(pr, server);
	// delete the new database
	pr = db_delete(pr, server, newdb);
	// show all DBs
	pr = db_get_all(pr, server);

	// cleanup
	pr_free(pr);

	// end
	return 0;
}
