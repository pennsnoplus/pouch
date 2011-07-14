#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <curl/curl.h>

#include "pouch.h"

char *url_escape(CURL *curl, char *str){
    /*
       URL escapes a string. Use this to 
       escape database names.
     */
    return curl_easy_escape(curl, str, strlen(str));
}
char *combine(char **out, char *f, char *s, char *sep){
    /*
       Appends the strings f, sep, and s, in that order,
       and copies the result to *out. If a separator is
       unnecessary, call the function with sep=NULL. *out
       must have been malloced() or initiated as NULL.
     */
    size_t length = 0;
    length += strlen(f);
    length += strlen(s);
    if(sep)
        length += strlen(sep);
    length++; // must have room for terminating \0
    char buf[length];
    if(sep)
        sprintf(buf, "%s%s%s", f, sep, s);
    else
        sprintf(buf, "%s%s", f, s);
    buf[length-1] = '\0'; // null terminate
    if(*out){
        free(*out);
    }
    *out = (char *)malloc(length);
    memcpy(*out, buf, length);
    return *out;
}

pouch_request *pr_add_header(pouch_request *pr, char *h){
    /*
       Add a custom header to a request.
     */
    pr->headers = curl_slist_append(pr->headers, h);
    return pr;
}

pouch_request *pr_init(void){
    /*
       Initializes a new pouch_request
       object.
     */
    pouch_request *pr = calloc(1, sizeof(pouch_request));

    // initializes the request buffer
    pr->req.offset = pr->req.data = NULL;
    pr->req.size = 0;

    // initializes the response buffer
    pr->resp.offset = pr->resp.data = NULL;
    pr->resp.size = 0;

    return pr;
}

pouch_request *pr_set_method(pouch_request *pr, char *method){
    /*
       Sets the HTTP method of
       a specific request.
     */
    size_t length = strlen(method)+1; // include '\0' terminator
    if (pr->method)
        free(pr->method);
    pr->method = (char *)malloc(length); // allocate space
    memcpy(pr->method, method, length);	 // copy the method
    return pr;
}
pouch_request *pr_set_url(pouch_request *pr, char *url){
    /*
       Sets the target URL of
       a CouchDB request.
     */
    size_t length = strlen(url)+1; // include '\0' terminator
    if (pr->url)	// if there is an older url, get rid of it
        free(pr->url);
    pr->url = (char *)malloc(length); // allocate space
    memcpy(pr->url, url, length);	  // copy the new url

    return pr;
}
pouch_request *pr_set_data(pouch_request *pr, char *str){
    /*
       Sets the data that a request
       sends. If the request does not
       need to send data, do NOT call
       this function with an empty string,
       just refrain from calling the function.
     */
    size_t length = strlen(str);
    if (pr->req.data){	// free older data
        free(pr->req.data);
    }
    pr->req.data = (char *)malloc(length+1);	// allocate space, include '\0'
    memset(pr->req.data, '\0', length+1);		// write nulls to the new space
    memcpy(pr->req.data, str, length);	// copy over the data

    // Because of the way CURL sends data,
    // before sending the pouch_pkt's
    // offset must point to the same address
    // in memory as the data pointer does.
    pr->req.offset = pr->req.data;
    pr->req.size = length; // do not send the last '\0' - JSON is not null terminated
    return pr;
}
pouch_request *pr_set_bdata(pouch_request *pr, void *dat, size_t length){
    if (pr->req.data){
        free(pr->req.data);
    }
    pr->req.data = (char *)malloc(length);
    memcpy(pr->req.data, dat, length);
    pr->req.offset = pr->req.data;
    pr->req.size = length;
    return pr;
}

void pr_free(pouch_request *pr){
    /*
       Frees any memory that may have been
       allocated during the creation / 
       processing of a request. Although it
       is ok to reuse requests, this
       MUST be called at the end of your program
       in order to not leak memory like Assange
       leaks secret documents.
     */
    if (pr->resp.data){			// free response
        free(pr->resp.data);
    }if (pr->req.data){
        free(pr->req.data);		// free request
    }if (pr->method){			// free method string
        free(pr->method);
    }if (pr->url){				// free URL string
        free(pr->url);
    }if (pr->headers){
        curl_slist_free_all(pr->headers);	// free headers
    }if (pr->usrpwd){
        free(pr->usrpwd);
    }
    free(pr);				// free structure
}

