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

#define MAX_SERVERS 3
#define MAX_BUFFER_SIZE 4096

//=========================================================
void error(const char *msg)
{                // Done
    perror(msg); // Done
    exit(1);     // Done
}

//==========================================================
struct Server
{             // Done
    char* ip; // Done
    int port; // Done
};


//================================================================================
struct Security
{                         // Done
    const char *criteria; // Done
    int max_per_user;     // Done
    char *block;          // Done
    char *hold;           // Done
};

//=================================================================================
struct Config
{                             // Done
    const char *strategy;     // Done
    int port;                 // Done
    struct Server *servers;   // Done
    size_t num_servers;       // Done
    struct Security security; // Done
};

//==================================================================================
void freeConfig(struct Config *config)
{                          // Done
    free(config->servers); // Done
}

//=====================================================================================
char *readConf()
{                                     // Done
    FILE*file;                       // Done
    const char *filePath = "eg.json"; // Done
    file = fopen(filePath, "r");      // Done
    if (file == NULL)
    {                                    // Done
        error("Error in opening file."); // Done
    }
    fseek(file, 0, SEEK_END);                      // Done
    long file_size = ftell(file);                  // Done
    fseek(file, 0, SEEK_SET);                      // Done
    char *content = (char *)malloc(file_size + 1); // Done
    if (content == NULL)
    {                                                      // Done
        fclose(file);                                      // Done
        error("Error allocating memory.");                 // Done
        return "";                                         // Done
    }                                                      // Done
    size_t read_size = fread(content, 1, file_size, file); // Done
    if (read_size != (size_t)file_size)
    {                                 // Done
        fclose(file);                 // Done
        free(content);                // Done
        error("Error reading file."); // Done
        return "";                    // Done
    }
    content[file_size] = '\0'; // Done
    fclose(file);              // Done
    return content;            // Done
}

//===================================================================================
void parseJSONdata(const cJSON *json, struct Config *config)
{                                                                         // Done
    cJSON *strategy = cJSON_GetObjectItemCaseSensitive(json, "strategy"); // Done
    cJSON *port = cJSON_GetObjectItemCaseSensitive(json, "port");         // Done
    cJSON *servers = cJSON_GetObjectItemCaseSensitive(json, "servers");   // Done
    cJSON *security = cJSON_GetObjectItemCaseSensitive(json, "security"); // Done

    config->strategy = strategy ? strategy->valuestring : "round-robin"; // Done
    config->port = port ? port->valueint : 443;                          // Done

    config->num_servers = cJSON_GetArraySize(servers);                     // Done
    config->servers = malloc(config->num_servers * sizeof(struct Server)); // Done
    for (size_t i = 0; i < config->num_servers; i++)
    {                                                   // Done
        cJSON *server = cJSON_GetArrayItem(servers, i); // Done
        config->servers[i].ip = cJSON_GetObjectItemCaseSensitive(server, "domain")->valuestring;
        config->servers[i].port = cJSON_GetObjectItemCaseSensitive(server, "port")->valueint;
    }

    cJSON *criteria = cJSON_GetObjectItemCaseSensitive(security, "criteria"); // Done
    cJSON *max_per_user = cJSON_GetObjectItemCaseSensitive(security, "max_per_user");
    cJSON *block = cJSON_GetObjectItemCaseSensitive(security, "block"); // Done
    cJSON *hold = cJSON_GetObjectItemCaseSensitive(security, "hold");   // Done

    config->security.criteria = criteria->valuestring;      // Done
    config->security.block = block->valuestring;            // Done
    config->security.hold = hold->valuestring;              // Done
    config->security.max_per_user = max_per_user->valueint; // Done
}

//=================================================================================
int create_socket(const char* hostname, const int port){
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        error("Error in creating sockets.");
    }
    server = gethostbyname(hostname);
    if(server == NULL){
        error("Error, no such host.");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(port);
    printf("Going other side.\n\n");
    if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
        error("Error in connecting.");
    }

    return sockfd;
}

