#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringstore.h>

/* A database to store keys and its respective value */
struct StringStore { 
    const char* key;
    const char* value;
    struct StringStore* nextEntry;
};

StringStore* stringstore_init(void) {
    StringStore* firstEntry = malloc(sizeof(StringStore));
    firstEntry->key = NULL;
    firstEntry->value = NULL;
    firstEntry->nextEntry = NULL;
    return firstEntry;
}

StringStore* stringstore_free(StringStore* store) {
    StringStore* currentEntry;
    while(store != NULL) {
	currentEntry = store;
	store = store->nextEntry;
	free((char*)(currentEntry->key));
	free((char*)(currentEntry->value));
	free(currentEntry);
    }
    return store;
}

int stringstore_add(StringStore* store, const char* key, const char* value) {
    StringStore* currentEntry;
    char* newKey = strdup(key);
    char* newValue = strdup(value);
    if (newKey == NULL || newValue == NULL) {
	return 0;
    } else {
	for (;;) {
	    currentEntry = store;
	    store = store->nextEntry;
    	    if (store == NULL) {
    		// Populate the next entry
    		StringStore* newEntry = malloc(sizeof(StringStore));
    		newEntry->key = newKey;
    		newEntry->value = newValue;
    		newEntry->nextEntry = NULL;
    		// Link the new entry to the previous
    		currentEntry->nextEntry = newEntry;
    		return 1;
	    // Overwrite value if given key exist already
    	    } else if (strcmp(store->key, newKey) == 0) {
		free(newKey);
    		free((char*)(store->value));
    		store->value = newValue;
		return 1;
    	    }
	}
    }    
}

const char* stringstore_retrieve(StringStore* store, const char* key) {
    // Disregard the head (NULL entry)
    store = store->nextEntry;
    // Goes through every entry and return the value if the key is found
    while(store != NULL) {
	if (strcmp(store->key, key) == 0) {
	    return store->value;
	}
	store = store->nextEntry;
    }
    return NULL;
}

int stringstore_delete(StringStore* store, const char* key) {
    StringStore* currentEntry;
    // Check if the given key exists
    if (stringstore_retrieve(store, key) != NULL) {
	while(store != NULL) {
	    currentEntry = store;
	    store = store->nextEntry;
	    if (strcmp(store->key, key) == 0) {
		currentEntry->nextEntry = store->nextEntry;
		free((char*)store->key);
		free((char*)store->value);
		free(store);
		return 1;
	    }
	}
    }
    return 0;
}


