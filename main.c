#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <cJSON.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_SERVERS 3
#define MAX_BUFFER_SIZE 4096

//=========================================================
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

//==========================================================
struct Server
{
    char *ip;
    int port;
};

//================================================================================
struct Security
{
    const char *criteria;
    int max_per_user;
    char *block;
    char *hold;
};

//=================================================================================
struct Config
{
    const char *strategy;
    int port;
    struct Server *servers;
    size_t num_servers;
    struct Security security;
};

//==================================================================================
void freeConfig(struct Config *config)
{
    free(config->servers);
}

//=====================================================================================
char *readConf()
{
    FILE *file;
    const char *filePath = "eg.json";
    file = fopen(filePath, "r");
    if (file == NULL)
    {
        error("Error in opening file.");
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = (char *)malloc(file_size + 1);
    if (content == NULL)
    {
        fclose(file);
        error("Error allocating memory.");
        return "";
    }
    size_t read_size = fread(content, 1, file_size, file);
    if (read_size != (size_t)file_size)
    {
        fclose(file);
        free(content);
        error("Error reading file.");
        return "";
    }
    content[file_size] = '\0';
    fclose(file);
    return content;
}

//===================================================================================
void parseJSONdata(const cJSON *json, struct Config *config)
{
    cJSON *strategy = cJSON_GetObjectItemCaseSensitive(json, "strategy");
    cJSON *port = cJSON_GetObjectItemCaseSensitive(json, "port");
    cJSON *servers = cJSON_GetObjectItemCaseSensitive(json, "servers");
    cJSON *security = cJSON_GetObjectItemCaseSensitive(json, "security");

    config->strategy = strategy ? strategy->valuestring : "round-robin";
    config->port = port ? port->valueint : 443;

    config->num_servers = cJSON_GetArraySize(servers);
    config->servers = malloc(config->num_servers * sizeof(struct Server));
    for (size_t i = 0; i < config->num_servers; i++)
    {
        cJSON *server = cJSON_GetArrayItem(servers, i);
        config->servers[i].ip = cJSON_GetObjectItemCaseSensitive(server, "domain")->valuestring;
        config->servers[i].port = cJSON_GetObjectItemCaseSensitive(server, "port")->valueint;
    }

    cJSON *criteria = cJSON_GetObjectItemCaseSensitive(security, "criteria");
    cJSON *max_per_user = cJSON_GetObjectItemCaseSensitive(security, "max_per_user");
    cJSON *block = cJSON_GetObjectItemCaseSensitive(security, "block");
    cJSON *hold = cJSON_GetObjectItemCaseSensitive(security, "hold");

    config->security.criteria = criteria->valuestring;
    config->security.block = block->valuestring;
    config->security.hold = hold->valuestring;
    config->security.max_per_user = max_per_user->valueint;
}

//=================================================================================
SSL_CTX *init_openssl()
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

//====================================================================================
int create_socket(const char *hostname, const int port)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Error in creating sockets.");
    }
    char *host = "www.google.com";
    server = gethostbyname(host);
    if (server == NULL)
    {
        error("Error, no such host.");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("Error in connecting.");
    }

    return sockfd;
}

//====================================================================================
void *handleConnection(void *args)
{
    SSL_CTX *ssl_ctx = init_openssl();
    int newSockfd = *((int *)args);
    struct Server s1 = {"127.0.0.1", 3000 };
    int revSockfd = create_socket(s1.ip, s1.port);

    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, revSockfd);
    if (SSL_connect(ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        close(newSockfd);
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
        error("Error during SSL connection.");
    }

    printf("Reverse socket in created.\n");
    char buffer[MAX_BUFFER_SIZE];

    size_t bytes_received, bytes_sent;

    printf("Going to read all data.\n");
    while ((bytes_received = read(newSockfd, buffer, sizeof(buffer))) > 0)
    {
        SSL_write(ssl, buffer, bytes_received);
        if (bytes_received < sizeof(buffer))
        {
            break;
        }
    }

    printf("Sending is done.\n");

    while ((bytes_received = SSL_read(ssl, buffer, sizeof(buffer))) > 0)
    {
        send(newSockfd, buffer, bytes_received, 0);
        if (bytes_received < sizeof(buffer))
        {
            break;
        }
    }

    if (bytes_received < 0)
    {
        error("Error during communication.");
    }

    close(newSockfd);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ssl_ctx);
    // pthread_exit(NULL);
}

//====================================================================================
void printConfig(const struct Config *config)
{
    printf("Strategy: %s\n", config->strategy);
    printf("Port: %d\n", config->port);

    printf("Servers:\n");
    for (size_t i = 0; i < config->num_servers; i++)
    {
        printf("  Domain: %s, Port: %d\n", config->servers[i].ip, config->servers[i].port);
    }

    printf("Security:\n");
    printf("  Criteria: %s\n", config->security.criteria);
    printf("  Max Per User: %d\n", config->security.max_per_user);
    printf("  Block: %s\n", config->security.block);
    printf("  Hold: %s\n", config->security.hold);
}

//====================================================================================
//====================================================================================
int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        error("Error, no port number given.\n");
    }

    // Firstly read the eg.json file.
    char *configurationString = readConf();

    // Parse the configuration string.
    cJSON *json = cJSON_Parse(configurationString);
    if (json == NULL)
    {
        error("Error in parsing the configuration file.");
    }

    // Memory free to prevent memory leak.
    free(configurationString);
    struct Config config;
    parseJSONdata(json, &config);
    // printConfig(&config);

    struct Server server = {"127.0.0.1", atoi(argv[1])};
    printf("Server listening over %s on port %d.\n\n", server.ip, server.port);
    printf("Starting server...\n\n");

    // Sockfd to store file descriptor.
    int sockfd, portno, n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Error opening socket.\n");
    }

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server.ip);
    serv_addr.sin_port = htons(server.port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("Binding failed.");
    }

    listen(sockfd, 5);

    while (1)
    {
        printf("Waiting for a socket connection....\n");
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        printf("New socket accepted.\n");
        if (newsockfd < 0)
        {
            error("Error in accepting the connections.\n");
        }

        // pthread_t pid;
        handleConnection((void *)&newsockfd);
        // if (pthread_create(&pid, NULL, handleConnection, &newsockfd) != 0)
        // {
        //     error("Error creating thread.");
        // }
        // printf("Thread created.\n\n");
        // pthread_detach(pid);
    }

    freeConfig(&config);
    cJSON_Delete(json);
    return 0;
}
