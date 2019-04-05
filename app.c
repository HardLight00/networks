// Created by ilia on 04/04/19.
#include "app.h"

#define MY_IP_ADDRESS "10.240.20.163"
#define SERVER_PORT 7000

int main(int argc, char **argv) {
    struct global_data_t gl_data;
    struct node_database_t nodes = init_node_db();
    struct file_database_t files = init_file_db();
    struct blacklist_database_t blacklist = init_blacklist_db();
    struct arg_database_t args = init_args_db();

    // init databases
    FILE *known_nodes, *known_files;
    known_nodes = fopen("./database/known_nodes.txt", "r");
    known_files = fopen("./database/known_files.txt", "r");
    load_nodes(known_nodes, &nodes);
    load_files(known_files, &files);

    // automate testing
    load_args(argc, argv, &args);

    // init the global data
    gl_data.files = &files;
    gl_data.nodes = &nodes;
    gl_data.blacklist = &blacklist;
    gl_data.args = &args;

    // execute client and server code
    pthread_t client;

    pthread_create(&client, 0, client_thread, &gl_data);
//    pthread_create(&server, 0, server_thread, &gl_data);
    server_thread(&gl_data);
    pthread_join(client, 0);
//    pthread_join(server, 0);

    known_nodes = fopen("./database/known_nodes_res.txt", "wb");
    known_files = fopen("./database/known_files_res.txt", "wb");

    char *ch_node;
    for (int i = 0; i < gl_data.nodes->count; ++i) {
        ch_node = format_node(&gl_data.nodes->nodes[i]);
        fprintf(known_files, "%s\n", ch_node);
    }

    char *ch_file;
    for (int i = 0; i < gl_data.files->count; ++i) {
        ch_file = format_file(&gl_data.files->files[i]);
        fprintf(known_files, "%s\n", ch_file);
    }

    fclose(known_nodes);
    fclose(known_files);

    return 0;
}

void *client_thread(void *data_void) {
    struct global_data_t *gl_data = (struct global_data_t *) data_void;
    struct sockaddr_in service_addr;
    printf("Welcome to shit torrent\n");

    char node_info[MAX_LENGTH + INFO_LENGTH];
    printf("Enter the node information in format name:ip_address:port\n");
    if (gl_data->args->count < 2) {
        scanf("%s", node_info);
    } else {
        strcpy(node_info, gl_data->args->data[1]);
    }

    struct node_t node = parse_node(node_info);

    service_addr.sin_family = AF_INET;
    service_addr.sin_addr.s_addr = node.sockaddr->sin_addr.s_addr;
    service_addr.sin_port = (node.sockaddr->sin_port);

    int command = 0;
    do {
//        printf("Print the command\n");
        if (gl_data->args->count < 3) {
            scanf("%d", &command);
        } else {
            command = atoi(gl_data->args->data[2]);
        }

        if (command == SYN) {
            printf("Client sent SYN flag\n");
            make_connection(gl_data, &service_addr);
        } else if (command == REQUEST) {
            printf("Client sent REQUEST flag\n");
//            request_file(gl_data, &service_addr);
        } else if (command == FLOOD) {
            printf("Client sent FLOOD flag\n");
            flood_attack(gl_data, &service_addr);
        } else if (command != EXIT_COMMAND) {
            printf("Wrong command, try again\n");
        }
    } while (command != EXIT_COMMAND);
//    } while (strcmp(command, (const char *) EXIT_COMMAND) != 0 && terminate-- > 0);
    printf("Client die :)\n");

    return NULL;
}

