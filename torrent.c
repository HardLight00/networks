// Created by ilia on 27/03/19.
#include "torrent.h"

#define MY_IP_ADDRESS "10.91.45.245"
#define SERVER_PORT 2000

int main(int argc, char **argv) {
    struct global_data gl_data;
    struct database files, nodes, connections, blacklist, arguments;

    // init databases
    files = init_db("known_files.txt");
    nodes = init_db("known_nodes.txt");
    connections = init_db(NULL);
    blacklist = init_db(NULL);
    arguments = init_db(NULL);

    // automate testing
    arguments.count = argc;
    for (int i = 0; i < arguments.count; i++) {
        add(argv[i], &arguments);
    }

    // init the global data
    gl_data.files = &files;
    gl_data.nodes = &nodes;
    gl_data.connections = &connections;
    gl_data.blacklist = &blacklist;
    gl_data.args = &arguments;

    // execute client and server code
    pthread_t client;
    pthread_create(&client, 0, client_thread, &gl_data);
    server_thread(&gl_data);
    pthread_join(client, 0);

    return 0;
}

void *client_thread(void *data_void) {
    struct global_data *gl_data = (struct global_data *) data_void;

    int socket_fd;
    struct sockaddr_in service_addr;

    // socket create and verification
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        printf("Socket creation failed...\n");
        exit(1);
    }
    bzero(&service_addr, sizeof(service_addr));

    printf("Welcome to shit torrent\n");

    char node_info[MAX_NAME_LENGTH + INFO_LENGTH];
    printf("Enter the node information in format name:ip_address:port\n");
    scanf("%s", node_info);
//    strcpy(node_info, gl_data->args->data[1]);
    struct node_t node = parse_node(node_info);

    service_addr.sin_family = AF_INET;
    service_addr.sin_addr.s_addr = inet_addr(node.ip_address);
    service_addr.sin_port = htons(node.port);

    if (connect(socket_fd, (struct sockaddr *) &service_addr, sizeof(service_addr)) != 0) {
        printf("Connection with the server failed...\n");
        exit(1);
    }

    struct net_info net;
    net.socket_fd = socket_fd;
    net.sockaddrIn = service_addr;

    char command[MAX_COMMAND_LENGTH];
    do {
        printf("Print the command\n");
        scanf("%s", command);
//    strcpy(command, gl_data->args->data[2]);
        if (strcmp(command, (const char *) SYN) == 0) {
            make_connection(gl_data, &net);
        } else if (strcmp(command, (const char *) REQUEST) == 0) {
            request_file(gl_data, &net);
        } else if (strcmp(command, (const char *) FLOOD) == 0) {
            request_file(gl_data, &net);
        } else if (strcmp(command, (const char *) EXIT_COMMAND) != 0) {
            printf("Wrong command, try again\n");
        }
    } while (strcmp(command, (const char *) EXIT_COMMAND) != 0);

    printf("Client die :)\n");
    sleep(2);

    close(socket_fd);

    return NULL;
}

void *server_thread(void *data_void) {
    struct global_data *gl_data = (struct global_data *) data_void;
    int socket_fd;
    struct sockaddr_in server_addr;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        printf("socket creation failed...\n");
        exit(1);
    }
    gl_data->socket_fd = socket_fd;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        printf("Socket bind failed\n");
        exit(1);
    }

    if ((listen(socket_fd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(1);
    }

    // start several servers
    pthread_t server[MAX_SERVER_CONNECTIONS];
    for (int i = 0; i < MAX_SERVER_CONNECTIONS; i++) {
        pthread_create(&server[i], 0, process_connection, &gl_data);
    }

    for (int i = 0; i < MAX_SERVER_CONNECTIONS; i++) {
        pthread_join(server[i], 0);
    }

    close(socket_fd);
    return NULL;
}

void *process_connection(void *data_void) {
    struct global_data *gl_data = (struct global_data *) data_void;
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));

    socklen_t addr_len = sizeof(client_addr);

    // Accept the data packet from client and verification
    int connect_fd = accept(gl_data->socket_fd, (struct sockaddr *) &client_addr, &addr_len);
    if (connect_fd < 0) {
        printf("server accept failed...\n");
        exit(1);
    }

    struct node_t client_node;
    client_node.ip_address = malloc(sizeof(char) * INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_node.ip_address, INET_ADDRSTRLEN);

    if (contain_ip(client_node.ip_address, gl_data->blacklist) != -1) {
        printf("Blocked a not-trustful client with ip: %s\n", client_node.ip_address);
        return NULL;
    }

    int num_bytes;
    char command[10];
    pthread_t thread_id = pthread_self();
    while (1) {
        num_bytes = recvfrom(gl_data->socket_fd, (char *) &command, sizeof(command), 0,
                             (struct sockaddr *) &client_addr, &addr_len);
        printf("Request got by server #%lu", thread_id);
        if (num_bytes > 0) {
            if (strcmp(command, SYN) == 0) {
                printf("Get SYN command from\n");
                accept_connection(gl_data, gl_data->socket_fd);
            } else if (strcmp(command, REQUEST) == 0) {
                printf("Get REQUEST command\n");
                send_file(gl_data, gl_data->socket_fd);
            }
            sleep(2);
            printf("Server done his work\n");
            num_bytes = 0;
        }
    }
    printf("Server fully done his work\n");
}

