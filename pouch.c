#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
#include "json.h"
#include "json.c"

int main(int argc, char* argv[]){

	// define strings for connecting to the database
	char *server = "https://peterldowns:2rlz54NeO3@peterldowns.cloudant.com";
	char *newdb = "example_db";
	char *docid = "firstdoc";

	// create some json data
	JsonNode *json_obj = json_mkobject();
	JsonNode *json_arr = json_mkarray();
	JsonNode *json_arr2 = json_mkarray();
	char *key = "C";
	char *val1 = "greatest language ever";
	int val2 = 4;
	char *val3 = "what's up?";
	json_append_element(json_arr, json_mkstring(val1));
	json_append_element(json_arr, json_mknumber(val2));
	json_append_element(json_arr, json_mkstring(val3));
	json_append_element(json_arr2, json_mkstring("hi"));
	json_append_element(json_arr2, json_mkbool(0));
	json_append_member(json_obj, key, json_arr);
	json_append_member(json_obj, "first?", json_mkstring("FIRST!!!1111!1!11"));
	json_append_member(json_obj, "HEP", json_arr2);
	char *datastr = json_encode(json_obj);
	printf("datastr = %s\n", datastr);

	// create a pouch_request* object
	pouch_request *pr = pr_init();

	// get all databases
	pr = get_all_dbs(pr, server);
	pr_do(pr);
	getchar();

	// create new database
	pr = db_create(pr, server, newdb);
	pr_do(pr);
	getchar();

	// get db revs limit
	pr = db_get_revs_limit(pr, server, newdb);
	pr_do(pr);
	getchar();

	// set db revs limit
	pr = db_set_revs_limit(pr, server, newdb, "123456");
	pr_do(pr);
	getchar();

	// compact the database
	pr = db_compact(pr, server, newdb);
	pr_do(pr);
	getchar();

	// get db changes
	pr = db_get_changes(pr, server, newdb);
	pr_do(pr);
	getchar();

	// get db information
	pr = db_get(pr, server, newdb);
	pr_do(pr);
	getchar();

	// create a new doc without an id
	pr = doc_create(pr, server, newdb, datastr);
	pr_do(pr);
	getchar();

	// create a new doc with an id
	pr = doc_create_id(pr, server, newdb, docid, datastr);
	pr_do(pr);
	getchar();

	// get revision information
	pr = doc_get_revs(pr, server, newdb, docid);
	pr_do(pr);
	getchar();

	// get current revision
	char *rev = doc_cur_rev(pr, server, newdb, docid);
	
	char buf[strlen(rev)+1];
	memcpy(&buf, rev, strlen(rev));
	buf[strlen(rev)] = '\0';
	printf("Current revision: %s\n", buf);
	getchar();

	// get a document
	pr = doc_get(pr, server, newdb, docid);
	pr_do(pr);
	getchar();

	// get a document with a specific revision
	pr = doc_get_rev(pr, server, newdb, docid, buf);
	pr_do(pr);
	getchar();
	
	// head a document
	pr = doc_get_info(pr, server, newdb, docid);
	pr_do(pr);
	getchar();

	// get all docs
	pr = get_all_docs(pr, server, newdb);
	pr_do(pr);
	getchar();

	// delete a document
	pr = doc_delete(pr, server, newdb, docid, buf);
	pr_do(pr);
	getchar();

	// get all docs by sequence
	pr = get_all_docs_by_seq(pr, server, newdb);
	pr_do(pr);
	getchar();

	// delete a database
	pr = db_delete(pr, server, newdb);
	pr_do(pr);
	getchar();


//////////////////////////////////////


	// cleanup
	pr_free(pr);
	json_delete(json_arr);
	json_delete(json_arr2);
	json_delete(json_obj);
	if(*datastr)
		free(datastr);

	// end
	return 0;
}
