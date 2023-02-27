#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <csse2310a3.h>  
#include <csse2310a4.h>
#include <pthread.h>
#include <stringstore.h>
#include <signal.h>

#define EXIT_USAGE_ERROR 1
#define EXIT_AUTHFILE_ERROR 2
#define EXIT_SOCKET_ERROR 3
#define MIN_ARGUMENTS 3
#define MAX_ARGUMENTS 4
#define AUTHFILE_ARG 1
#define CONNECTIONS_ARG 2
#define PORTNUM_ARG 3
#define KEY_ARG 2
#define DEFAULT_PORTNUM "0"
#define MIN_PORTNUM 1024
#define MAX_PORTNUM 65535
#define MAX_URL_LENGTH 3
#define BASE_10 10
#define NO_BODY "0"

#define OK_STATUS 200
#define OK_EXPLAIN "OK"
#define NOT_FOUND_STATUS 404
#define NOT_FOUND_EXPLAIN "Not Found"
#define INTERNAL_ERROR_STATUS 500
#define INTERNAL_ERROR_EXPLAIN "Internal Server Error"
#define BAD_STATUS 400
#define BAD_EXPLAIN "Bad Request"
#define UNAUTHORIZED_STATUS 401
#define UNAUTHORIZED_EXPLAIN "Unauthorized"
#define UNAVAILABLE_STATUS 503
#define UNAVAILABLE_EXPLAIN "Service Unavailable"

/* Command line arguments passed when creating dbserver */
typedef struct ServerParameters {
    char* authString;
    int connections;
    char* portnum;
} ServerParameters;

/* The server statistics */
typedef struct ServerStats {
    int connected;
    int completed;
    int authFails;
    int getOps;
    int putOps;
    int deleteOps;
} ServerStats;

/* Arguments to be passed into client handling thread */
typedef struct ThreadParameters {
    int client;
    StringStore* public;
    StringStore* private;
    char* authString;
    ServerStats* stats;
    pthread_mutex_t lock;
} ThreadParameters;

/* Arguments to be passed into signal handling thread */
typedef struct SigParameters {
    sigset_t set;
    ServerStats* stats;
} SigParameters;

/* usage_error()
 * −−−−−−−−−−−−−−−
 * Exits the program with the usage error message
 * 
 * Returns: Exit code 1
 */
void usage_error(void) {
    fprintf(stderr, "Usage: dbserver authfile connections [portnum]\n");
    exit(EXIT_USAGE_ERROR);
}

/* digits_only()
 * −−−−−−−−−−−−−−−
 * Determines if a given string is a valid number
 * 
 * string: the string to check
 */
int digits_only(const char* string) {
    char* notDigits;
    strtol(string, &notDigits, BASE_10);
    if (string[0] == '\0' || *notDigits != '\0') {
        return 0;
    } else {
     	return 1;
    }
}

/* process_command_arguments()
 * −−−−−−−−−−−−−−−
 * Checks and extracts command line arguments provided.
 * Exits the program if invalid.
 * 
 * argc: argument count
 * argv: argument vector
 *
 * Returns: Parsed command line arguments,
 *          Exit code 1 if invalid arguments are given
 *          Exit code 2 if invalid authFile is given 
 */
