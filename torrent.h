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

#define MAX_ENTITIES 64
#define MAX_NAME_LENGTH 128

#define DECIMAL 10
#define MAX_COMMAND_LENGTH 64
#define FILE_SEPARATOR ","
#define INFO_SEPARATOR ":"
#define NUM_OF_SEPARATORS_PER_INFO 2
#define INFO_LENGTH 21 // :***.***.***.***:**** - :ip_address:port

#define MY_NAME "torrent"
#define EXIT_COMMAND "exit"
#define FLOOD "2"
#define SYN "1"
#define REQUEST "0"

typedef struct database {
    char **data;
    int count;
    pthread_mutex_t lock;
} database;

typedef struct node_t {
    char *name;
    char *ip_address;
    int port;
} node_t;

typedef struct global_data {
    struct database *files;
    struct database *nodes;
    struct database *args;
} global_data;

typedef struct net_info {
    struct sockaddr_in sockaddrIn;
    int socket_fd;
} net_info;

void *client_thread(void *data_void);

void *server_thread(void *data_void);

void process_connection(struct global_data *, int socket);

void make_connection(struct global_data *, struct net_info *);

void request_file(struct global_data *, struct net_info *);

void accept_connection(struct global_data *, int);

void send_file(struct global_data *, int);

int load(char *, struct database *);

int add(char *, struct database *);

int is_exist(char *, struct database *);

char **split_file(FILE *);

struct database split_files(char *);

int count_words(FILE *);

struct node_t parse_node(char *);

char *convert_node(struct node_t *);

int get_int_len(int);

int powBase10(int, int);

int str_to_int(char *);