void *server_thread(void *data_void) {
    struct global_data_t *gl_data = (struct global_data_t *) data_void;
    struct sockaddr_in server_addr;
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1)
        perror("Socket creation failed...\n");

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, MY_IP_ADDRESS, &(server_addr.sin_addr));
    server_addr.sin_port = SERVER_PORT;

    int enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        perror("Socket bind failed\n");

    if ((listen(socket_fd, 5)) != 0)
        perror("Listen failed...\n");

    printf("Server is ready\n");
    struct sockaddr_in client_addr;
    int command = 3;
    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t addr_len = sizeof(client_addr);

        // Accept the data packet from client and verification
        int connect_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &addr_len);
        if (connect_fd < 0)
            perror("server accept failed...\n");

        struct net_t net_info;
        net_info.socket_fd = connect_fd;
        net_info.sockaddr_in = client_addr;

        char *ip_addr = malloc(sizeof(char *));
        int num_bytes;

        if (is_contain_ip(&net_info.sockaddr_in.sin_addr, gl_data->blacklist) != -1) {
            inet_ntop(AF_INET, &(net_info.sockaddr_in.sin_addr), ip_addr, INET_ADDRSTRLEN);
            printf("Blocked a not-trustful client with ip: %s\n", ip_addr);
            close(connect_fd);
        } else {
            num_bytes = recvfrom(connect_fd, &command, sizeof(int), 0,
                                 (struct sockaddr *) &client_addr, &addr_len);
//            if (num_bytes > 0)
//                printf("%d RECEIVE SOME SHIT\n", command);
//            command = ntohl(command);

            if (num_bytes > 0) {
                if (command == 1) {
                    printf("Got SYN command\n");
                    accept_connection(gl_data, &net_info);
                    close(connect_fd);
                } else if (command == 0) {
                    printf("Got REQUEST command\n");
//                    send_file(gl_data, &net_info);
                }
                usleep(SLEEP_TIME);
            }
        }
        close(connect_fd);
    }
    printf("Server done his work\n");

    close(socket_fd);
    return NULL;
}

void make_connection(struct global_data_t *gl_data, struct sockaddr_in *dest) {
    int socket_fd;

    // socket create and verification
    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1)
        perror("Socket creation failed...\n");
//    bzero(&dest, sizeof(dest));

    if (connect(socket_fd, (struct sockaddr *) dest, sizeof(struct sockaddr)) != 0)
        perror("Connection with the server failed...\n");

    int command = htonl(SYN);
    sendto(socket_fd, &command, sizeof(int), 0,
           (struct sockaddr *) &dest, sizeof(struct sockaddr));
    usleep(SLEEP_TIME);

    char *syn_info = get_myself_info(gl_data);
    sendto(socket_fd, syn_info, MAX_LENGTH, 0, (struct sockaddr *) dest, sizeof(struct sockaddr));
    usleep(SLEEP_TIME);

//    pthread_mutex_lock(&gl_data->nodes->lock);
    int count = gl_data->nodes->count;
    int num_nodes = htonl(count);
    sendto(socket_fd, &num_nodes, sizeof(int), 0,
           (struct sockaddr *) dest, sizeof(struct sockaddr));
    usleep(SLEEP_TIME);

//    // loop (send "name:ip_address:port)
//    for (int i = 0; i < gl_data->nodes->count; i++) {
//        char *cur_node = convert_node(&gl_data->nodes->nodes[i]);
//        sendto(socket_fd, (char *) cur_node, strlen(cur_node), 0,
//               (struct sockaddr *) &dest, sizeof(struct sockaddr));
//        printf("Client sent %s\n", cur_node);
//        usleep(SLEEP_TIME);
//    }
//    pthread_mutex_unlock(&gl_data->nodes->lock);
    close(socket_fd);
    printf("done sync\n");
}

//void request_file(struct global_data_t *gl_data, struct sockaddr_in *dest) {
//
//}

void flood_attack(struct global_data_t *gl_data, struct sockaddr_in *dest) {
    int socket_fd;
    while (1) {
        // socket create and verification
        socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_fd == -1)
            perror("Socket creation failed...\n");
//    bzero(&dest, sizeof(dest));

        if (connect(socket_fd, (struct sockaddr *) dest, sizeof(struct sockaddr)) != 0)
            perror("Connection with the server failed...\n");

        int command = htonl(SYN);
        sendto(socket_fd, &command, sizeof(int), 0,
               (struct sockaddr *) &dest, sizeof(struct sockaddr));
        sleep(1);
//        usleep(SLEEP_TIME);

        close(socket_fd);
        printf("Client flood\n");
    }
}

