#POUCH - Penn cOUCHdb interface
*A pure C wrapper for working with CouchDB.*
##Depends
libcurl: link to at compile time with -lcurl
##Usage
gcc -o $program $program.c pouch.c -lcurl
##Description
coming soon

###todo:
- add an error holder to each pouch_request to hold
  wrapper return status (for instance, if reading a
  file to upload fails, set the status as -1, and
  return the pouch_request pointer as is);
- add a *good* way to connect to authenticated
  servers.
- update the documentation, clean it up, and make it
  valid for doxygen
  (http://drupal.org/node/1354#inline)