ServerParameters process_command_arguments(int argc, char** argv) {
    ServerParameters parameters;
    // Check number if command line arguments is correctly supplied
    if (argc < MIN_ARGUMENTS || argc > MAX_ARGUMENTS) {
	usage_error();
    }

    // Check for valid connection parameter
    char* connectionsParam = argv[CONNECTIONS_ARG];
    if (digits_only(connectionsParam) && connectionsParam[0] != '-') {
	parameters.connections = atoi(connectionsParam);
    } else {
	usage_error();
    }

    // Check if portnum is specified, otherwise set to 0
    parameters.portnum = DEFAULT_PORTNUM;
    if (argc == MAX_ARGUMENTS) {
	char* portnum = argv[PORTNUM_ARG];
	if (strcmp(portnum, "0") == 0 || (digits_only(portnum) && 
		atoi(portnum) >= MIN_PORTNUM && 
		atoi(portnum) <= MAX_PORTNUM)) {
	    parameters.portnum = portnum;
	} else {
	    usage_error();
	}
    }

    // Check for valid authFile
    FILE* authFile = fopen(argv[AUTHFILE_ARG], "r");
    char* authString = NULL;
    // Extract authString when authFile is valid
    if (authFile != NULL) {
	authString = read_line(authFile);
    }
    // Exit when file cannot be opened
    if (authString == NULL) {
	fprintf(stderr, "dbserver: unable to read authentication string\n");
	exit(EXIT_AUTHFILE_ERROR);
    }
    fclose(authFile);
    parameters.authString = authString;
    return parameters;
}

/* setup_listen()
 * −−−−−−−−−−−−−−−
 * Sets up the server socket for listening.
 * Exits the program if invalid.
 * 
 * portnum: the port number
 * connections: maximum amount of connections
 *
 * Returns: The socket file descriptor bounded
 *          Exit code 3 if socket is unable to listen
 */
int setup_listen(char* portnum, int connections) {
    // Set up addrinfo struct
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo("localhost", portnum, &hints, &ai);

    // Create socket and bind it to port
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    // Allow address (port number) to be reused immediately
    int optVal = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int));

    if (bind(serverSocket, (struct sockaddr*)ai->ai_addr,
	    sizeof(struct sockaddr))) {
    	fprintf(stderr, "dbserver: unable to open socket for listening\n");
	exit(EXIT_SOCKET_ERROR);
    }
    listen(serverSocket, connections);
    return serverSocket;
}

/* construct_empty_headers()
 * −−−−−−−−−−−−−−−
 * Constructs HTTP response header for no body
 *
 * Returns: HTTP response header
 */
HttpHeader** construct_empty_headers() {
    HttpHeader* header = malloc(sizeof(HttpHeader));
    HttpHeader** headers = malloc(sizeof(HttpHeader*) + 1);
    header->name = "Content-Length";
    headers[0] = header; // Set 1st elem of header array to the header created
    header->value = NO_BODY;

    return headers;
}

/* send_empty_http_response()
 * −−−−−−−−−−−−−−−
 * Sends a HTTP response with the specified status with no body
 * 
 * status: HTTP response status code
 * statusExplain: HTTP response status message
 * toClient: file descriptor writing to connected client
 */
void send_empty_http_response(int status, char* statusExplain, int toClient) {
    HttpHeader** headers = construct_empty_headers();
    char* response = construct_HTTP_response(status, statusExplain, 
	    headers, NULL);
    write(toClient, response, strlen(response));
}

/* process_get_request()
 * −−−−−−−−−−−−−−−
 * Processes GET requests from the client and sends back the 
 * HTTP response based on the operation
 * 
 * stats: the server statistics
 * store: database API
 * toClient: file descriptor writing to connected client
 * key: the value's key in the database
 */
void process_get_request(ServerStats* stats, struct StringStore* store, 
	int toClient, char* key) {
    char* response;
    const char* value;

    HttpHeader** headers = construct_empty_headers();
    value = stringstore_retrieve(store, key);

    // Send HTTP response based on value retrieved
    if (value == NULL) {
    	response = construct_HTTP_response(NOT_FOUND_STATUS, 
		NOT_FOUND_EXPLAIN, headers, NULL);
    } else {
	// Get content length by converting int to string
	int valueLength = strlen(value);
	char contentLength[sizeof(int) + 1];
	sprintf(contentLength, "%d", valueLength);

	headers[0]->value = contentLength;
	response = construct_HTTP_response(OK_STATUS, 
		OK_EXPLAIN, headers, value);
	stats->getOps++; // successful GET request processed
    }
    write(toClient, response, strlen(response));
}

