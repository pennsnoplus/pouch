#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pouch.h"
#include "json.h"

int main(int argc, char* argv[]){
	printf("define strings for connecting to the database\n");
	char *server = "https://peterldowns:2rlz54NeO3@peterldowns.cloudant.com";
	char *newdb = "example_db";
	char *docid = "firstdoc";

	printf("create some json data\n");
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

	printf("create a pouch_request* object\n");
	pouch_request *pr = pr_init();

	printf("get all databases\n");
	pr = get_all_dbs(pr, server);
	pr_do(pr);

	printf("create new database\n");
	pr = db_create(pr, server, newdb);
	pr_do(pr);

	printf("get db revs limit\n");
	pr = db_get_revs_limit(pr, server, newdb);
	pr_do(pr);

	printf("set db revs limit\n");
	pr = db_set_revs_limit(pr, server, newdb, "123456");
	pr_do(pr);

	printf("compact the database\n");
	pr = db_compact(pr, server, newdb);
	pr_do(pr);

	printf("get db changes\n");
	pr = db_get_changes(pr, server, newdb);
	pr_do(pr);

	printf("get db information\n");
	pr = db_get(pr, server, newdb);
	pr_do(pr);

	printf("create a new doc without an id\n");
	pr = doc_create(pr, server, newdb, datastr);
	pr_do(pr);

	printf("create a new doc with an id\n");
	pr = doc_create_id(pr, server, newdb, docid, datastr);
	pr_do(pr);

	printf("copy a doc to a new id\n");
	pr = doc_copy(pr, server, newdb, docid, "seconddoc", NULL);
	pr_do(pr);

	printf("create a new attachment\n");
	char *fname = "king-tut.jpg";
	pr = doc_add_attachment(pr, server, newdb, fname, fname);
	pr_do(pr);

	printf("create another attachment\n");
	char *fname2 = "README.md";
	pr = doc_add_attachment(pr, server, newdb, fname2, fname2);
	pr_do(pr);

	printf("get an attachment\n");
	pr = doc_get_attachment(pr, server, newdb, fname2, fname2);
	pr_do(pr);

	printf("get revision information\n");
	pr = doc_get_revs(pr, server, newdb, docid);
	pr_do(pr);

	printf("get current revision\n");
	char *rev = doc_get_cur_rev(pr, server, newdb, docid);
	
	char buf[strlen(rev)+1];
	memcpy(&buf, rev, strlen(rev));
	buf[strlen(rev)] = '\0';

	printf("get a document\n");
	pr = doc_get(pr, server, newdb, docid);
	pr_do(pr);

	printf("get a document with a specific revision\n");
	pr = doc_get_rev(pr, server, newdb, docid, buf);
	pr_do(pr);
	
	printf("head a document\n");
	pr = doc_get_info(pr, server, newdb, docid);
	pr_do(pr);

	printf("get all docs\n");
	pr = get_all_docs(pr, server, newdb);
	pr_do(pr);
	getchar();

	printf("delete a document\n");
	pr = doc_delete(pr, server, newdb, docid, buf);
	pr_do(pr);

	printf("get all docs by sequence\n");
	pr = get_all_docs_by_seq(pr, server, newdb);
	pr_do(pr);

	printf("delete a database\n");
	pr = db_delete(pr, server, newdb);
	pr_do(pr);


//////////////////////////////////////


	printf("cleanup\n");
	pr_free(pr);
	json_delete(json_arr);
	json_delete(json_arr2);
	json_delete(json_obj);
	if(*datastr)
		free(datastr);

	printf("end\n");
	return 0;
}