void accept_connection(struct global_data_t *gl_data, struct net_t *net) {
    int index;
    int num_connection;
    char *ch_node;
    char *ip_addr = malloc(sizeof(char *));
    inet_ntop(AF_INET, &(net->sockaddr_in.sin_addr), ip_addr, INET_ADDRSTRLEN);
    if (is_contain_ip(&net->sockaddr_in.sin_addr, gl_data->blacklist) >= 0) {
        printf("Blocked ddoser\n");
        return;
    }


    if ((index = is_exist_node(&net->sockaddr_in, gl_data->nodes)) >= 0) {
        if (is_contain_ip(&net->sockaddr_in.sin_addr, gl_data->blacklist) >= 0) {
            printf("Blocked ddoser\n");
            return;
        }

        num_connection = increment_query(index, gl_data->nodes);
        if (num_connection == MAX_SYN_REQUESTS) {
            increment_query(index, gl_data->nodes);
            add_in_blacklist(net->sockaddr_in.sin_addr, gl_data->blacklist);
//            delete_node(&net->sockaddr_in, gl_data->nodes);
            printf("Add not-trustful client in blacklist: %s\n", ip_addr);
            return;
        } else if (num_connection > MAX_SYN_REQUESTS) {
            printf("Refuse ddoser %s\n", ip_addr);
        } else {
            printf("Client: %s:%d sent his %d syn\n", ip_addr, net->sockaddr_in.sin_port, num_connection);
        }
    } else {
        if (is_contain_ip(&net->sockaddr_in.sin_addr, gl_data->blacklist) >= 0) {
            printf("Blocked ddoser\n");
            return;
        }

        struct node_t client_node;
        client_node.name = DEFAULT_NAME;
        client_node.sockaddr = &net->sockaddr_in;
        index = add_node(&client_node, gl_data->nodes) - 1;
        increment_query(index, gl_data->nodes);
        printf("Client: %s:%d sent his first syn\n", ip_addr, net->sockaddr_in.sin_port);
    }

    char syn_data[MAX_LENGTH];
    struct sockaddr client_addr;
    socklen_t addr_len = sizeof(client_addr);

    memset(&client_addr, 0, sizeof(client_addr));
    int num_bytes = recvfrom(net->socket_fd, (char *) syn_data, sizeof(syn_data), 0,
                             (struct sockaddr *) &client_addr, &addr_len);

    if (num_bytes == -1 || strlen(syn_data) < INFO_LENGTH)
        return;

    //parse data and write in database
    node_t node = parse_node(syn_data);
    add_node(&node, gl_data->nodes);

    int start = gl_data->files->count;
    split_files(node, syn_data, gl_data->files);
    int end = gl_data->files->count;

    printf("Server syn with %s\n", syn_data);

    int num_known_nodes = 0;
    num_bytes = recvfrom(net->socket_fd, &num_known_nodes, sizeof(int), 0,
                         (struct sockaddr *) &client_addr, &addr_len);
    if (num_bytes == -1)
        perror("Recvfrom failed");

    num_known_nodes = ntohl(num_known_nodes);
    printf("Client know %d nodes\n", num_known_nodes);

//    char cur_node[MAX_LENGTH];
//    node_t known_node;
//    for (int i = 0; i < num_known_nodes; i++) {
//        num_bytes = recvfrom(net->socket_fd, (char *) cur_node, sizeof(cur_node), 0,
//                             (struct sockaddr *) &client_addr, &addr_len);
//        if (num_bytes == -1)
//            perror("Recvfrom failed");
//
//        known_node = parse_node(ch_node);
//        add_node(&known_node, gl_data->nodes);
//        printf("Server received %s\n", cur_node);
//    }

    decrement_query(index, gl_data->nodes);

    printf("Connection accepted successfully\n");
}

