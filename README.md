POUCH
=======
###Penn cOUCHdb interface
A pure C wrapper for working with CouchDB.

####todo:
- add an error holder to each pouch_request to hold
  wrapper return status (for instance, if reading a
  file to upload fails, set the status as -1, and
  return the pouch_request pointer as is);
- add a *good* way to connect to authenticated
  servers.
