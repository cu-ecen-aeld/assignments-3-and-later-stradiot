#include <pthread.h> 
#include <errno.h>
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
#include <sys/queue.h>
#include <stdbool.h>

#define TMPFILE "/var/tmp/aesdsocketdata"

bool signal_caught = false;
pthread_mutex_t lock;

struct thread_data{
    pthread_mutex_t* mutex;
	int sockfd_in; 
	struct sockaddr_in addr_client;

	bool thread_complete;
    bool thread_complete_success;
};

struct conn_thread{
	pthread_t thread;
	struct thread_data thread_data;

	SLIST_ENTRY(conn_thread) entries;
};

static void signal_handler (int signal_number){
    if (signal_number == SIGINT || signal_number == SIGTERM){
        syslog(LOG_INFO, "Caught signal, exiting");
		signal_caught = true;
    }
}

void* handle_conn(void* conn_data){
	int rc;

    struct thread_data* thread_args = (struct thread_data *) conn_data;

	FILE* file = fopen(TMPFILE, "a");
	if (file == NULL){
		syslog(LOG_ERR, "Error opening file for append: %s\n", strerror(errno));
		thread_args->thread_complete = true;
		thread_args->thread_complete_success = false;
		return conn_data;
	}

	char buffer[1024] = { 0 };
	int bytes_received;
	while ((bytes_received = recv(thread_args->sockfd_in, buffer, sizeof(buffer) - 1, 0)) > 0){
		if (signal_caught){
			thread_args->thread_complete = true;
			return conn_data;
		}
		if (bytes_received < 0){
			syslog(LOG_ERR, "Error receiving data: %s\n", strerror(errno));
			thread_args->thread_complete = true;
			thread_args->thread_complete_success = false;
			return conn_data;
		}
		rc = pthread_mutex_lock(thread_args->mutex); 
		if (rc != 0){
			syslog(LOG_ERR, "Mutex lock failed to lock with %d", rc);
			thread_args->thread_complete = true;
			thread_args->thread_complete_success = false;
			return conn_data;
		}
		if (fwrite(buffer, sizeof(char), bytes_received, file) < 0){
			syslog(LOG_ERR, "Error writing to file: %s\n", strerror(errno));
			thread_args->thread_complete = true;
			thread_args->thread_complete_success = false;
			return conn_data;
		}
		rc = pthread_mutex_unlock(thread_args->mutex);
		if (rc != 0){
			syslog(LOG_ERR, "Mutex unlock failed to lock with %d", rc);
			thread_args->thread_complete = true;
			thread_args->thread_complete_success = false;
			return conn_data;
		}
		if(strchr(buffer ,'\n') != NULL){
			break;
		}
	}

	if (fclose(file) == EOF){
		syslog(LOG_ERR, "Error closing the file: %s\n", strerror(errno));
		thread_args->thread_complete = true;
		thread_args->thread_complete_success = false;
		return conn_data;
	}

	file = fopen(TMPFILE, "r");
	if (file == NULL){
		syslog(LOG_ERR, "Error opening the file for read: %s\n", strerror(errno));
		thread_args->thread_complete = true;
		thread_args->thread_complete_success = false;
		return conn_data;
	}
	char buff[1024] = { 0 };
	int bytes_read;
	while((bytes_read = fread(buff, sizeof(char), sizeof(buff), file)) > 0){
		if (send(thread_args->sockfd_in, buff, bytes_read, 0) < 0){
			syslog(LOG_ERR, "Error sending data: %s\n", strerror(errno));
			thread_args->thread_complete = true;
			thread_args->thread_complete_success = false;
			return conn_data;
		}
	}
	if (fclose(file) == EOF){
		syslog(LOG_ERR, "Error closing the file: %s\n", strerror(errno));
		thread_args->thread_complete = true;
		thread_args->thread_complete_success = false;
		return conn_data;
	}

	close(thread_args->sockfd_in);
	syslog(LOG_INFO, "Closed connection from %s\n", inet_ntoa(thread_args->addr_client.sin_addr));

	thread_args->thread_complete = true;
	thread_args->thread_complete_success = true;

    return conn_data;
}

int main(int argc, char const* argv[]){
    int sockfd, status, opt = 1;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    bool rundaemon = false;

	SLIST_HEAD(list_head, conn_thread) threads_head;
	SLIST_INIT(&threads_head);

    if (pthread_mutex_init(&lock, NULL) < 0) { 
		perror("mutex init");
		exit(EXIT_FAILURE);
    } 

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

    if (listen(sockfd, 5) < 0){
        perror("listen");
		exit(EXIT_FAILURE);
    }

    while (!signal_caught){
        int sockfd_in; 
        struct sockaddr_in addr_client;
        socklen_t sockaddr_client_len = sizeof(addr_client);

        if ((sockfd_in = accept(sockfd, (struct sockaddr*) &addr_client, &sockaddr_client_len)) < 0){
            perror("accept");
		    break;
        }

		struct conn_thread* finished_threads[64] = { NULL };
		struct conn_thread* tmp_thread;
		int thread_index = 0;
		SLIST_FOREACH(tmp_thread, &threads_head, entries){
			if(tmp_thread->thread_data.thread_complete){
				finished_threads[thread_index++] = tmp_thread;
			}
		}
		for (int i = 0; i < thread_index; i++){
			pthread_join(finished_threads[i]->thread, NULL);
			SLIST_REMOVE(&threads_head, finished_threads[i], conn_thread, entries);
			free(finished_threads[i]);
		}

        syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntoa(addr_client.sin_addr));

		struct thread_data data = {
			.mutex = &lock,
			.sockfd_in = sockfd_in,
			.addr_client = addr_client,
			.thread_complete = false,
			.thread_complete_success = true
		};

		struct conn_thread* new_thread = malloc(sizeof(struct conn_thread));
		new_thread->thread_data = data;
		int rc = pthread_create(&new_thread->thread, NULL, handle_conn, &new_thread->thread_data);
		if (rc != 0){
			syslog(LOG_ERR, "Failed to create a thread %d", rc);
			new_thread->thread_data.thread_complete_success = false;
			break;
		}
		SLIST_INSERT_HEAD(&threads_head, new_thread, entries);
    }

	while (!SLIST_EMPTY(&threads_head)){
		struct conn_thread* tmp = SLIST_FIRST(&threads_head);
		pthread_join(tmp->thread, NULL);
		SLIST_REMOVE_HEAD(&threads_head, entries);
		free(tmp);
	}
    pthread_mutex_destroy(&lock);
    close(sockfd);
    freeaddrinfo(servinfo);
    closelog();

    remove(TMPFILE);

    return 0;
}
