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
        syslog(LOG_INFO, "Caught signal, exiting");
	signal_caught = true;
    }
}

int main(int argc, char const* argv[])
{
    int sockfd, status, opt = 1;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    bool rundaemon = false;

    struct sigaction new_action = {.sa_handler = signal_handler};

    if (argc > 1 && strcmp(argv[1], "-d") == 0){
        rundaemon = true;
    }

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

    if (rundaemon){
        daemon(0, 0);
    }

    if (listen(sockfd, 1) < 0){
        perror("listen");
	exit(EXIT_FAILURE);
    }

    char* line = calloc(1, 1);
    while (!signal_caught){
        int sockfd_in; 
        struct sockaddr_in addr_client;
        socklen_t sockaddr_client_len = sizeof(addr_client);

        if ((sockfd_in = accept(sockfd, (struct sockaddr*) &addr_client, &sockaddr_client_len)) < 0){
            perror("accept");
	    break;
        }

        syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntoa(addr_client.sin_addr));

        FILE* file = fopen(TMPFILE, "a");
        if (file == NULL){
            perror("fileopen");
            exit(EXIT_FAILURE);
        }

        char buffer[1024] = { 0 };
	int bytes_received;
        while ((bytes_received = recv(sockfd_in, buffer, sizeof(buffer) - 1, 0)) > 0) {
            if (fwrite(buffer, sizeof(char), bytes_received, file) < 0){
	        perror("write");
		break;
	    }
	    if(strchr(buffer ,'\n') != NULL){
	        break;
	    }
	}

        if (fclose(file) == EOF){
            perror("fileclose");
            exit(EXIT_FAILURE);
        }

        file = fopen(TMPFILE, "r");
        if (file == NULL){
            perror("fileopen");
            exit(EXIT_FAILURE);
        }
        char buff[1024] = { 0 };
	int bytes_read;
        while((bytes_read = fread(buff, sizeof(char), sizeof(buff), file)) > 0){
            send(sockfd_in, buff, bytes_read, 0);
        }
        if (fclose(file) == EOF){
            perror("fileclose");
            exit(EXIT_FAILURE);
        }

	line[0] = '\0';

        close(sockfd_in);
        syslog(LOG_INFO, "Closed connection from %s\n", inet_ntoa(addr_client.sin_addr));
    }

    free(line);
    close(sockfd);
    freeaddrinfo(servinfo);
    closelog();

    remove(TMPFILE);

    return 0;
}