//====================================================================================
void *handleConnection(void *args)
{
    printf("Inside the handle.\n");
    int newSockfd = *((int *)args);
    struct Server s1 = { "127.0.0.1", 3000 };
    // int revSockfd = create_socket(s1.ip, s1.port);
    printf("Reverse socket in created.\n");
    char buffer[MAX_BUFFER_SIZE];
    
    size_t bytes_received, bytes_sent;

    char url[256];
    char method[16];
    char http_version[16];

    read(newSockfd,buffer, sizeof(buffer));

    sscanf(buffer, "%s %255s %s",method, url, http_version);

    printf("%s.\n",url);
    // printf("Going to read all data.\n");
    // while((bytes_received = read(newSockfd, buffer, sizeof(buffer))) > 0 ){
    //     send(revSockfd, buffer, bytes_received, 0);
    //     if(bytes_received < sizeof(buffer)){
    //         break;
    //     }
    // }

    // printf("Sending is done.\n");

    // while((bytes_received = recv(revSockfd, buffer, sizeof(buffer), 0)) > 0 ){
    //     send(newSockfd, buffer, bytes_received, 0);
    //     if(bytes_received < sizeof(buffer)){
    //         break;
    //     }
    // }

    printf("Send.\n");

    if(bytes_received < 0){
        error("Error during communication.");
    }

    // close(revSockfd);
    close(newSockfd);
    // pthread_exit(NULL);
}

//====================================================================================
void printConfig(const struct Config *config)
{                                               // Done
    printf("Strategy: %s\n", config->strategy); // Done
    printf("Port: %d\n", config->port);         // Done

    printf("Servers:\n"); // Done
    for (size_t i = 0; i < config->num_servers; i++)
    { // Done
        printf("  Domain: %s, Port: %d\n", config->servers[i].ip, config->servers[i].port);
    }

    printf("Security:\n");                                         // Done
    printf("  Criteria: %s\n", config->security.criteria);         // Done
    printf("  Max Per User: %d\n", config->security.max_per_user); // Done
    printf("  Block: %s\n", config->security.block);               // Done
    printf("  Hold: %s\n", config->security.hold);                 // Done
}
//====================================================================================
//====================================================================================
int main(int argc, char *argv[])
{

    if (argc < 2)
    {                                            // Done
        error("Error, no port number given.\n"); // Done
    }

    // Firstly read the eg.json file.                           // Done
    char *configurationString = readConf(); // Done

    // Parse the configuartion string.                          // Done
    cJSON *json = cJSON_Parse(configurationString); // Done
    if (json == NULL)
    {                                                      // Done
        error("Error in parsing the configuration file."); // Done
    }

    // Memory free to prevent memory leak.
    free(configurationString);    // Done
    struct Config config;         // Done
    parseJSONdata(json, &config); // Done
    // printConfig(&config);         // Done

    struct Server server = {"127.0.0.1", atoi(argv[1])};                        // Done
    printf("Server listening over %s on port %d.\n\n", server.ip, server.port); // Done
    printf("Starting server...\n\n");                                           // Done

    // Sockfd to store file descriptor.                         // Done
    int sockfd, portno, n;                    // Done
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // Done
    if (sockfd < 0)
    {                                     // Done
        error("Error opening socket.\n"); // Done
    }

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);      // Done                             // Done
    memset(&serv_addr, 0, sizeof(serv_addr)); // Done

    serv_addr.sin_family = AF_INET;                   // Done
    serv_addr.sin_addr.s_addr = INADDR_ANY;//inet_addr(server.ip); // INADDR_ANY
    serv_addr.sin_port = htons(server.port);          // Done

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {                             // Done
        error("Binding failed."); // Done
    }

    listen(sockfd, 5); // Done

    while (1)
    { // Done
        printf("Waiting for a socket connection....\n");
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen); // Done
        printf("New socket accepted.\n");
        if (newsockfd < 0)
        {                                                   // Done
            error("Error in accepting the connections.\n"); // Done
        }

        // pthread_t pid; // Done
        handleConnection((void*)&newsockfd);
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