//void send_file(struct global_data_t *gl_data, struct net_t *net) {
//
//}

char *get_myself_info(struct global_data_t *gl_data) {
    char *my_node_info = malloc(sizeof(char *));
    char *server_port = malloc(sizeof(int));
    sprintf(server_port, "%d", SERVER_PORT);

    my_node_info = realloc(my_node_info, sizeof(char) * (strlen(MY_NAME) + strlen(my_node_info)));
    strcpy(my_node_info, MY_NAME);

    my_node_info = realloc(my_node_info, sizeof(char) * (strlen(INFO_SEPARATOR) + strlen(my_node_info)));
    strcat(my_node_info, INFO_SEPARATOR);

    my_node_info = realloc(my_node_info, sizeof(char) * (strlen(MY_IP_ADDRESS) + strlen(my_node_info)));
    strcat(my_node_info, MY_IP_ADDRESS);

    my_node_info = realloc(my_node_info, sizeof(char) * (strlen(INFO_SEPARATOR) + strlen(my_node_info)));
    strcat(my_node_info, INFO_SEPARATOR);

    my_node_info = realloc(my_node_info, sizeof(char) * (strlen(server_port) + strlen(my_node_info)));
    strcat(my_node_info, server_port);

    my_node_info = realloc(my_node_info, sizeof(char) * (strlen(INFO_SEPARATOR) + strlen(my_node_info)));
    strcat(my_node_info, INFO_SEPARATOR);

    pthread_mutex_lock(&gl_data->files->lock);
    for (int i = 0; i < gl_data->files->count; i++) {
        my_node_info = realloc(my_node_info,
                               sizeof(char) *
                               (strlen(my_node_info) + strlen(gl_data->files->files[i].name) + strlen(FILE_SEPARATOR)));
        strcat(my_node_info, gl_data->files->files[i].name);
        if (i < gl_data->files->count - 1)
            strcat(my_node_info, FILE_SEPARATOR);
    }
    pthread_mutex_unlock(&gl_data->files->lock);
    return my_node_info;
}

char *format_node(struct node_t *node) {
    char *str;
    char *ip_address;
    int n_port;
    char *s_port = malloc(sizeof(int));

    ip_address = malloc(sizeof(char) * INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(node->sockaddr->sin_addr), ip_address, INET_ADDRSTRLEN);

    n_port = node->sockaddr->sin_port;
    sprintf(s_port, "%d", n_port);

    str = malloc(strlen("{"));
    strcpy(str, "{");

    str = realloc(str, strlen("name:") + strlen(str));
    strcat(str, "name:");

    str = realloc(str, strlen(node->name) + strlen(str));
    strcat(str, node->name);

    str = realloc(str, strlen(", ") + strlen(str));
    strcat(str, ", ");

    str = realloc(str, strlen("ip:") + strlen(str));
    strcat(str, "ip:");

    str = realloc(str, strlen(ip_address) + strlen(str));
    strcat(str, ip_address);

    str = realloc(str, strlen(", ") + strlen(str));
    strcat(str, ", ");

    str = realloc(str, strlen("port:") + strlen(str));
    strcat(str, "port:");

    str = realloc(str, sizeof(n_port) + strlen(str));
    strcat(str, s_port);

    str = realloc(str, strlen("}") + strlen(str));
    strcat(str, "}");

    return str;
}

struct node_t unformat_node(char *str_node) {
    struct node_t node;
    char *token;

    char *s_node = malloc(sizeof(char) * strlen(str_node));
    strcpy(s_node, str_node);

//    token = strtok(s_node, "{");
    token = strtok(s_node, ":");
    token = strtok(NULL, ", ");
    node.name = malloc(sizeof(char) * strlen(token));
    strcpy(node.name, token);

    strtok(NULL, ":");
    token = strtok(NULL, ", ");
    node.sockaddr = malloc(sizeof(struct sockaddr_in));
    inet_pton(AF_INET, token, &(node.sockaddr->sin_addr));

