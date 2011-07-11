#POUCH - Penn cOUCHdb interface
###A pure C wrapper for working with CouchDB.
##Depends
libcurl: link to at compile time with -lcurl
##Usage
	gcc -o $program $program.c pouch.c -lcurl

to compile the example program, demo.c, which
uses Joseph Adams' [JSON library](http://git.ozlabs.org/?p=ccan;a=tree;f=ccan/json):

	gcc -o demo demo.c pouch.c json.c -lcurl
##Description
coming soon

###todo:
- add an error holder to each pouch_request to hold
  wrapper return status (for instance, if reading a
  file to upload fails, set the status as -1, and
  return the pouch_request pointer as is); pouch
  needs error checking.
- update the documentation, clean it up, and make it
  valid for doxygen
  (http://drupal.org/node/1354#inline)
