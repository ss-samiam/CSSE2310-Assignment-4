#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <csse2310a3.h>
#include <csse2310a4.h>

#define EXIT_USAGE_ERROR 1
#define EXIT_CONNECTION_ERROR 2
#define OK_STATUS 200
#define CLIENT_GET_ERROR 3
#define CLIENT_PUT_ERROR 4
#define PROCESS_NAME 1
#define PORTNUM_ARG 1
#define KEY_ARG 2
#define VALUE_ARG 3

/* check_usage()
 * −−−−−−−−−−−−−−−
 * Checks if commandline arguments are valid and prints its respective 
 * error message if invalid
 *
 * argc: argument count
 * argv: argument vector
 *
 * Returns: Exit code 1
 */
void check_usage(int argc, char** argv) {
    // Check if arguments have fewer than 2 arguments (no key argument)
    if (argc <= KEY_ARG) {
	fprintf(stderr, "Usage: dbclient portnum key [value]\n");
	exit(EXIT_USAGE_ERROR);
    }
    // Check if key argument contains spaces or newlines
    if (strchr(argv[KEY_ARG], ' ') != NULL || 
	    strchr(argv[KEY_ARG], '\n') != NULL) {
    	fprintf(stderr, 
		"dbclient: key must not contain spaces or newlines\n");
	exit(EXIT_USAGE_ERROR);
    }
}

/* process_http_status()
 * −−−−−−−−−−−−−−−
 * Checks if HTTP response code from server is a success, otherwise exit 
 * with its respective exit code
 *
 * httpStatus: HTTP response code from server
 * getFlag: determines if a GET response was sent to the server
 *
 * Returns: Exit code 3 if GET request results in a bad response code
 * 	    Exit code 4 if PUT request results in a bad response code
 */
void process_http_status(int httpStatus, int getFlag) {
    // Check if http status returned is anything but 200 (OK) response
    if (httpStatus != OK_STATUS) {
	// Determine which exit code to exit depending on if 
	// GET or PUT was used
	if (getFlag == 1) {
	    exit(CLIENT_GET_ERROR);
	} else {
	    exit(CLIENT_PUT_ERROR);
	}
    }
}

/* process_http_response()
 * −−−−−−−−−−−−−−−
 * Extracts the HTTP response code into httpStatus variable passed
 *
 * fromServer: file stream to read from server
 * httpStatus: HTTP response code from server
 * body: body of HTTP response
 */
void process_http_response(FILE* fromServer, int* httpStatus, char** body) {
    char* statusExplain;
    HttpHeader** headers;
    // Extracts the relevant information from HTTP response
    get_HTTP_response(fromServer, httpStatus, &statusExplain, &headers, body);
    free_array_of_headers(headers);
}

/* setup_connection()
 * −−−−−−−−−−−−−−−
 * Sets up the connection to the server by constructing and populating
 * the addrinfo struct
 *
 * portnum: the port number
 *
 * Returns: the populated addrinfo struct
 */
struct addrinfo* setup_connection(char* portnum) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // Set up for IPv4
    hints.ai_socktype = SOCK_STREAM;
    // Exit with error depending if connection to port is not possible
    if (getaddrinfo("localhost", portnum, &hints, &ai)) {
	freeaddrinfo(ai);
	fprintf(stderr, "dbclient: unable to connect to port %s\n", portnum);
	exit(EXIT_CONNECTION_ERROR);
    }
    return ai;
}

int main(int argc, char** argv) {
    // Checks if the user input is valid then extract the relevant information
    check_usage(argc, argv);
    char* portnum = argv[PORTNUM_ARG];
    char* key = argv[KEY_ARG];
   
    // Set up connection to the server
    struct addrinfo* ai = setup_connection(portnum);
    int connectToServer = socket(AF_INET, SOCK_STREAM, 0);
    connect(connectToServer, ai->ai_addr, sizeof(struct sockaddr));

    // Set up relevant streams for reading and writing
    FILE* toServer = fdopen(connectToServer, "w");
    int connectFromServer = dup(connectToServer);
    FILE* fromServer = fdopen(connectFromServer, "r");

    // Set up variables for get_HTTP_response()
    int httpStatus;
    char* body;
    int getFlag = 0; // Determines if GET request is sent
    
    // Logic for PUT
    if (argc >= VALUE_ARG + PROCESS_NAME) {
	char* value = argv[VALUE_ARG];
	fprintf(toServer, "PUT /public/%s HTTP/1.1\r\n"
		"Content-Length: %zu\r\n"
		"\r\n"
		"%s", key, strlen(value), value);
    // Logic for GET
    } else {
	getFlag = 1;
	fprintf(toServer, "GET /public/%s HTTP/1.1\r\n"
    		"\r\n", key);
    }
    fflush(toServer);
    fclose(toServer);
    freeaddrinfo(ai);
    // Process the response from the server
    process_http_response(fromServer, &httpStatus, &body);
    process_http_status(httpStatus, getFlag);
    fclose(fromServer);
    if (getFlag) {
	printf("%s\n", body);
    }
    exit(EXIT_SUCCESS);
}
