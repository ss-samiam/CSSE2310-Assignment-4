# CSSE2310-Assignment-4
This program was developed as part of a course (CSSE2310 - Computer Systems Principles and Programming) I undertook at the University of Queensland.

The program is a network client/server application that uses multiple threads to communicate using HTTP requests and responses and a simple RESTful API. 
+ ``dbserver`` - is a networked database server that takes requests from clients, allowing them to store, retrieve and delete string-based key/value pairs.
+ ``dbclient`` - is a simple network client that can query the database managed by ``dbserver``. Communication between the ``dbclient`` and ``dbserver`` is done using HTTP requests and responses, using a simple RESTful API. Advanced functionality such as authentication, connection limiting, signal handling, and statistics reporting are also implemented.

### dbclient
The program provides a commandline interface to allow access to a subset of the dbserver’s capabilities, in particular it permits only the setting and retrieving of key/value pairs. It does not support ``dbserver`` authentication, or deletion of key/value pairs.

### dbserver
``dbserver`` is a networked database server, capable of storing and returning text-based key/value pairs. Client requests and server responses are communicated over HTTP.
+ The ``GET`` operation permits a client to query the database for the provided key. If present, the server returns the corresponding stored value.
+ The ``PUT`` operation permits a client to store a key/value pair. If a value is already stored for the provided key, then it is replaced by the new value.
+ The ``DELETE`` operation permits a client to delete a stored key/value pair. ``dbserver`` must implement at least one database instance, known as public, which can be accessed by any connecting client without authentication.
