#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "pthread.h"
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SLEEP_TIME 20000
#define MAX_SERVER_CONNECTIONS 4
#define MAX_SYN_REQUESTS 4
#define MAX_ENTITIES 64
#define MAX_LENGTH 1024

#define DECIMAL 10
#define MAX_COMMAND_LENGTH 64
#define FILE_SEPARATOR ","
#define INFO_SEPARATOR ":"
#define NUM_OF_SEPARATORS_PER_INFO 2
#define INFO_LENGTH 21 // :***.***.***.***:**** - :ip_address:port

#define DEFAULT_NAME "default_name"
#define MY_NAME "hardlight"
#define REQUEST 0
#define SYN 1
#define FLOOD 2
#define EXIT_COMMAND 3

typedef struct node_t {
    char *name;
    struct sockaddr_in *sockaddr;
} node_t;

typedef struct file_t {
    char *name;
    struct node_t *owner;
} file_t;

typedef struct net_t {
    int socket_fd;
    struct sockaddr_in sockaddr_in;
} net_t;

typedef struct node_database_t {
    struct node_t *nodes;
    int *queries;
    int count;
    pthread_mutex_t lock;
} node_database_t;

typedef struct file_database_t {
    struct file_t *files;
    int count;
    pthread_mutex_t lock;
} file_database_t;

typedef struct blacklist_database_t {
    struct in_addr *addresses;
    int count;
    pthread_mutex_t lock;
} blacklist_database_t;

typedef struct arg_database_t {
    char **data;
    int count;
    pthread_mutex_t lock;
} arg_database_t;

typedef struct global_data_t {
    struct node_database_t *nodes;
    struct file_database_t *files;
    struct blacklist_database_t *blacklist;
    struct arg_database_t *args;
} global_data_t;

void *client_thread(void *data_void);

void *server_thread(void *data_void);

void make_connection(struct global_data_t *gl_data, struct sockaddr_in *server);

void request_file(struct global_data_t *gl_data, struct sockaddr_in *server);

void flood_attack(struct global_data_t *gl_data, struct sockaddr_in *server;

void accept_connection(struct global_data_t *gl_data, struct net_t *net);

void send_file(struct global_data_t *gl_data, struct net_t *net);

char *get_myself_info(struct global_data_t *gl_data);

char *format_node(struct node_t *node);

struct node_t unformat_node(char *str_node);

char *format_file(struct file_t *file);

struct file_t unformat_file(char *str_file);

void load_nodes(FILE *file, struct node_database_t *node_db);

void load_files(FILE *file, struct file_database_t *file_db);

void load_args(int argc, char **argv, struct arg_database_t *arg_db);

struct node_database_t init_node_db();

struct file_database_t init_file_db();

struct blacklist_database_t init_blacklist_db();

struct arg_database_t init_args_db();

int add_node(struct node_t *node, struct node_database_t *node_db);

int add_file(struct file_t *file, struct file_database_t *file_db);

int add_in_blacklist(struct in_addr ip_addr, struct blacklist_database_t *blacklist_db);

int delete_node(struct sockaddr_in *sockaddr, struct node_database_t *node_db);

int is_exist_node(struct sockaddr_in *sockaddr, struct node_database_t *node_db);

int is_exist_file(struct file_t *file, struct file_database_t *file_db);

int is_contain_ip(struct in_addr *ip_addr, struct blacklist_database_t *blacklist_db);

int count_words(FILE *file);

struct file_database_t split_files(struct node_t *owner, char *data);

struct node_t parse_node(char *str_node);

char *convert_node(struct node_t *node);

int increment_query(int index, struct node_database_t *node_db);

int decrement_query(int index, struct node_database_t *node_db);