    strtok(NULL, ":");
    token = strtok(NULL, "}");
    node.sockaddr->sin_port = atoi(token);

    return node;
}

char *format_file(struct file_t *file) {
    char *str;
    char *name;
    char *s_node;
    struct node_t *node;

    name = malloc(sizeof(char) * strlen(file->name));
    strcpy(name, file->name);

    node = file->owner;

    str = malloc(sizeof(char *));
    strcpy(str, "{");

    str = realloc(str, strlen("name:") + strlen(str));
    strcat(str, "name:");

    str = realloc(str, strlen(node->name) + strlen(str));
    strcat(str, node->name);

    str = realloc(str, strlen(", ") + strlen(str));
    strcat(str, ", ");

    s_node = format_node(node);

    str = realloc(str, strlen(s_node) + strlen(str));
    strcat(str, s_node);

    str = realloc(str, strlen("}") + strlen(str));
    strcat(str, "}");

    return str;
}

struct file_t unformat_file(char *str_file) {
    struct file_t file;
    struct node_t node;
    char *token;

    char *s_file = malloc(sizeof(char) * strlen(str_file));
    strcpy(s_file, str_file);

//    strtok(s_file, "{");
    strtok(s_file, ":");
    token = strtok(NULL, ", ");
    file.name = malloc(sizeof(char) * strlen(token));
    strcpy(file.name, token);

    token = strtok(NULL, ":");
    token = strtok(NULL, "}}");
    node = unformat_node(token);
    file.owner = &node;

    return file;
}

void load_nodes(FILE *file, struct node_database_t *node_db) {
    if (file == NULL)
        perror("Could not open file");


    rewind(file);

    char str[MAX_LENGTH];
    while (fgets(str, sizeof(str), file) != NULL) {
        char *token = strtok(str, "\n");

        node_t current_node;
        while (token != NULL) {
            current_node = unformat_node(token);
            add_node(&current_node, node_db);
            token = strtok(NULL, "\n");
        }
    }
    fclose(file);
}

void load_files(FILE *file, struct file_database_t *file_db) {
    if (file == NULL)
        perror("Could not open file");

    rewind(file);

    char str[MAX_LENGTH];
    while (fgets(str, sizeof(str), file) != NULL) {
        char *token = strtok(str, "\n");

        file_t current_file;
        while (token != NULL) {
            current_file = unformat_file(token);
            add_file(&current_file, file_db);
            token = strtok(NULL, "\n");
        }
    }

    fclose(file);
}

void load_args(int argc, char **argv, struct arg_database_t *arg_db) {
    pthread_mutex_lock(&arg_db->lock);
    for (int i = 0; i < argc; i++) {
        arg_db->data = realloc(arg_db->data, (arg_db->count + 1) * (sizeof(char *)));
        arg_db->data[arg_db->count] = malloc(strlen(argv[i]) * sizeof(char));
        strcpy(arg_db->data[arg_db->count++], argv[i]);

        pthread_mutex_unlock(&arg_db->lock);
    }
}

struct node_database_t init_node_db() {
    struct node_database_t db;
    db.nodes = malloc(sizeof(struct node_t *));
    db.queries = malloc(sizeof(int *));
    db.count = 0;
    if (pthread_mutex_init(&db.lock, NULL) != 0)
        perror("DB mutex init has failed\n");

    return db;
}

struct file_database_t init_file_db() {
    struct file_database_t file_db;
    file_db.files = malloc(sizeof(struct file_t *));
    file_db.count = 0;
    if (pthread_mutex_init(&file_db.lock, NULL) != 0)
        perror("DB mutex init has failed\n");
    return file_db;
}

struct blacklist_database_t init_blacklist_db() {
    struct blacklist_database_t blacklist_db;
    blacklist_db.addresses = malloc(sizeof(struct in_addr *));
    blacklist_db.count = 0;
    if (pthread_mutex_init(&blacklist_db.lock, NULL) != 0)
        perror("DB mutex init has failed\n");
    return blacklist_db;
}