pouch_request *pr_clear_data(pouch_request *pr){
    /*
       Removes all data from a request's
       data buffer, if it exists.
     */
    if (pr->req.data){
        free(pr->req.data);
        pr->req.data = NULL;
    }
    pr->req.size = 0;
    return pr;
}
size_t recv_data_callback(char *ptr, size_t size, size_t nmemb, void *data){
    /*
       This callback is used to save responses from CURL requests.
       It loads the response into the pouch_request pointed to by
       data.
     */
    size_t ptrsize = nmemb*size; // this is the size of the data pointed to by ptr
    pouch_request *pr = (pouch_request *)data;
    pr->resp.data = (char *)realloc(pr->resp.data, pr->resp.size + ptrsize +1);
    if (pr->resp.data){	// realloc was successful
        memcpy(&(pr->resp.data[pr->resp.size]), ptr, ptrsize); // append new data
        pr->resp.size += ptrsize;
        pr->resp.data[pr->resp.size] = '\0'; // null terminate the new data
    }
    else { // realloc was NOT successful
        fprintf(stderr, "recv_data_callback: realloc failed\n");
    }
    return ptrsize; // theoretically, this is the amount of processed data
}

size_t send_data_callback(void *ptr, size_t size, size_t nmemb, void *data){
    /*
       This callback is used to send data for a CURL request. The JSON
       string stored in the pouch_request pointed to by data is read out
       and sent, piece by piece.
     */
    size_t maxcopysize = nmemb*size;
    if (maxcopysize < 1){
        return 0;
    }
    pouch_request *pr = (pouch_request *)data;
    if (pr->req.size > 0){ // only send data if there's data to send
        size_t tocopy = (pr->req.size > maxcopysize) ? maxcopysize : pr->req.size;
        memcpy(ptr, pr->req.offset, tocopy);
        pr->req.offset += tocopy;	// advance our offset by the number of bytes already sent
        pr->req.size -= tocopy;	//next time there are tocopy fewer bytes to copy
        return tocopy;
    }
    return 0;
}

pouch_request *pr_do(pouch_request *pr){
    CURL *curl;	// CURL object to make the requests
    //pr->headers= NULL;	// Custom headers for uploading

    // empty the response buffer
    if (pr->resp.data){
        free(pr->resp.data);
    }
    pr->resp.data = NULL;
    pr->resp.size = 0;

    // initialize the CURL object
    curl = curl_easy_init();
    if (curl){
        // Print the request
        //printf("%s : %s\n", pr->method, pr->url);

        // setup the CURL object/request
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "pouch/0.1");				// add user-agent
        curl_easy_setopt(curl, CURLOPT_URL, pr->url);						// where to send this request
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2);					// Timeouts
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_callback);	// where to store the response
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pr);
        if(pr->usrpwd){	// if there's a valid auth string, use it
            curl_easy_setopt(curl, CURLOPT_USERPWD, pr->usrpwd);
        }

        if (pr->req.data && pr->req.size > 0){ // check for data upload
            //printf("--> %s\n", pr->req.data);
            // let CURL know what data to send
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, send_data_callback);
            curl_easy_setopt(curl, CURLOPT_READDATA, (void *)pr);
        }

        if (!strncmp(pr->method, PUT, 3)){ // PUT-specific option
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
            // Note: Content-Type: application/json is automatically assumed
        }
        else if (!strncmp(pr->method, POST, 4)){ // POST-specific options
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            pr_add_header(pr, "Content-Type: application/json");
        }

        if (!strncmp(pr->method, HEAD, 4)){ // HEAD-specific options
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            curl_easy_setopt(curl, CURLOPT_HEADER, 1);
        }
        else {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, pr->method);
        } // THIS FIXED HEAD REQUESTS

        // add the custom headers
        pr_add_header(pr, "Transfer-Encoding: chunked");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pr->headers);

        // make the request and store the response
        pr->curlcode = curl_easy_perform(curl);
    }
    else{
        // if we were unable to initialize a CURL object
        pr->curlcode = 2;
    }
    // clean up
    if (pr->headers){
        curl_slist_free_all(pr->headers);	// free headers
        pr->headers = NULL;
    }
    if (!pr->curlcode){
        pr->curlcode = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &pr->httpresponse);
        if (pr->curlcode != CURLE_OK)
            pr->httpresponse = 500;
    }
    curl_easy_cleanup(curl);		// clean up the curl object

    // Print the response
    //printf("Received %d bytes, status = %d\n",
    //		(int)pr->resp.size, pr->curlcode);
    //printf("--> %s\n", pr->resp.data);
    return pr;
}