void make_connection(struct global_data *gl_data, struct net_info *server) {
    sendto(server->socket_fd, (char *) SYN, strlen(SYN), 0,
           (struct sockaddr *) &server->sockaddrIn, sizeof(server->sockaddrIn));
    sleep(1);

    //send "name:ip_address:port:[file1, file2 ... fileN]"
    char *my_node_info = malloc(sizeof(char) *
                                (strlen(MY_NAME) + strlen(MY_IP_ADDRESS) + (NUM_OF_SEPARATORS_PER_INFO + 1) +
                                 get_int_len(SERVER_PORT)));
    strcpy(my_node_info, MY_NAME);
    strcat(my_node_info, INFO_SEPARATOR);
    strcat(my_node_info, MY_IP_ADDRESS);
    strcat(my_node_info, INFO_SEPARATOR);
    char *server_port = malloc(sizeof(char) * get_int_len(SERVER_PORT));
    sprintf(server_port, "%d", SERVER_PORT);
    strcat(my_node_info, server_port);
    strcat(my_node_info, INFO_SEPARATOR);
    for (int i = 0; i < gl_data->files->count; i++) {
        my_node_info = realloc(my_node_info, strlen(my_node_info) + strlen(gl_data->files->data[i]) + 1);
        strcat(my_node_info, gl_data->files->data[i]);
        if (i < gl_data->files->count - 1)
            strcat(my_node_info, FILE_SEPARATOR);
    }

    char syn_info[strlen(my_node_info)];
    strcpy(syn_info, my_node_info);

    sendto(server->socket_fd, (char *) &syn_info, strlen(syn_info), 0,
           (struct sockaddr *) &server->sockaddrIn, sizeof(server->sockaddrIn));
    sleep(1);

    printf("Client trying make syn and send %s\n", syn_info);

    //send #nodes
    int num_nodes = gl_data->nodes->count;
    sendto(server->socket_fd, (char *) &num_nodes, sizeof(int), 0,
           (struct sockaddr *) &server->sockaddrIn, sizeof(server->sockaddrIn));
    sleep(1);

    // loop (send "name:ip_address:port)
    for (int i = 0; i < num_nodes; i++) {
        char cur_node[strlen(gl_data->nodes->data[i])];
        strcpy(cur_node, gl_data->nodes->data[i]);
        sendto(server->socket_fd, (char *) cur_node, strlen(cur_node), 0,
               (struct sockaddr *) &server->sockaddrIn, sizeof(server->sockaddrIn));
        printf("Client sent %s\n", gl_data->nodes->data[i]);
        sleep(1);
    }

    printf("Client done the synchronization success\n");
}

