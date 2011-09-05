#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../src/pouch.h"
#include "lib/json.h"

int main(int argc, char* argv[]){
	//define strings for connecting to the database
	char *server = "snoplus.cloudant.com";
	char *newdb = "example_db";
	char *docid = "firstdoc";

	//create some json data
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
	json_append_element(json_arr2, json_mkstring("incredibly cool"));
	json_append_element(json_arr2, json_mkstring("wicked awesome"));
	json_append_element(json_arr2, json_mkstring("the bees knees"));
	json_append_member(json_obj, key, json_arr);
	json_append_member(json_obj, "first?", json_mkstring("FIRST!!!1111!1!11"));
	json_append_member(json_obj, "HEP", json_arr2);
	char *datastr = json_encode(json_obj);

	//create a PouchReq* object
	PouchReq *pr = pr_init();

	/*
	 * set up authentication by reading from a file
	 * - you may want to do this if you're connecting
	 * to a private server but also publishing your project
	 * somewhere online; this way, your user/pass combination
	 * isn't stored in source, but can still be used.
	 */
	
	/*
	// read in authentication information
	char *pwdfile_name = "files/usrpwd";
	FILE *pwdfile;
	pwdfile = fopen(pwdfile_name, "r");
	char usrpwd[10000]; // seems large enough for most usr:pwd combos
	fgets(usrpwd, 10000, pwdfile);
	fclose(pwdfile);
	char *endl;
	if ( (endl = strchr(usrpwd, '\n')) != NULL){
		usrpwd[endl-usrpwd] = '\0';
	}
	size_t usrpwd_length = strlen(usrpwd)+1;
	pr = pr_add_usrpwd(pr, usrpwd, usrpwd_length);
	*/

	// set up authentication by adding a string from code
	char *usrpwd = "snoplus:snoplustest";
	pr = pr_add_usrpwd(pr, usrpwd, strlen(usrpwd)+1);

	//get all databases
	pr = get_all_dbs(pr, server);
	pr_do(pr);
	
	// Print out all the databases
	JsonNode *all_dbs = json_decode(pr->resp.data);
	int has_our_db = 0;
	printf("Databases on %s\n", server);
	JsonNode *db;

	json_foreach(db, all_dbs){
		printf("\t%s\n", json_get_string(db));
		if(!strcmp(json_get_string(db), newdb))
			has_our_db = 1;
	}
	json_delete(db);
	json_delete(all_dbs);

	if (!has_our_db){
		//create new database
		printf("Creating new database %s\n", newdb);
		pr = db_create(pr, server, newdb);
		pr_do(pr);
	}

	//get db revs limit
	pr = db_get_revs_limit(pr, server, newdb);
	pr_do(pr);

	//set db revs limit
	pr = db_set_revs_limit(pr, server, newdb, "123456");
	pr_do(pr);

	//compact the database
	pr = db_compact(pr, server, newdb);
	pr_do(pr);

	//get db changes
	pr = db_get_changes(pr, server, newdb);
	pr_do(pr);

	//get db information
	pr = db_get(pr, server, newdb);
	pr_do(pr);

	//create a new doc without an id
	pr = doc_create(pr, server, newdb, datastr);
	pr_do(pr);

	//create a new doc with an id
	pr = doc_create_id(pr, server, newdb, docid, datastr);
	pr_do(pr);

	//copy a doc to a new id
	pr = doc_copy(pr, server, newdb, docid, "seconddoc", NULL);
	pr_do(pr);

	//create a new attachment
	char *fname = "files/king-tut.JPG";
	pr = doc_add_attachment(pr, server, newdb, "king-tut.JPG", fname);
	pr_do(pr);

	//create another attachment
	char *fname2 = "files/README.md";
	pr = doc_add_attachment(pr, server, newdb, "README.md", fname2);
	pr_do(pr);

	//get an attachment
	pr = doc_get_attachment(pr, server, newdb, fname2, fname2);
	pr_do(pr);

	//get revision information
	pr = doc_get_revs(pr, server, newdb, docid);
	pr_do(pr);

	//get current revision
	char *rev;
	rev = doc_get_cur_rev(pr, server, newdb, docid);
	printf("Current revision of %s/%s is %s\n", newdb, docid, rev);
	
	//get a document with a specific revision
	pr = doc_get_rev(pr, server, newdb, docid, rev);
	pr_do(pr);


	//get a document
	pr = doc_get(pr, server, newdb, docid);
	pr_do(pr);

	
	//head a document
	pr = doc_get_info(pr, server, newdb, docid);
	pr_do(pr);

	//get all docs
	pr = get_all_docs(pr, server, newdb);
	pr_do(pr);
	
	
	printf("Finished creating some documents.\n");
	printf("\tYou may go to %s/_utils/database.html?%s to see them using Futon.\n", server, newdb);
	printf("\tWhen you're ready, press enter to delete them.");
	getchar();

	//delete a document
	pr = doc_delete(pr, server, newdb, docid, rev);
	pr_do(pr);
	
	free(rev); // all done with this revision

	// Delete all the documents on newdb
	pr = get_all_docs(pr, server, newdb);
	pr_do(pr);

	JsonNode *response = json_decode(pr->resp.data);
	JsonNode *all_docs = json_find_member(response, "rows");
	printf("Deleting all docs on %s/%s\n", server, newdb);
	JsonNode *doc, *del_resp;
	char *_rev;
	json_foreach(doc, all_docs){
		char *id = json_get_string(json_find_member(doc, "id")); // get the doc's id
		
		_rev = doc_get_cur_rev(pr, server, newdb, id); // get the doc's revision
		
		pr = doc_delete(pr, server, newdb, id, _rev); // delete the doc
		pr_do(pr);

		del_resp = json_decode(pr->resp.data); // check the response
		bool ok = json_get_bool(json_find_member(del_resp, "ok"));
		if (ok)
			printf("\tDeleted \"%s\"\t(rev= %s )\n", id, _rev);
		else {
			printf("Unable to delete \"%s\"\t(rev= %s )\n", id, _rev);
			printf("server returned: %s\n", pr->resp.data);
		}
		// clean up
		json_delete(del_resp);
		free(_rev);
	}

	json_delete(doc);
	json_delete(response);

	//cleanup
	pr_free(pr);
	json_delete(json_arr);
	json_delete(json_arr2);
	json_delete(json_obj);
	if(*datastr)
		free(datastr);
	
	printf("Done.\n");
	return 0;
}