pouch_request *pr_add_param(pouch_request *pr, char *key, char *value){
    /*
       Adds a parameter to a request's URL string,
       regardless of whether or not other parameters already exist.
     */
    pr->url = (char *)realloc(pr->url, // 3: new ? or &, new =, new '\0'
            strlen(pr->url) + 3 + sizeof(char)*(strlen(key)+strlen(value)));
    if (strchr(pr->url, '?') == NULL){
        strcat(pr->url, "?");
    }
    else{
        strcat(pr->url, "&");
    }
    strcat(pr->url, key);
    strcat(pr->url, "=");
    strcat(pr->url, value);
    strcat(pr->url, "\0");
    return pr;
}
pouch_request *pr_clear_params(pouch_request *pr){
    /*
       Removes all parameters from a request's URL string,
       if they exist. Otherwise, the URL string is left alone.
     */
    char *div;
    if ( (div = strchr(pr->url, '?')) != NULL){ // if there are any params
        char *temp = &pr->url[strlen(pr->url)]; // end of the string
        while (*temp != '?'){
            *temp = '\0'; // wipe out the old character
            temp--;	// move back another character
        }
        *temp = '\0'; // get rid of the ?
    }
    return pr;
}


pouch_request *get_all_dbs(pouch_request *p_req, char *server){
    /*
       Return a list of all databases on a
       CouchDB server.
     */
    pr_set_method(p_req, GET);
    pr_set_url(p_req, server);
    p_req->url = combine(&(p_req->url), p_req->url, "_all_dbs", "/");
    return p_req;
}

pouch_request *db_delete(pouch_request *p_req, char *server, char *db){
    /*
       Delete the database /db/ on the CouchDB
       server /server/
     */
    pr_set_method(p_req, DELETE);
    pr_set_url(p_req, server);
    p_req->url = combine(&(p_req->url), p_req->url, db, "/");
    return p_req;
}

pouch_request *db_create(pouch_request *p_req, char *server, char *db){
    /*
       Create the database /db/ on the CouchDB
       server /server/
     */
    pr_set_method(p_req, PUT);
    pr_set_url(p_req, server);
    p_req->url = combine(&(p_req->url), p_req->url, db, "/");
    return p_req;
}

pouch_request *db_get(pouch_request *p_req, char *server, char *db){
    /*
       Get information about the database /db/
       on the CouchDB server /server/
     */
    pr_set_method(p_req, GET);
    pr_set_url(p_req, server);
    p_req->url = combine(&(p_req->url), p_req->url, db, "/");
    return p_req;
}
pouch_request *db_get_changes(pouch_request *pr, char *server, char *db){
    /*
       Return a list of changes to a document
       in a CouchDB database. Add custom params with
       pr_add_param();
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, "_changes", "/");
    return pr;
}
pouch_request *db_get_revs_limit(pouch_request *pr, char *server, char *db){
    /*
       Returns the current maximum number of revisions
       allowed for a database.
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, "_revs_limit", "/");
    return pr;
}
pouch_request *db_set_revs_limit(pouch_request *pr, char *server, char *db, char *revs){
    /*
       Sets the maximum number of revisions a database
       is allowed to have.
     */
    pr_set_method(pr, PUT);
    pr_set_data(pr, revs);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, "_revs_limit", "/");
    return pr;
}
pouch_request *db_compact(pouch_request *pr, char *server, char *db){
    /*
       Initiates compaction on a database.
     */
    pr_set_method(pr, POST);
    pr_set_url(pr, server);
    pr_set_data(pr, "{}");
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, "_compact", "/");
    return pr;
}