void request_file(struct global_data *gl_data, struct net_info *server) {
    char filename[MAX_NAME_LENGTH];
    printf("Enter the filename\n");
    scanf("%s", filename);
//    strcpy(filename, gl_data->args->data[3]);

    sendto(server->socket_fd, (char *) REQUEST, strlen(REQUEST), 0,
           (struct sockaddr *) &server->sockaddrIn, sizeof(server->sockaddrIn));
    sleep(1);

    //send_filename "filename.txt"
    sendto(server->socket_fd, (char *) &filename, strlen(filename), 0,
           (struct sockaddr *) &server->sockaddrIn, sizeof(server->sockaddrIn));
    sleep(1);

    //receive # words
    char ch_num_words[sizeof(int)];
    int num_words = 0;
    recvfrom(server->socket_fd, (char *) ch_num_words, sizeof(int), 0,
             (struct sockaddr *) &server->sockaddrIn, (socklen_t *) sizeof(server->sockaddrIn));
    num_words = atoi(ch_num_words);
    if (num_words != -1) {
        //loop receive words
        char *file_info = malloc(sizeof(char *));
        char *word;
        for (int i = 0; i < num_words; i++) {
            word = malloc(sizeof(char) * MAX_NAME_LENGTH * MAX_ENTITIES);
            recvfrom(server->socket_fd, (char *) word, sizeof(word), 0,
                     (struct sockaddr *) &server->sockaddrIn, (socklen_t *) sizeof(server->sockaddrIn));
            printf("Receive word %s \n\n", word);
            strcat(file_info, word);
            if (i < num_words - 1) {
                strcat(file_info, " ");
            }
        }
        add(filename, gl_data->files);
        FILE *file = fopen(filename, "w");

        if (file == NULL) {
            printf("File dont work correctly\n");
        } else {
            fprintf(file, "%s", file_info);
            fclose(file);
        }
        printf("Client done request file: %s successful\n", filename);
    } else {
        printf("Client try request file: %s but file does not exist\n", filename);
    }

}

void accept_connection(struct global_data *gl_data, int socket_fd) {
    struct sockaddr_in client_addr;
    char syn_data[MAX_NAME_LENGTH + INFO_LENGTH + (MAX_NAME_LENGTH + 1) * MAX_ENTITIES];

    socklen_t addr_len = sizeof(client_addr);

    memset(&client_addr, 0, sizeof(client_addr));
    printf("%lu\n", sizeof(syn_data));
    int num_bytes = recvfrom(socket_fd, (char *) syn_data, sizeof(syn_data), 0,
                             (struct sockaddr *) &client_addr, &addr_len);
    if (num_bytes == -1) {
        perror("Recvfrom failed");
        exit(1);
    }

    //parse data and write in database
    char syn_data_copy[sizeof(syn_data)];
    strcpy(syn_data_copy, syn_data);

    node_t node = parse_node(syn_data_copy);
    char *ch_node = convert_node(&node);
    add(ch_node, gl_data->nodes);

    int index;
    int num_connection;
    if ((index = is_exist(ch_node, gl_data->connections)) >= 0) {
        num_connection = increment_query(index, gl_data->connections);
        if (num_connection > MAX_SYN_REQUESTS) {
            add(ch_node, gl_data->blacklist);
            delete(ch_node, gl_data->connections);
            printf("Add not-trustful client in blacklist: %s\n", ch_node);
            return;
        } else {
            num_connection = increment_query(index, gl_data->connections);
            printf("Client: %s made his %d connection", ch_node, num_connection);
        }
    } else {
        add(ch_node, gl_data->connections);
        increment_query(index, gl_data->connections);
        printf("Client: %s made his first connection", ch_node);
    }

    strcpy(syn_data_copy, syn_data);
    struct database arr_files = split_files(syn_data_copy);
    for (int i = 0; i < arr_files.count; i++) {
        add(arr_files.data[i], gl_data->files);
    }

    printf("Server syn with %s\n", syn_data);

    int num_known_nodes;
    char ch_num_known_nodes[get_int_len(MAX_ENTITIES)];
    num_bytes = recvfrom(socket_fd, (char *) ch_num_known_nodes, sizeof(ch_num_known_nodes), 0,
                         (struct sockaddr *) &client_addr, &addr_len);
    if (num_bytes == -1) {
        perror("Recvfrom failed");
        exit(1);
    }
    num_known_nodes = str_to_int(ch_num_known_nodes);

    char cur_node[MAX_NAME_LENGTH + INFO_LENGTH];
    for (int i = 0; i < num_known_nodes; i++) {
        recvfrom(socket_fd, (char *) cur_node, sizeof(cur_node), 0,
                 (struct sockaddr *) &client_addr, &addr_len);
        add(cur_node, gl_data->nodes);
        printf("Server received %s\n", cur_node);
    }

    decrement_query(index, gl_data->connections);

    printf("Connection accepted successfully\n");
}