/* process_put_request()
 * −−−−−−−−−−−−−−−
 * Processes PUT requests from the client and sends back the
 * HTTP response based on the operation
 * 
 * stats: the server statistics
 * store: database API
 * toClient: file descriptor writing to connected client
 * key: the value's key in the database
 * valueToUpdate: the key's value in the database
 */
void process_put_request(ServerStats* stats, struct StringStore* store,
	int toClient, char* key, char* valueToUpdate) {
    int status;
    char* statusExplain;
    
    // Get status code and explanation based on if the operation succeeds
    int err;
    if ((err = stringstore_add(store, key, valueToUpdate)) == 0) {
	status = INTERNAL_ERROR_STATUS;
	statusExplain = INTERNAL_ERROR_EXPLAIN;
    } else {
	status = OK_STATUS;
	statusExplain = OK_EXPLAIN;
	stats->putOps++; // successful PUT request processed
    }
    send_empty_http_response(status, statusExplain, toClient);
}

/* process_delete_request()
 * −−−−−−−−−−−−−−−
 * Processes DELETE requests from the client and sends back the
 * HTTP response based on the operation
 * 
 * stats: the server statistics
 * store: database API
 * toClient: file descriptor writing to connected client
 * key: the value's key in the database
 */
void process_delete_request(ServerStats* stats, struct StringStore* store, 
	int toClient, char* key) {
    int status;
    char* statusExplain;

    // Get status code and explanation based on if the operation succeeds
    int err;
    if ((err = stringstore_delete(store, key)) == 0) {
	status = NOT_FOUND_STATUS;
	statusExplain = NOT_FOUND_EXPLAIN;
    } else {
	status = OK_STATUS;
	statusExplain = OK_EXPLAIN;
	stats->deleteOps++; // successful DELETE request processed
    }
    send_empty_http_response(status, statusExplain, toClient);
}

/* check_authorization()
 * −−−−−−−−−−−−−−−
 * Checks for authorization from the client
 * 
 * headers: the headers from HTTP request
 * authString: authorization string
 * 
 * Returns: 1 if authorization is successful, 0 otherwise
 */
int check_authorization(HttpHeader** headers, char* authString) {
    int authorized = 0; // Indicates if a valid authorization header is found
    // Finds and checks if authorization header and string is valid
    for (int i = 0; headers[i] != NULL; i++) {
	if ((strcmp((headers[i])->name, "Authorization") == 0) && 
		(strcmp(((headers[i])->value), authString) == 0)) {
	    authorized = 1;
	}   
    }
    return authorized; 
}

/* check_valid_request()
 * −−−−−−−−−−−−−−−
 * Checks if a HTTP request is well-formed and valid
 * 
 * method: the request type
 * parsedAddress: parsed address URL
 * 
 * Returns: 1 if HTTP request is valid, 0 otherwise
 */
int check_valid_request(char* method, char** parsedAddress, 
	HttpHeader** headers, char* body) {
    // Check if given request is a valid method
    if ((strcmp(method, "GET")) && (strcmp(method, "PUT")) && 
	    (strcmp(method, "DELETE"))) {
	return 0;
    }

    // Check for valid address
    int i;
    for (i = 0; parsedAddress[i] != NULL; i++) {	
	// Checks if database type given is in the correct address URL format
	if (i == 1) {
	    if (strcmp(parsedAddress[i], "public") && 
		    strcmp(parsedAddress[i], "private")) {
		return 0;
	    }   
	}
    }

    // Checks if address URL length is valid
    if (i > MAX_URL_LENGTH) {
	return 0;
    }
    return 1;
}

/* process_method()
 * −−−−−−−−−−−−−−−
 * Processes the HTTP request based on its request type
 * 
 * method: the request type
 * stats: the server statistics
 * store: database API
 * toClient: file descriptor writing to connected client
 * key: the value's key in the database
 * body: the HTTP request body
 */
void process_method(char* method, ServerStats* stats, StringStore* store, 
	int toClient, char* key, char* body) {
    if (strcmp(method, "GET") == 0) {
	process_get_request(stats, store, toClient, key);
    } else if (strcmp(method, "PUT") == 0) {
	process_put_request(stats, store, toClient, key, body);
	
    } else if (strcmp(method, "DELETE") == 0) {
	process_delete_request(stats, store, toClient, key);
    }
}

