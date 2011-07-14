#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
#include "json.h"

int main(int argc, char* argv[]){
	// read in authentication information
	//char *pwdfile_name = "usrpwd";
	//FILE *pwdfile;
	//pwdfile = fopen(pwdfile_name, "r");
	//char usrpwd[10000]; // seems large enough for most usr:pwd combos
	//fgets(usrpwd, 10000, pwdfile);
	//fclose(pwdfile);
	//char *endl;
	//if ( (endl = strchr(usrpwd, '\n')) != NULL){
		//usrpwd[endl-usrpwd] = '\0';
	//}
	//size_t usrpwd_length = strlen(usrpwd)+1;
	
	//define strings for connecting to the database
	char *server = "peterldowns.cloudant.com";
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

	//create a pouch_request* object
	pouch_request *pr = pr_init();

	// set up authentication
	//pr = pr_add_usrpwd(pr, usrpwd, usrpwd_length);

	//get all databases
	pr = get_all_dbs(pr, server);
	pr_do(pr);

	//create new database
	pr = db_create(pr, server, newdb);
	pr_do(pr);

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
	char *fname = "king-tut.JPG";
	pr = doc_add_attachment(pr, server, newdb, fname, fname);
	pr_do(pr);

	//create another attachment
	char *fname2 = "README.md";
	pr = doc_add_attachment(pr, server, newdb, fname2, fname2);
	pr_do(pr);

	//get an attachment
	pr = doc_get_attachment(pr, server, newdb, fname2, fname2);
	pr_do(pr);

	//get revision information
	pr = doc_get_revs(pr, server, newdb, docid);
	pr_do(pr);

	//get current revision
	char *rev = doc_get_cur_rev(pr, server, newdb, docid);
	char buf[strlen(rev)+1]; // must copy revision to a buffer,
							 // because rev points to pr->req.data
							 // (which holds the revision string);
							 // when the request is reused, that
							 // memory is overwritten.
	memcpy(&buf, rev, strlen(rev));
	buf[strlen(rev)] = '\0';

	//get a document
	pr = doc_get(pr, server, newdb, docid);
	pr_do(pr);

	//get a document with a specific revision
	pr = doc_get_rev(pr, server, newdb, docid, buf);
	pr_do(pr);
	
	//head a document
	pr = doc_get_info(pr, server, newdb, docid);
	pr_do(pr);

	//get all docs
	pr = get_all_docs(pr, server, newdb);
	pr_do(pr);
	
	
	printf("Finished creating some documents.\n");
	printf("\tpress enter to delete them.");
	getchar();

	//delete a document
	pr = doc_delete(pr, server, newdb, docid, buf);
	pr_do(pr);

	//get all docs by sequence
	pr = get_all_docs_by_seq(pr, server, newdb);
	pr_do(pr);

	//delete a database
	pr = db_delete(pr, server, newdb);
	pr_do(pr);


//////////////////////////////////////


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
