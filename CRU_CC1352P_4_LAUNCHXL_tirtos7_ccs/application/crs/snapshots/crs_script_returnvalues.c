/*
 * crs_script_returnvalues.c
 *
 *  Created on: Jan 11, 2023
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h> // str
#include "crs_script_returnvalues.h"
#include "application/crs/crs_cli.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
enum
{
    STATUS_ELEMENT,
    NUM_OF_ELEMENTS
};


#define HASH_STARTING_VAL   5381 // start value for hashing algorithm
#define HASH_TABLE_MAX_SIZE 10
#define STATUS_KEY  "status"

struct Node{
    char *key;
    char *value;
    struct Node *next;
};

typedef struct HashTable{
    struct Node *table[HASH_TABLE_MAX_SIZE];
} hashTable_t;


typedef struct scriptRetValElement
{
    char key [RETVAL_ELEMENT_KEY_SZ];
    char val [RETVAL_ELEMENT_VAL_SZ];
}scriptRetValElement_t;

typedef struct scriptRetVal
{
    scriptRetValElement_t elements[NUM_OF_ELEMENTS];
}scriptRetVal_t;


/******************************************************************************
 Global Variables
 *****************************************************************************/
static char *gScriptRetVals_statusMessages [] = {
                                          "OK",
                                          "General Failure",
                                          "Param out of range",
                                          "Invalid script line"
                                        };
//static scriptRetVal_t gRetVal = {0};
static bool isModuleInit = false;
static hashTable_t *gRetVal = NULL;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static uint32_t hash(char *key);
static hashTable_t *hashTableCreate(void);
static void hashTableDelete(hashTable_t *ht);
static CRS_retVal_t hashTableInsert(hashTable_t *ht, char *key, char *value);
static char *hashTableSearch(hashTable_t *ht, char *key);
static void hashTableRmKey(hashTable_t *ht, char *key);
static struct Node *findNode(hashTable_t *ht, char *key, uint32_t *index);
static struct Node *nodeCreate(char *key, char *value, struct Node *next);
static void nodeDelete(struct Node *node);

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t ScriptRetVals_init(void)
{
    hashTableDelete(gRetVal);
    gRetVal = hashTableCreate();
    if (NULL == gRetVal)
    {
        return CRS_FAILURE;
    }
    isModuleInit = true;
    if (CRS_SUCCESS != ScriptRetVals_setValue(STATUS_KEY, gScriptRetVals_statusMessages[scriptRetVal_OK]))
    {
        hashTableDelete(gRetVal);
        isModuleInit = false;

        return CRS_FAILURE;
    }

    return CRS_SUCCESS;

}

CRS_retVal_t ScriptRetVals_getValue(char *key, char *_value)
{
    if (!isModuleInit)
    {
        return CRS_FAILURE;
    }

//    uint8_t i = 0;
//    for(i = 0; i < NUM_OF_ELEMENTS; i++)
//    {
//        if(0 == memcmp(gRetVal.elements[i].key, key, strlen(key)))
//        {
//            memcpy(value, gRetVal.elements[i].val, strlen(gRetVal.elements[i].val));
//            return CRS_SUCCESS;
//        }
//    }
    char *val = hashTableSearch(gRetVal, key);
    if (NULL == val)
    {
        return CRS_FAILURE;
    }

    memcpy(_value, val, strlen(val));

    return CRS_SUCCESS;
}

CRS_retVal_t ScriptRetVals_setValue(char *key, char* value)
{
    if (!isModuleInit)
    {
        return CRS_FAILURE;
    }

//    uint8_t i = 0;
//    for(i = 0; i < NUM_OF_ELEMENTS; i++)
//    {
//        if(0 == memcmp(gRetVal.elements[i].key, key, strlen(key)))
//        {
//            memset(gRetVal.elements[i].val, 0, RETVAL_ELEMENT_VAL_SZ);
//            memcpy(gRetVal.elements[i].val, value, strlen(value));
//            return CRS_SUCCESS;
//        }
//    }

//    return CRS_FAILURE;

    return hashTableInsert(gRetVal, key, value);


}

CRS_retVal_t ScriptRetVals_setStatus(scriptStatusReturnValue_t status)
{

    char *statusMsg = gScriptRetVals_statusMessages[status];
    return ScriptRetVals_setValue(STATUS_KEY, statusMsg);
}

scriptStatusReturnValue_t ScriptRetVals_getStatus(void)
{
    char statusLine[RETVAL_ELEMENT_VAL_SZ] = {0};
    ScriptRetVals_getValue(STATUS_KEY, statusLine);
    uint8_t i = 0;
    for (i = 0; i < scriptRetVal_Num_of_retVals;i++)
    {
        if (0 == memcmp(statusLine, gScriptRetVals_statusMessages[i], strlen(gScriptRetVals_statusMessages[i])))
        {
            return (scriptStatusReturnValue_t) i;
        }
    }

    return scriptRetVal_OK; //should never happen, condition will always be true at some value
}


