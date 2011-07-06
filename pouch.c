#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
#include "json.h"
#include "json.c"

int main(int argc, char* argv[]){

	// define strings for connecting to the database
	char *server = "https://peterldowns:2rlz54NeO3@peterldowns.cloudant.com";
	char *newdb = "a_database";
	char *docid = "ff345";


	// create some json data
	JsonNode *json_obj = json_mkobject();
	JsonNode *json_arr = json_mkarray();
	char *key = "C";
	char *val1 = "greatest language ever";
	int val2 = 4;
	json_append_element(json_arr, json_mkstring(val1));
	json_append_element(json_arr, json_mknumber(val2));
	json_append_member(json_obj, key, json_arr);
	json_append_member(json_obj, "first?", json_mkstring("FIRST!!!1111!1!11"));
	char *datastr = json_encode(json_obj);

	// create a pouch_request* object
	pouch_request *pr = pr_init();
	JsonNode *resp;

	// list all of the databases
	pr = get_all_dbs(pr, server);
	resp = json_decode(pr->resp.data);
	getchar();

	// create a new database
	pr = db_create(pr, server, newdb);
	getchar();

	// get info on the new database
	pr = db_get(pr, server, newdb);
	getchar();

	// create a new document
	pr = doc_put(pr, server, newdb, docid, datastr);
	getchar();

	// create a second new document
	pr = doc_post(pr, server, newdb, datastr);
	getchar();

	// get info on the new document
	pr = doc_get_info(pr, server, newdb, docid);
	getchar();

	// get the new document
	pr = doc_get(pr, server, newdb, docid);
	getchar();

	// show all DBs
	pr = get_all_dbs(pr, server);
	getchar();

	// delete the new database
	pr = db_delete(pr, server, newdb);
	getchar();

	// show all DBs
	pr = get_all_dbs(pr, server);

	// cleanup
	pr_free(pr);
	json_delete(resp);
	json_delete(json_arr);
	json_delete(json_obj);
	if(*datastr)
		free(datastr);

	// end
	return 0;
}