pouch_request * doc_get(pouch_request *pr, char *server, char *db, char *id){
    /*
       Retrieves a document.
     */
    /*
TODO: URL escape database and document names (/'s become %2F's)
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    return pr;
}
pouch_request *doc_get_rev(pouch_request *pr, char *server, char *db, char *id, char *rev){
    /*
       Get a specific revision of a document.
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    pr_add_param(pr, "rev", rev);
    return pr;
}
pouch_request *doc_get_revs(pouch_request *pr, char *server, char *db, char *id){
    /*
       Finds out what revisions are available for a document.
       Returns the current revision of the document, but with
       an additional field, _revisions, the value being a list
       of the available revision IDs.
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    pr_add_param(pr, "revs", "true");
    return pr;
}
pouch_request *doc_get_info(pouch_request *pr, char *server, char *db, char *id){
    /*
       A HEAD request returns basic information about the document, including its current revision.
     */
    pr_set_method(pr, HEAD);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    return pr;
}
pouch_request *doc_create_id(pouch_request *pr, char *server, char *db, char *id, char *data){
    /*
       Creates a new document with an automatically generated
       revision ID. The JSON body must include a _id property
       which contains a unique id. If the document already exists,
       and the JSON data body includes a _rev property, then
       the document is updated.
     */
    pr_set_method(pr, PUT);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    pr_set_data(pr, data);
    return pr;
}
pouch_request *doc_create(pouch_request *pr, char *server, char *db, char *data){
    /*
       Creates a new document with a server generated DocID.
     */
    pr_set_method(pr, POST);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr_set_data(pr, data);
    return pr;
}
pouch_request *get_all_docs(pouch_request *pr, char *server, char *db){
    /*
       Returns all of the docs in a database.
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, "_all_docs", "/");
    return pr;
}
pouch_request *get_all_docs_by_seq(pouch_request *pr, char *server, char *db){
    /*
       Returns all the documents that have been updated or deleted, in the
       order that they were modified.
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, "_all_docs_by_seq", "/");
    return pr;
}
pouch_request *doc_get_attachment(pouch_request *pr, char *server, char *db, char *id, char *name){
    /*
       Gets an attachment on a document.
     */
    pr_set_method(pr, GET);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    pr->url = combine(&(pr->url), pr->url, name, "/");
    return pr;
}
pouch_request *doc_copy(pouch_request *pr, char *server, char *db, char *id, char *newid, char *revision){
    /*
       Copies a document from one id to another,
       all server side.
     */
    pr_set_method(pr, COPY);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    // TODO: add support for document overwrite on copy
    char *headerstr = NULL;
    headerstr = combine(&headerstr, "Destination: ", newid, NULL);
    if (revision != NULL) {
        headerstr = combine(&headerstr, headerstr, revision, "?rev=");
    }
    pr_add_header(pr, headerstr);
    free(headerstr);
    return pr;
}
pouch_request *doc_delete(pouch_request *pr, char *server, char *db, char *id, char *rev){
    /*
       Delete a document and all of its attachments.
       Must include the revision of the document you
       want to delete.
     */
    pr_set_method(pr, DELETE);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, id, "/");
    pr_add_param(pr, "rev", rev);
    return pr;
}
char *doc_get_cur_rev(pouch_request *pr, char *server, char *db, char *id){
    /*
       Stores the current revision of the document in pr->resp.data.
       If you want to do anything with that revision string, make sure
       to copy it to another place in memory before reusing the request.
     */
    pr = doc_get_info(pr, server, db, id);
    pr_do(pr);
    // at this point, pr->resp.data has all of the header stuff.
    char *etag_begin = strchr(pr->resp.data, '\"');
    char *etag_end = strrchr(pr->resp.data, '\"');
    size_t length = (size_t)(etag_end - etag_begin)-1;
    char buf[length+1];
    memcpy(&buf, etag_begin+1, length);
    buf[length] = '\0';
    free(pr->resp.data);
    pr->resp.data = (char *)malloc(length+1);
    memcpy(pr->resp.data, buf, length);
    pr->resp.data[length] = '\0';
    return pr->resp.data;
}
pouch_request *doc_add_attachment(pouch_request *pr, char *server, char *db, char *doc, char *filename){
    /*
       Given a filename, try to read that file and upload it as an attachment to a document.
     */
    // load the file into memory
    struct stat file_info;
    int fd = open(filename, O_RDONLY);
    if (!fd) {
        fprintf(stderr, "doc_upload_attachment: could not open file %s\n", filename);
    }
    if (lstat(filename, &file_info) != 0){
        fprintf(stderr, "doc_upload_attachment: could not lstat file %s\n", filename);
        return pr;
        // TODO: include an "error" integer in each pouch_request, to be set
        //		 by different wrapper functions
    }
    // read file into buffer
    size_t fd_len = file_info.st_size;
    char fd_buf[fd_len];
    int numbytes = read(fd, fd_buf, fd_len);
    pr_set_bdata(pr, (void *)fd_buf, fd_len);
    close(fd);
    // just in case the actual mime-type is weird or broken, add a default
    // mime-type of application/octet-stream, which is used for binary files.
    // this way, even if something goes horribly wrong, we'll be able to download
    // and view the data we've uploaded.
    pr = pr_add_header(pr, "Content-Type: application/octet-stream");
    // get mime type
    if(USE_SYS_FILE){
        FILE *comres;
        char combuf[strlen("file --mime-type ")+strlen(filename)+1];
        sprintf(combuf, "file --mime-type %s", filename);
        comres = popen(combuf, "r");
        char comdet[10000];
        fgets(comdet, 10000, comres);
        fclose(comres);
        // store the mime type to a buffer
        char *mtype;
        if ( (mtype = strchr(comdet, ' ')) == NULL){
            fprintf(stderr, "could not get mimetype\n");
        }
        mtype++;
        char *endmtype;
        if ( (endmtype = strchr(mtype, '\n')) == NULL){
            fprintf(stderr, "could not get end of mimetype\n");
        }
        char ct[strlen("Content-Type: ")+(endmtype-mtype)+1];
        snprintf(ct, strlen("Content-Type: ")+(size_t)(endmtype-mtype)+1, "Content-Type: %s", mtype);
        // add the actual mime-type
        pr = pr_add_header(pr, ct);
    }
    else {
        char *dot;
        if  ( (dot = strchr(filename, '.')) != NULL){ // if null, then binary file
            char lowercase[strlen(dot)+1];
            strcpy(lowercase, dot);
            int i;
            for(i = 0; i < strlen(dot); i++){
                lowercase[i] = tolower(lowercase[i]);
            }
            if(!strcmp(lowercase, ".jpg") || !strcmp(lowercase, ".jpeg")){
                pr_add_header(pr, "Content-Type: image/jpeg");
            }
            else if(!strcmp(lowercase, ".png")){
                pr_add_header(pr, "Content-Type: image/png");
            }
            else if(!strcmp(lowercase, ".gif")){
                pr_add_header(pr, "Content-Type: image/gif");
            }
            else if(!strcmp(lowercase, ".tif")){
                pr_add_header(pr, "Content-Type: image/tiff");
            }
            else if(!strcmp(lowercase, ".c")
                    || !strcmp(lowercase, ".h")
                    || !strcmp(lowercase, ".cpp")
                    || !strcmp(lowercase, ".cxx")
                    || !strcmp(lowercase, ".py")
                    || !strcmp(lowercase, ".md")
                    || !strcmp(lowercase, ".text")
                    || !strcmp(lowercase, ".txt")){
                pr_add_header(pr, "Content-Type: text/plain");
            }
            else if(!strcmp(lowercase, ".pdf")){
                pr_add_header(pr, "Content-Type: application/pdf");
            }
        }
    }
    // finish setting request
    pr_set_method(pr, PUT);
    pr_set_url(pr, server);
    pr->url = combine(&(pr->url), pr->url, db, "/");
    pr->url = combine(&(pr->url), pr->url, doc, "/");
    pr->url = combine(&(pr->url), pr->url, filename, "/");
    // TODO: add support for adding to existing documents by auto-fetching the rev parameter
    // pr_add_param(pr, "rev", rev);
    return pr;
}
    pouch_request *pr_add_usrpwd(pouch_request *pr, char *usrpwd, size_t length){
        if (pr->usrpwd)
            free(pr->usrpwd);
        pr->usrpwd = (char *)malloc(length);
        memcpy(pr->usrpwd, usrpwd, length);
        return pr;
    }
