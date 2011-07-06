#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
#include "json.h"
#include "json.c"

/*
	Database API:
	-------------------------------
	pouch_request *db_get_changes(pouch_request *pr, char *server, char *db,
								  int since, int limit, char *feed, int heartbeat,
								  int timeout, char *filter, char *include_docs)
	pouch_request *db_get(pouch_request *p_req, char *server, char *db)
	pouch_request *db_create(pouch_request *p_req, char *server, char *db);
	pouch_request *db_delete(pouch_request *p_req, char *server, char *db);
	pouch_request *db_get_all(pouch_request *p_req, char *server);
*/

pouch_request * doc_get(pouch_request *pr, char *server, char *db, char *id){
	/*
	   Retrieves a document.
	   */
	/*
TODO: URL escape database and document names (/'s become %2F's)
*/
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	/*
TODO: would it be right/wrong to include a "build_url" method? something like
	void build_url(char *dest, char *serv, char *db, char *id);
	*/
	pr->url = combine(pr->url, db, "/");
	pr->url = combine(pr->url, id, "/");
	pr_do(pr);
	return pr;
	/*
TODO: change every instance of pr_do(pr); return pr; to return pr_do(pr);
	*/
}
pouch_request *doc_get_rev(pouch_request *pr, char *server, char *db, char *id, char *rev){
	/*
	   Get a specific revision of a document.
	   */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, db, "/");
	pr->url = combine(pr->url, id, "/");
	pr_add_param(pr, "rev", rev);
	pr_do(pr);
	return pr;
}
pouch_request *doc_get_all_revs(pouch_request *pr, char *server, char *db, char *id){
	/*
	   Finds out what revisions are available for a document.
	   Returns the current revision of the document, but with
	   an additional field, _revisions, the value being a list
	   of the available revision IDs.
	 */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, db, "/");
	pr->url = combine(pr->url, id, "/");
	pr_add_param(pr, "revs", "true");
	pr_do(pr);
	return pr;
}
pouch_request *doc_get_info(pouch_request *pr, char *server, char *db, char *id){
	/*
	   A HEAD request returns basic information about the document, including its current revision.
	   */
	/*
TODO: figure out why HEAD requests time out without a response from the couchDB server
*/
	pr_set_method(pr, HEAD);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, db, "/");
	pr->url = combine(pr->url, id, "/");
	printf("HEAD to %s\n", pr->url);
	pr_do(pr);
	return pr;
}
pouch_request *doc_put(pouch_request *pr, char *server, char *db, char *id, char *data){
	/*
	   Creates a new document with an automatically generated
	   revision ID. The JSON body must include a _id property
	   which contains a unique id. If the document already exists,
	   and the JSON data body includes a _rev property, then
	   the document is updated.
	   */
	pr_set_method(pr, PUT);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, db, "/");
	pr->url = combine(pr->url, id, "/");
	pr_set_data(pr, data);
	pr_do(pr);
	return pr;
}
pouch_request *doc_post(pouch_request *pr, char *server, char *db, char *data){
	/*
	   Creates a new document with a server generated DocID.
	   */
	pr_set_method(pr, POST);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, db, "/");
	pr_set_data(pr, data);
	pr_do(pr);
	return pr;
}
pouch_request *doc_get_all(pouch_request *pr, char *server, char *db){
	/*
	   Returns all of the docs in a database.
	   */
	pr_set_method(pr, GET);
	pr_set_url(pr, server);
	pr->url = combine(pr->url, db, "/");
	pr->url = combine(pr->url, "_all_docs", "/");
	pr_do(pr);
	return pr;
}

int main(int argc, char* argv[]){
	/*
TODO: change all wrappers so that instead of performing the
	  request, they just return an initialized object. This
	  lets the user add custom headers whenever they want,
	  or set specific options or parameters.
	  */

	// define strings for connecting to the database
	char *server = "https://peterldowns:2rlz54NeO3@peterldowns.cloudant.com";
	char *newdb = "a_database";
	char *docid = "ff345";

	
	// create some json data
	JsonNode *json_obj = json_mkobject();
	JsonNode *json_arr = json_mkarray();
	char *key = "C";
	char *val1 = "one hell of a language";
	int val2 = 4;
	json_append_element(json_arr, json_mkstring(val1));
	json_append_element(json_arr, json_mknumber(val2));
	json_append_member(json_obj, key, json_arr);
	char *datastr = json_encode(json_obj);
	printf("JSON data: %s\n", datastr);
	
	// create a pouch_request* object
	pouch_request *pr = pr_init();
	JsonNode *resp;

	// list all of the databases
	pr = db_get_all(pr, server);
	resp = json_decode(pr->resp.data);

	// create a new database
	pr = db_create(pr, server, newdb);
	
	// get info on the new database
	pr = db_get(pr, server, newdb);

	// create a new document
	pr = doc_put(pr, server, newdb, docid, datastr);

	// create a second new document
	pr = doc_post(pr, server, newdb, datastr);

	// get info on the new document
	pr = doc_get_info(pr, server, newdb, docid);

	// get the new document
	pr = doc_get(pr, server, newdb, docid);

	// show all DBs
	pr = db_get_all(pr, server);

	// delete the new database
	//pr = db_delete(pr, server, newdb);
	
	// show all DBs
	//pr = db_get_all(pr, server);

	// cleanup
	pr_free(pr);

	// end
	return 0;
}