void send_file(struct global_data *gl_data, int socket_fd) {
    struct sockaddr_in client_addr;
    char filename[MAX_NAME_LENGTH];

    socklen_t addr_len = sizeof(client_addr);

    memset(&client_addr, 0, sizeof(client_addr));
    recvfrom(socket_fd, (char *) &filename, sizeof(filename), 0,
             (struct sockaddr *) &client_addr, &addr_len);

    struct node_t client_node;
    client_node.ip_address = malloc(sizeof(char) * INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_node.ip_address, INET_ADDRSTRLEN);

    if (contain_ip(client_node.ip_address, gl_data->connections) < 0) {
        printf("Client with ip: %s have not made a connection yet", client_node.ip_address);
        return;
    }

    // send # words and loop (send word)
    int file_index;
    int num_words = 0;
    char ch_num_words[sizeof(int)];
    if ((file_index = is_exist(filename, gl_data->files)) >= 0) {
        FILE *file = fopen(filename, "r");
        if (file == NULL) {
            printf("File dont work correctly\n");
        } else {
            increment_query(file_index, gl_data->files);
            num_words = count_words(file);
            sprintf(ch_num_words, "%d", num_words);
            sendto(socket_fd, (char *) &ch_num_words, strlen(ch_num_words), 0,
                   (struct sockaddr *) &client_addr, addr_len);
            sleep(1);
            char **words = split_file(file);
            for (int i = 0; i < num_words; i++) {
                char word[strlen(words[i])];
                strcpy(word, words[i]);
                sendto(socket_fd, (char *) &word, strlen(word), 0,
                       (struct sockaddr *) &client_addr, addr_len);
                printf("Server sent word: %s\n", word);
                sleep(1);
            }
            fclose(file);
        }
    } else {
        strcpy(ch_num_words, "-1");
        sendto(socket_fd, (char *) &ch_num_words, strlen(ch_num_words), 0,
               (struct sockaddr *) &client_addr, addr_len);
        sleep(1);
    }
}

int load(char *source_name, struct database *db) {
    char str[MAX_NAME_LENGTH * MAX_ENTITIES];
    FILE *fp;
    fp = fopen(source_name, "r");

    if (fp == NULL) {
        perror("Error while opening the file.\n");
        return -1;
    }

    fgets(str, sizeof(str), fp);
    char *token = strtok(str, FILE_SEPARATOR);

    while (token != NULL) {
        add(token, db);
        token = strtok(NULL, FILE_SEPARATOR);
    }

    fclose(fp);
    return db->count;
}

int add(char *entity, struct database *db) {
    if (is_exist(entity, db) < 0 || db->count == MAX_ENTITIES) {
        pthread_mutex_lock(&db->lock);

        db->queries = realloc(db->queries, (db->count + 1) * sizeof(int));
        db->queries[db->count] = 0;

        db->data = realloc(db->data, (db->count + 1) * (sizeof(char *)));
        db->data[db->count] = malloc(strlen(entity) * sizeof(char));
        strcpy(db->data[db->count++], entity);

        pthread_mutex_unlock(&db->lock);
        return db->count;
    } else {
        return -1;
    }
}

int delete(char *entity, struct database *db) {
    int index;
    if ((index = is_exist(entity, db)) >= 0) {
        pthread_mutex_lock(&db->lock);

        for (int i = index - 1; i < db->count - 1; i++) {
            db->queries[i] = db->queries[i - 1];
            db->data[i] = db->data[i + 1];
        }
        db->count--;

        db->queries = realloc(db->queries, (db->count) * sizeof(int));
        db->data = realloc(db->data, (db->count) * (sizeof(char *)));

        pthread_mutex_unlock(&db->lock);
        return db->count;
    } else {
        return -1;
    }
}

int is_exist(char *entity, struct database *db) {
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->data[i], entity) == 0)
            return i;
    }
    return -1;
}

char **split_file(FILE *file) {
    rewind(file);
    char **words = malloc(sizeof(char *));

    int count = 0;
    char str[MAX_NAME_LENGTH * MAX_ENTITIES];
    char *token;

    while (fgets(str, MAX_NAME_LENGTH * MAX_ENTITIES, file) != NULL) {
        printf("split file - %s\n", str);
        token = strtok(str, " ");
        while (token != NULL) {
            words = realloc(words, sizeof(char *) * (count + 1));
            words[count] = malloc(sizeof(char) * strlen(token));
            strcpy(words[count++], token);
            token = strtok(NULL, " ");
        }
    }

    return words;
}

