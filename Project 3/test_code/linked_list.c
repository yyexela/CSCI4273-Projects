#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>

/*
struct timespec curr_time, req_time;
clock_gettime(CLOCK_MONOTONIC, &req_time);
*/

typedef struct node{
    struct timespec cache_time;
    char md5[39];
    struct node *next;
} node;

// Print the names of the items in the linked list
void print_ll();

// Create an empty node, returns the memory value of the malloc'd pointer
node * create_node();

// Given a node *, update its time to the current time
void update_time(node *ptr);

// Given a node *, update its name (md5)
void update_name(node *ptr, char *name);

// Add node to head of linked list
// (more recently created files are more likely to be accessed)
void add_node(node *ptr);

// Given a node *, delete it from the linked list freeing memory
void delete_node(node *ptr);

// Given a node *, return 1 if the name is in the ll, 0 otherwise
node * check_node(char *name);

pthread_mutex_t ll_lock;
node *head = NULL;
time_t cache_timeout = (time_t) 2; // cache timeout in seconds

// Print the names of the items in the linked list
void print_ll(){
    pthread_mutex_lock(&ll_lock);
    if(head == NULL)
        return;

    node *tmp = head;

    // See if the next value of tmp is ptr, then delete it and update the LL
    for(; tmp->next != NULL; tmp = tmp->next){
        printf("%s->", tmp->md5);
    }

    printf("%s\n", tmp->md5);

    pthread_mutex_unlock(&ll_lock);
}

// Create an empty node, returns the memory value of the malloc'd pointer
node * create_node(){
    unsigned long node_sz = sizeof(node);
    node *ptr = (node *) malloc(node_sz);
    memset(ptr, 0, node_sz);
    return ptr;
}

// Given a node *, update its time to the current time
void update_time(node *ptr){
    clock_gettime(CLOCK_MONOTONIC, &(ptr->cache_time));
}

// Given a node *, update its name (md5)
void update_name(node *ptr, char *name){
    memset(ptr->md5, 0, 39);
    strncpy(ptr->md5, name, 38);
}

// Add node to head of linked list
// (more recently created files are more likely to be accessed)
void add_node(node *ptr){
    pthread_mutex_lock(&ll_lock);
    if(head == NULL){
        // LL doesn't exist
        head = ptr;
    } else {
        // LL does exist
        ptr->next = head;
        head = ptr;
    }
    pthread_mutex_unlock(&ll_lock);
}

// Given a node *, delete it from the linked list freeing memory
void delete_node(node *ptr){
    pthread_mutex_lock(&ll_lock);

    // See if there's anything in the LL
    if(head == NULL){
        pthread_mutex_unlock(&ll_lock);
        return;
    }

    // See if the node is head
    if(head == ptr){
        head = ptr->next;
        free(ptr);
        pthread_mutex_unlock(&ll_lock);
        return;
    }

    // See if the next value of tmp is ptr, then delete it and update the LL
    for(node *tmp = head; tmp->next != NULL; tmp = tmp->next){
        if(tmp->next == ptr){
            tmp->next = tmp->next->next;
            free(ptr);
            pthread_mutex_unlock(&ll_lock);
            return;
        }
    }

    // Node isn't in LL
    pthread_mutex_unlock(&ll_lock);
}

node * check_node(char *name){
    pthread_mutex_lock(&ll_lock);

    // See if there's anything in the LL
    if(head == NULL){
        pthread_mutex_unlock(&ll_lock);
        return NULL;
    }

    // Check LL for matching name
    for(node *tmp = head; tmp != NULL; tmp = tmp->next){
        if(strncmp(tmp->md5, name, 38) == 0){
            pthread_mutex_unlock(&ll_lock);
            return tmp;
        }
    }

    // Node isn't in LL
    pthread_mutex_unlock(&ll_lock);
    return NULL;
}

int check_timeout(node *ptr){
    struct timespec curr_time;
    clock_gettime(CLOCK_MONOTONIC, &curr_time);
    if(ptr->cache_time.tv_sec + cache_timeout <= curr_time.tv_sec && ptr->cache_time.tv_nsec <= curr_time.tv_nsec){
        return 1;
    }
    return 0;
}

int main(int argc, char * argv[]){
    // Initialize mutex
    pthread_mutex_init(&ll_lock, NULL);

    // Initialize nodes
    node *one = NULL, *two = NULL, *three = NULL;
    one = create_node();
    two = create_node();
    three = create_node();
    update_time(one);
    update_time(two);
    update_time(three);
    update_name(one, "juan");
    update_name(two, "doritos");
    update_name(three, "terceratops");
    add_node(one);
    add_node(two);
    add_node(three);

    print_ll();
    delete_node(two);
    print_ll();
    two = create_node();
    update_time(two);
    add_node(two);
    update_name(two, "doritos");
    print_ll();
    delete_node(two);
    print_ll();
    two = create_node();
    update_time(two);
    add_node(two);
    update_name(two, "doritos");
    print_ll();
    delete_node(one);
    print_ll();

    one = create_node();
    update_time(one);
    add_node(one);
    update_name(one, "juan");
    print_ll();

    printf("\"juan\" in LL? ");
    if (check_node("juan") != NULL){
        printf("yes\n");
    } else {
        printf("no\n");
    }
    printf("\"doritos\" in LL? ");
    if (check_node("doritos") != NULL){
        printf("yes\n");
    } else {
        printf("no\n");
    }
    printf("\"terceratops\" in LL? ");
    if (check_node("terceratops") != NULL){
        printf("yes\n");
    } else {
        printf("no\n");
    }
    printf("\"bruh\" in LL? ");
    if (check_node("bruh") != NULL){
        printf("yes\n");
    } else {
        printf("no\n");
    }

    printf("time (\"juan\" - \"terceratops\"): %ld.%.9ld\n", one->cache_time.tv_sec - three->cache_time.tv_sec, one->cache_time.tv_nsec - three->cache_time.tv_nsec);
    printf("\"juan timeout?\" %d\n", check_timeout(one));

    sleep(1);

    printf("\"juan timeout?\" %d\n", check_timeout(one));
    
    sleep(1);

    printf("\"juan timeout?\" %d\n", check_timeout(one));

    pthread_mutex_destroy(&ll_lock);
}