struct arg_database_t init_args_db() {
    struct arg_database_t arg_db;
    arg_db.data = malloc(sizeof(char *));
    arg_db.count = 0;
    if (pthread_mutex_init(&arg_db.lock, NULL) != 0)
        perror("DB mutex init has failed\n");
    return arg_db;
}

int add_node(struct node_t *node, struct node_database_t *node_db) {
    pthread_mutex_lock(&node_db->lock);
    if (is_exist_node(node->sockaddr, node_db) < 0 && node_db->count < MAX_ENTITIES) {

        node_db->queries = realloc(node_db->queries, (node_db->count + 1) * sizeof(int));
        node_db->queries[node_db->count] = 0;

        node_db->nodes = realloc(node_db->nodes, (node_db->count + 1) * (sizeof(struct node_t)));
        node_db->nodes[node_db->count].sockaddr = node->sockaddr;
        node_db->nodes[node_db->count].name = malloc(sizeof(char) * strlen(node->name));
        strcpy(node_db->nodes[node_db->count++].name, node->name);

        pthread_mutex_unlock(&node_db->lock);
        return node_db->count;
    } else {
        pthread_mutex_unlock(&node_db->lock);
        return -1;
    }
}

int add_file(struct file_t *file, struct file_database_t *file_db) {
    if (is_exist_file(file, file_db) < 0 && file_db->count < MAX_ENTITIES) {
        pthread_mutex_lock(&file_db->lock);

        file_db->files = realloc(file_db->files, (file_db->count + 1) * sizeof(struct file_t));
        file_db->files[file_db->count].name = malloc(sizeof(char) * strlen(file->name));
        strcpy(file_db->files[file_db->count].name, file->name);
        file_db->files[file_db->count++].owner = file->owner;

        pthread_mutex_unlock(&file_db->lock);
        return file_db->count;
    } else {
        return -1;
    }
}

int add_in_blacklist(struct in_addr ip_addr, struct blacklist_database_t *blacklist_db) {
    if (is_contain_ip(&ip_addr, blacklist_db) < 0 && blacklist_db->count < MAX_ENTITIES) {
        pthread_mutex_lock(&blacklist_db->lock);

        blacklist_db->addresses = realloc(blacklist_db->addresses, sizeof(blacklist_db->addresses) + sizeof(ip_addr));
        blacklist_db->addresses[blacklist_db->count++] = (ip_addr);

        pthread_mutex_unlock(&blacklist_db->lock);
        return blacklist_db->count;
    } else {
        return -1;
    }
}

int delete_node(struct sockaddr_in *sockaddr, struct node_database_t *node_db) {
    int index;
    if ((index = is_exist_node(sockaddr, node_db)) >= 0) {
        pthread_mutex_lock(&node_db->lock);

        for (int i = index; i < node_db->count - 1; i++) {
            node_db->queries[i] = node_db->queries[i + 1];
            node_db->nodes[i] = node_db->nodes[i + 1];
        }
        node_db->count--;

        node_db->queries = realloc(node_db->queries, (node_db->count) * sizeof(int *));
//        node_db->nodes = realloc(node_db->nodes, sizeof(node_db->nodes) - sizeof(struct node_t));

        pthread_mutex_unlock(&node_db->lock);
        return node_db->count;
    } else {
        return -1;
    }
}

int is_exist_node(struct sockaddr_in *sockaddr, struct node_database_t *node_db) {
    for (int i = 0; i < node_db->count; i++) {
        if (&sockaddr->sin_addr == &node_db->nodes[i].sockaddr->sin_addr
            && &sockaddr->sin_port == &node_db->nodes[i].sockaddr->sin_port)
            return i;
    }
    return -1;
}

int is_exist_file(struct file_t *file, struct file_database_t *file_db) {
    for (int i = 0; i < file_db->count; i++) {
        if (strcmp(file->name, file_db->files[i].name) == 0
            && &file->owner == &file_db->files[i].owner)
            return i;
    }
    return -1;
}