/* disconnect_client()
 * −−−−−−−−−−−−−−−
 * Closes any open file descriptors and streams. Frees the headers.
 * 
 * headers: the HTTP response header
 * store: database API
 * toClient: file descriptor writing to client
 * fromClient: file stream reading from client
 * clientReadEnd: file descriptor reading to client
 */
void disconnect_client(HttpHeader** headers, ServerStats* stats, int toClient, 
	FILE* fromClient, int clientReadEnd) {
    free_array_of_headers(headers);
    // Closes all file descriptors and streams
    fclose(fromClient);
    close(toClient);
    close(clientReadEnd);
    // Indicate that client is completed and has finished connecting
    stats->completed++;
    stats->connected--;
}

/* handle_client()
 * −−−−−−−−−−−−−−−
 * Processes and handles HTTP requests from the client. Sends HTTP responses
 * based on the operation.
 *
 * arg: arguments passed to thread
 */
void* handle_client(void* arg) {
    // Extract arguments passed into thread function
    ThreadParameters arguments = *(ThreadParameters*)arg;
    char* authString = arguments.authString;
    ServerStats* stats = arguments.stats;
    pthread_mutex_t lock = arguments.lock;
    // Set up file descriptors, streams, and variables to be used
    int toClient = arguments.client;
    int clientReadEnd = dup(toClient);
    FILE* fromClient = fdopen(clientReadEnd, "r");
    char* method, *address, *body;
    HttpHeader** headers;
    int err;
    int failedAuthorization = 0; // Determines if an authorization has failed
    struct StringStore* store; 
    // Repeatedly read requests from client until EOF
    while ((err = get_HTTP_request(fromClient, &method, &address, 
	    &headers, &body) == 1)) {
	// Check if given request is well-formed AND valid
	char** parsedAddress = split_by_char(address, '/', MAX_URL_LENGTH); 
	if (check_valid_request(method, parsedAddress, headers, body)) {
	    pthread_mutex_lock(&lock);
	    // Checks if request is private with valid authorization
	    char* databaseType = parsedAddress[1];
	    if (strcmp(databaseType, "private") == 0) {
		if (check_authorization(headers, authString) == 0) {
		    failedAuthorization = 1;
		    stats->authFails++;
		    send_empty_http_response(UNAUTHORIZED_STATUS, 
			    UNAUTHORIZED_EXPLAIN, toClient);
		}
		store = arguments.private; // Database set to private instance
	    } else {
		store = arguments.public; // Database set to public instance
	    } 
	    // Process requests into their respective functions
	    if (failedAuthorization == 0) {
		char* key = parsedAddress[KEY_ARG]; // Extract key from address
		process_method(method, stats, store, toClient, key, body);
	    }
	    failedAuthorization = 0;
	    pthread_mutex_unlock(&lock);
	} else {
	    // Sends ahttp request if request is not well-formed
	    send_empty_http_response(BAD_STATUS, BAD_EXPLAIN, toClient);
    	}
    }
    free(arg);
    disconnect_client(headers, stats, toClient, fromClient, clientReadEnd);
    return NULL;
}

/* print_stats()
 * −−−−−−−−−−−−−−−
 * Prints the server statistics
 *
 * stats: the server statistics
 */
void print_stats(ServerStats* stats) {
    fprintf(stderr, "Connected clients:%d\n"
	    "Completed clients:%d\n"
	    "Auth failures:%d\n"
	    "GET operations:%d\n"
	    "PUT operations:%d\n"
	    "DELETE operations:%d\n", stats->connected, stats->completed, 
	    stats->authFails, stats->getOps, stats->putOps, stats->deleteOps);
    fflush(stderr);
}

/* handle_sig()
 * −−−−−−−−−−−−−−−
 * Handles signals. Prints the server statistics based on the signal received.
 *
 * args: arguments passed to thread
 */
