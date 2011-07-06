#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
#include "json.h"
#include "json.c"

char *doc_cur_rev(pouch_request *pr, char *server, char *db, char *id){
	/*
	   Returns the current revision of a document
	 */
	pr = doc_get_info(pr, server, db, id);
	pr_do(pr);
	// at this point, pr->resp.data has all of the header stuff.
	char *etag_begin = strchr(pr->resp.data, '\"');
	char *etag_end = strrchr(pr->resp.data, '\"');
	size_t length = (size_t)(etag_end - (etag_begin+1));
	char buf[length+1];
	memcpy(&buf, etag_begin+1, length);
	buf[length] = '\0';
	free(pr->resp.data);
	pr->resp.data = (char *)malloc(length+1);
	memcpy(pr->resp.data, buf, length);
	pr->resp.data[length-1] = '\0';
	return pr->resp.data;
}

int main(int argc, char* argv[]){

	// define strings for connecting to the database
	char *server = "https://peterldowns:2rlz54NeO3@peterldowns.cloudant.com";
	char *newdb = "a_databooohithisispeter";
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
	printf("datastr = %s\n", datastr);

	// create a pouch_request* object
	pouch_request *pr = pr_init();
	JsonNode *resp;

	// list all of the databases
	pr = get_all_dbs(pr, server);
	pr_do(pr);
	resp = json_decode(pr->resp.data);
	getchar();

	// create a new database
	pr = db_create(pr, server, newdb);
	pr_do(pr);
	getchar();

	// get info on the new database
	pr = db_get(pr, server, newdb);
	pr_do(pr);
	getchar();

	// create a new document
	pr = doc_create_id(pr, server, newdb, docid, datastr);
	pr_do(pr);
	getchar();

	// create a second new document
	pr = doc_create(pr, server, newdb, datastr);
	pr_do(pr);
	getchar();

	// get info on the new document
	pr = doc_get_info(pr, server, newdb, docid);
	pr_do(pr);
	getchar();

	// get current revision
	char *rev = doc_cur_rev(pr, server, newdb, docid);
	printf("Current revision: %s\n", rev);
	getchar();
	

	// get the new document
	pr = doc_get(pr, server, newdb, docid);
	pr_do(pr);
	getchar();

	// show all DBs
	pr = get_all_dbs(pr, server);
	pr_do(pr);
	getchar();

	// delete the new database
	pr = db_delete(pr, server, newdb);
	pr_do(pr);
	getchar();

	// show all DBs
	pr = get_all_dbs(pr, server);
	pr_do(pr);

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