CRS_retVal_t ScriptRetVals_printAll(void)
{
    if (!isModuleInit)
    {
        return CRS_FAILURE;
    }

    uint8_t i = 0;
    struct Node* node = NULL;
    for (i = 0; i < HASH_TABLE_MAX_SIZE; i++)
    {
        node = gRetVal->table[i];
        while (NULL != node)
        {
            CLI_cliPrintf("\r\n%s: %s",node->key, node->value);
            node = node->next;
        }
    }

    return CRS_SUCCESS;
}

/******************************************************************************
 Local Functions
 *****************************************************************************/
/*
 * This function uses an algorithm called DJB2 to generate a hash value for the given string
 */
static uint32_t hash(char *key)
{
    uint32_t hash = HASH_STARTING_VAL;
    uint32_t c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return (hash % HASH_TABLE_MAX_SIZE); //ensure the value is within range of the table size
}

static hashTable_t *hashTableCreate(void)
{
    hashTable_t *ht = CRS_malloc(sizeof(hashTable_t));
    if (NULL == ht)
    {
        return NULL;
    }
    memset(ht, 0, sizeof(hashTable_t));
    return ht;
}
static void hashTableDelete(hashTable_t *ht)
{
    if (NULL == ht)
    {
        return;
    }
    uint32_t i = 0;
    for (i = 0; i < HASH_TABLE_MAX_SIZE; i++) {
            struct Node* current = ht->table[i];
            while (current != NULL) {
                struct Node* next = current->next;
                nodeDelete(current);
                current = next;
            }
        }
    CRS_free((char**)&ht);
}

/*
 * This function inserts a new value with given key to the hash table
 * if the key exists it rewrites the value and returns CRS_SUCCESS
 * if it dosent exist it makes a new key and returns CRS_SUCCESS
 * in case of a allocation failure it returns CRS_FAILURE
 */
static CRS_retVal_t hashTableInsert(hashTable_t *ht, char *key, char *value)
{
    uint32_t index = 0;
    struct Node *currentNode = findNode(ht, key, &index);
    if (NULL != currentNode) // if key exists
    {
        CRS_free(&(currentNode->value));
        currentNode->value = strdup(value);
        if (NULL == currentNode->value)
        {
            return CRS_FAILURE;
        }
        return CRS_SUCCESS;
    }
    // insert new key
    currentNode = nodeCreate(key, value, NULL);
    if (NULL == currentNode)
    {
        return CRS_FAILURE;
    }

    // insert node to the head of the list
    currentNode->next = ht->table[index];
    ht->table[index] = currentNode;


    return CRS_SUCCESS;
}

/*
 * This function returns value of given key in the hash table
 * returns NULL if key is not in the table
 */
static char *hashTableSearch(hashTable_t *ht, char *key)
{
    uint32_t index = 0;
    struct Node *n = findNode(ht, key,&index);
    if (NULL == n)
    {
        return NULL;
    }

    return n->value;
}

static void hashTableRmKey(hashTable_t *ht, char *key)
{
    uint32_t index = 0;
    struct Node *n = findNode(ht, key,&index);
    if (NULL == n)
    {
        return;
    }
    nodeDelete(n);
}

static struct Node *findNode(hashTable_t *ht, char *key, uint32_t *index)
{
    *index = hash(key);
    struct Node *currentNode = ht->table[*index];
    while(NULL != currentNode)
    {
        if (0 == memcmp(currentNode->key, key, strlen(key))) // this loop helps for when collisions occur loop over a linked list
        {
            return currentNode;
        }

        currentNode = currentNode->next;
    }

    return NULL;
}

/*
 * When using this function if either key or value are passed as NULL an empty node is created,
 * a NULL next param is allowed
 */
static struct Node *nodeCreate(char *key, char *value, struct Node *next)
{
    struct Node *node = CRS_malloc(sizeof(struct Node));
    if (NULL == node)
    {
        return NULL;
    }

    node->next = next;
    if (key == NULL || value == NULL)
    {
        return node;
    }

    node->key = strdup(key);
    if (NULL == node->key)
    {
        CRS_free((char**)&node);

        return NULL;
    }

    node->value = strdup(value);
    if (NULL == node->value)
    {
        CRS_free(&(node->key));
        CRS_free((char**)&node);

        return NULL;
    }

    return node;
}
static void nodeDelete(struct Node *node)
{
    CRS_free(&(node->key));
    CRS_free(&(node->value));
    CRS_free((char **)&node);
}


