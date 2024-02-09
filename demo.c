#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

void error(const char *msg){
    perror(msg);
    exit(0);
}

int main(int argc, char* argv[]){
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0){
        error("Error opening socket.\n");
    }

    server = gethostbyname(argv[1]);

    if(server == NULL){
        error("Error, no such host.\n");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(portno);

    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 ){
        error("Error connecting.");
    }

    while(1){
        memset(&buffer, 0, sizeof(buffer)-1);
        fgets(buffer, sizeof(buffer)-1, stdin);
        int n = write(sockfd, buffer, strlen(buffer));
        if(n<0){
            error("Error writing to socket.");
        }
        memset(&buffer, 0, sizeof(buffer)-1);
        n = read(sockfd, buffer, sizeof(buffer)-1);
        if(n<0){
            error("Error reading from socket.");
        }
        int i = strncmp("Bye", buffer, 3);
        if(i==0){
            break;
        }
    }
    close(sockfd);

    return 0;
}