struct database split_files(char *str) {
    struct database db;
    db.data = malloc(sizeof(char *));

    char *token;
    strtok(str, INFO_SEPARATOR);
    strtok(NULL, INFO_SEPARATOR);
    strtok(NULL, INFO_SEPARATOR);
    token = strtok(NULL, FILE_SEPARATOR);
    db.count = 0;
    while (token != NULL) {
        db.data = realloc(db.data, (db.count + 1) * sizeof(char *));
        db.data[db.count] = malloc(strlen(token) * sizeof(char));
        strcpy(db.data[db.count++], token);
        token = strtok(NULL, FILE_SEPARATOR);
    }

    return db;
}

int count_words(FILE *file) {
    if (file == NULL) {
        printf("Could not open file");
        exit(1);
    }

    rewind(file);

    int count = 0;
    char str[MAX_NAME_LENGTH * MAX_ENTITIES];
    char *token;

    while (fgets(str, MAX_NAME_LENGTH * MAX_ENTITIES, file) != NULL) {
        printf("count words - %s\n", str);
        token = strtok(str, " ");
        while (token != NULL) {
            count++;
            token = strtok(NULL, " ");
        }
    }

    return count;
}

struct node_t parse_node(char *node_info) {
    struct node_t node;
    char *token;

    token = strtok(node_info, INFO_SEPARATOR);
    node.name = malloc(sizeof(char) * strlen(token));
    strcpy(node.name, token);

    token = strtok(NULL, INFO_SEPARATOR);
    node.ip_address = malloc(sizeof(char) * strlen(token));
    strcpy(node.ip_address, token);

    token = strtok(NULL, INFO_SEPARATOR);
    char *ch_port = malloc(sizeof(char) * strlen(token));
    strcpy(ch_port, token);
    node.port = atoi(ch_port);

    return node;
}

char *convert_node(struct node_t *node) {
    int port_len = get_int_len(node->port);

    char *node_info = malloc(
            sizeof(char) * (strlen(node->name) + strlen(node->ip_address) + port_len + NUM_OF_SEPARATORS_PER_INFO));
    char *port = malloc(sizeof(char) * port_len);
    sprintf(port, "%d", node->port);

    strcpy(node_info, node->name);
    strcat(node_info, INFO_SEPARATOR);
    strcat(node_info, node->ip_address);
    strcat(node_info, INFO_SEPARATOR);
    strcat(node_info, port);

    return node_info;
}

struct database init_db(char *filename) {
    struct database db;
    db.data = malloc(sizeof(char *));
    db.queries = malloc(sizeof(int *));
    db.count = 0;
    if (pthread_mutex_init(&db.lock, NULL) != 0) {
        perror("DB mutex init has failed\n");
        exit(1);
    }
    if (filename != NULL) {
        int load_count = load(filename, &db);
        if (load_count > 0)
            db.count = load_count;
    }
    return db;
}

int increment_query(int index, struct database *db) {
    if (0 < index && index < db->count) {
        pthread_mutex_lock(&db->lock);
        db->queries[index]++;
        pthread_mutex_unlock(&db->lock);

        return db->queries[index];
    }
    return -1;
}

int decrement_query(int index, struct database *db) {
    if (0 < index && index < db->count) {
        pthread_mutex_lock(&db->lock);
        if (db->queries[index] > 0)
            db->queries[index]--;
        pthread_mutex_unlock(&db->lock);

        return db->queries[index];
    }
    return -1;
}

int contain_ip(char *ip_address, struct database *db) {
    node_t node;
    for (int i = 0; i < db->count; i++) {
        node = parse_node(db->data[i]);
        if (strcmp(node.ip_address, ip_address) == 0)
            return i;
    }
    return -1;
}

int get_int_len(int number) {
    int count = 0;
    while (number / powBase10(DECIMAL, count) > 0)
        count++;
    return count;
}

int powBase10(int base, int power) {
    int res = 1;
    while (power-- > 0)
        res *= base;
    return res;
}

int str_to_int(char *str) {
    int res = 0;
    int ind = 0;
    while (str[ind] != '\0' && ind < strlen(str)) {
        res *= 10;
        res += str[ind++];
    }
    return res;
}