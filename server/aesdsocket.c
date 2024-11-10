#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>

#define TMPFILE "/var/tmp/aesdsocketdata"

bool signal_caught = false;

static void signal_handler (int signal_number){
    if (signal_number == SIGINT || signal_number == SIGTERM){
	signal_caught = true;
    }
}

int main(int argc, char const* argv[])
{
    int sockfd, status, opt = 1;
    struct addrinfo hints;
    struct addrinfo* servinfo;

    struct sigaction new_action;
    memset(&new_action, 0, sizeof(sigaction));
    new_action.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &new_action, NULL) != 0){
        perror("sigterm");
	exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &new_action, NULL) != 0){
        perror("sigint");
        exit(EXIT_FAILURE);
    }

    openlog("assignment5", LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_USER);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0){
        perror("socket");
	exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 1) < 0){
        perror("listen");
	exit(EXIT_FAILURE);
    }

    while (true){
        int sockfd_in; 
        char buffer[1024] = { 0 };
        char* line = NULL;
        struct sockaddr_in addr_client;
        socklen_t sockaddr_client_len = sizeof(addr_client);

	if (signal_caught){
            syslog(LOG_INFO, "Caught signal, exiting");
	    break;
	}

        if ((sockfd_in = accept(sockfd, (struct sockaddr*) &addr_client, &sockaddr_client_len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }

        syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntoa(addr_client.sin_addr));

        while (strchr(buffer, '\n') == NULL) {
            memset(buffer, 0, sizeof(buffer));
            recv(sockfd_in, buffer, sizeof(buffer - 1), 0);

            if (line == NULL){
                line = calloc(strlen(buffer) + 1, 1);
                strcpy(line, buffer);
            } else {
                char tmp[strlen(line) + strlen(buffer) + 1];
                memset(tmp, 0, strlen(line) + strlen(buffer) + 1);
                strcat(tmp, line);
                strcat(tmp, buffer);

                line = realloc(line, strlen(line) + strlen(buffer) + 1);
                strcpy(line, tmp);
            } 
        }

        FILE* file = fopen(TMPFILE, "a");
        if (file == NULL){
            perror("fileopen");
            exit(EXIT_FAILURE);
        }
        fprintf(file, "%s", line);
        if (fclose(file) == EOF){
            perror("fileclose");
            exit(EXIT_FAILURE);
        }

        file = fopen(TMPFILE, "r");
        if (file == NULL){
            perror("fileopen");
            exit(EXIT_FAILURE);
        }
        char buff[5] = { 0 };
        while(fgets(buff, sizeof(buff), file)){
           send(sockfd_in, buff, strlen(buff), 0);
        }
        if (fclose(file) == EOF){
            perror("fileclose");
            exit(EXIT_FAILURE);
        }

        free(line);

        close(sockfd_in);
        syslog(LOG_INFO, "Closed connection from %s\n", inet_ntoa(addr_client.sin_addr));
    }

    close(sockfd);

    freeaddrinfo(servinfo);

    closelog();

    remove(TMPFILE);

    return 0;
}