int is_contain_ip(struct in_addr *ip_addr, struct blacklist_database_t *blacklist_db) {
    for (int i = 0; i < blacklist_db->count; i++) {
        if (&ip_addr->s_addr == &blacklist_db->addresses[i].s_addr)
            return i;
    }
    return -1;
}

int count_words(FILE *file) {
    if (file == NULL)
        printf("Could not open file");

    rewind(file);

    int count = 0;
    char str[MAX_LENGTH * MAX_ENTITIES];
    char *token;

    while (fgets(str, MAX_LENGTH * MAX_ENTITIES, file) != NULL) {
        printf("count words - %s\n", str);
        token = strtok(str, " ");
        while (token != NULL) {
            count++;
            token = strtok(NULL, " ");
        }
    }

    return count;
}

void split_files(struct node_t owner, char *data, struct file_database_t *file_db) {
    char *str = malloc(sizeof(char) * strlen(data));
    strcpy(str, data);

    pthread_mutex_lock(&file_db->lock);
    char *token;
    strtok(str, INFO_SEPARATOR);
    strtok(NULL, INFO_SEPARATOR);
    strtok(NULL, INFO_SEPARATOR);
    token = strtok(NULL, FILE_SEPARATOR);
    file_db->count = 0;
    struct file_t file;
    while (token != NULL) {
        strcpy(file.name, token);
        file.owner = &owner;
        add_file(&file, file_db);
        token = strtok(NULL, FILE_SEPARATOR);
    }
    pthread_mutex_unlock(&file_db->lock);
}

struct node_t parse_node(char *s_node) {
    struct node_t node;
    char *token;

    char *str_node = malloc(sizeof(char) * strlen(s_node));
    strcpy(str_node, s_node);

    token = strtok(str_node, INFO_SEPARATOR);
    node.name = malloc(sizeof(char) * strlen(token));
    strcpy(node.name, token);

    token = strtok(NULL, INFO_SEPARATOR);
    node.sockaddr = malloc(sizeof(struct sockaddr_in));
    inet_pton(AF_INET, token, &(node.sockaddr->sin_addr));

    token = strtok(NULL, INFO_SEPARATOR);
    node.sockaddr->sin_port = atoi(token);

    return node;
}

char *convert_node(struct node_t *node) {
    char *str;
    char *ip_address;
    int n_port;
    char *s_port = malloc(sizeof(int));

    ip_address = malloc(sizeof(char) * INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(node->sockaddr->sin_addr), ip_address, INET_ADDRSTRLEN);

    n_port = node->sockaddr->sin_port;
    sprintf(s_port, "%d", n_port);

    str = malloc(strlen(node->name));
    strcpy(str, node->name);

    str = realloc(str, strlen(INFO_SEPARATOR) + strlen(str));
    strcat(str, INFO_SEPARATOR);

    str = realloc(str, strlen(ip_address) + strlen(str));
    strcat(str, ip_address);

    str = realloc(str, strlen(INFO_SEPARATOR) + strlen(str));
    strcat(str, INFO_SEPARATOR);

    str = realloc(str, sizeof(n_port) + strlen(str));
    strcat(str, s_port);

    return str;
}

int increment_query(int index, struct node_database_t *node_db) {
    if (0 <= index && index < node_db->count) {
        pthread_mutex_lock(&node_db->lock);

        node_db->queries[index]++;
        pthread_mutex_unlock(&node_db->lock);

        return node_db->queries[index];
    }
    return -1;
}

int decrement_query(int index, struct node_database_t *node_db) {
    if (0 < index && index < node_db->count) {
        pthread_mutex_lock(&node_db->lock);

        if (node_db->queries[index] > 0)
            node_db->queries[index]--;

        pthread_mutex_unlock(&node_db->lock);

        return node_db->queries[index];
    }
    return -1;
}
