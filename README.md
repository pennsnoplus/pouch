#POUCH - Penn cOUCHdb interface
###A pure C wrapper for working with CouchDB.

##Depends
libcurl: link to at compile time with -lcurl

##Usage
	gcc -o $program $program.c pouch.c -lcurl

to compile the example program, demo.c, which
uses an extension of Joseph Adams' [JSON library](http://git.ozlabs.org/?p=ccan;a=tree;f=ccan/json):

	make

##Description
A simple way to interact with CouchDB instances through low-level
CURL requests and the JSON library of your choice. 