void* handle_sig(void* args) {
    SigParameters arguments = *(SigParameters*)args;
    sigset_t set = arguments.set;
    ServerStats* stats = arguments.stats;
    free(args);
    int sig;

    // Repeatedly wait until SIGHUP is detected
    while (1) {
    	sigwait(&set, &sig);
	if (sig == SIGHUP) {
	    print_stats(stats);
	}
    }
}

/* process_connections()
 * −−−−−−−−−−−−−−−
 * Processes connections and creates threads to handle them 	
 *
 * serverSocket: the file descriptor socket for communication to server
 * stats: the server statistics
 * publicStore: database API for public instance
 * privateStoreL database API for private instance
 * serverDetails: command line arguments when creating dbserver 
 */
void process_connections(int serverSocket, ServerStats* stats, 
	struct StringStore* publicStore, struct StringStore* privateStore, 
	ServerParameters serverDetails) {
    // Set up locks
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize; 
    pthread_t thread;
    // Set up sigset and sigmask to handle specific signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    // Set up arguments in struct to be passsed onto signal handling thread
    SigParameters* sigArgs = (SigParameters*)malloc(sizeof(SigParameters));
    sigArgs->set = set;
    sigArgs->stats = stats;
    pthread_create(&thread, NULL, &handle_sig, sigArgs);
    int newClient;
    // Keep accepting new connections
    while (1) {
	fromAddrSize = sizeof(struct sockaddr_in);
	newClient = accept(serverSocket, (struct sockaddr*)&fromAddr, 
		&fromAddrSize);
	// Check if the number of current connections has exceeded the limit
    	if (((stats->connected + 1) > serverDetails.connections) 
		&& serverDetails.connections != 0) {
	    send_empty_http_response(UNAVAILABLE_STATUS, 
		    UNAVAILABLE_EXPLAIN, newClient);
	    close(newClient);
	} else {
	    stats->connected++;
	} 
	// Set up arguments in struct to be passed onto client handling thread
	ThreadParameters* args = 
		(ThreadParameters*) malloc(sizeof(ThreadParameters));
	args->client = newClient;
	args->public = publicStore;
	args->private = privateStore;
	args->authString = serverDetails.authString;
	args->stats = stats;
	args->lock = lock;
	// Create thread to handle accepted connections
	pthread_t threadId;
	pthread_create(&threadId, NULL, handle_client, args);
	pthread_detach(threadId);
    }
}

/* print_port()
 * −−−−−−−−−−−−−−−
 * Extracts the port from the socket and prints it
 *
 * serverSocket: the file descriptor socket for communication to server
 */
void print_port(int serverSocket) {
    struct sockaddr_in ad;
    // Extracts the port number
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    getsockname(serverSocket, (struct sockaddr*)&ad, &len);
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));
    fflush(stderr);
}

/* server_stats_init()
 * −−−−−−−−−−−−−−−
 * Initialises the server statistics struct
 *
 * Returns: the initialised server statistics
 */
ServerStats* server_stats_init(void) {
    ServerStats* newStats = malloc(sizeof(ServerStats));
    newStats->connected = 0;
    newStats->completed = 0;
    newStats->authFails = 0;
    newStats->getOps = 0;
    newStats->putOps = 0;
    newStats->deleteOps = 0;
    return newStats;
}

int main(int argc, char** argv) {
    ServerStats* stats = server_stats_init();
    // Creates public and private instances of StringStore
    struct StringStore* publicStore = stringstore_init();
    struct StringStore* privateStore = stringstore_init();

    // Sets up connections based on command line arguments
    ServerParameters serverDetails = process_command_arguments(argc, argv);
    int serverSocket = setup_listen(serverDetails.portnum, 
	    serverDetails.connections);
    print_port(serverSocket);
    // Processes connections 
    process_connections(serverSocket, stats, publicStore, privateStore, 
	    serverDetails);
    return(EXIT_SUCCESS);
}
    
