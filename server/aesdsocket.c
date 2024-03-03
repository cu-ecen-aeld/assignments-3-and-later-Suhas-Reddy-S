/*
 * File: aesdsocket.c
 * Author: Suhas Reddy
 * Brief: This program demonstrates a simple TCP server that listens on a custom port,
 *        accepts connections, and logs incoming data from clients to a file.
 *        It can be run in daemon mode using the "-d" command line argument.
 * Reference: https://beej.us/guide/bgnet/html/#what-is-a-socket
 * Date: 25th Feb 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>
#include <time.h>

// Macros for 
#define CUSTOM_PORT "9000"
#define MAX_CUSTOM_BUFFER 1024
#define CUSTOM_LOG_FILE "/var/tmp/mysocketlog"

/* Function prototypes */
void cleanup_resources();
void signal_handler(int signo);
void setup_logging();
int create_socket();
void set_socket_options(int sockfd);
void bind_socket(int sockfd);
void listen_for_connections(int sockfd);
void *handle_client(void *arg);
void accept_clients(int sockfd);
void daemonize();
void add_thread(pthread_t tid);
void remove_thread(pthread_t tid);
void join_completed_threads();
void *append_timestamp(void *arg);

typedef struct ThreadNode {
    pthread_t tid;
    struct ThreadNode *next;
} ThreadNode;

ThreadNode *thread_list = NULL; 

volatile bool sig_exit = false;
int custom_socket_fd = -1;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Cleans up resources on exit
void cleanup_resources() {
    syslog(LOG_INFO, "Cleaning up resources");
    closelog();
    if (custom_socket_fd != -1) {
        close(custom_socket_fd);
        custom_socket_fd = -1;
    }
    if (remove(CUSTOM_LOG_FILE) != 0) {
        syslog(LOG_ERR, "Error removing log file: %m");
    }
}

// Signal handler for SIGINT and SIGTERM
void signal_handler(int signo) {
    syslog(LOG_USER, "Signal %d caught, exiting", signo);
    cleanup_resources();
    sig_exit = true;
}

// Sets up logging using syslog
void setup_logging() {
    openlog("AESDSocket", LOG_PID | LOG_NDELAY, LOG_USER);
}

// Creates a socket and handles errors if any
int create_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Socket creation error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

// Sets socket options for reuse address and handles errors if any
void set_socket_options(int sockfd) {
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        syslog(LOG_ERR, "Socket options setting error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }
}

// Binds the socket to a port and handles errors if any
void bind_socket(int sockfd) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(CUSTOM_PORT));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        syslog(LOG_ERR, "Binding error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }
}

// Listens for incoming connections and handles errors if any
void listen_for_connections(int sockfd) {
    if (listen(sockfd, 5) != 0) {
        syslog(LOG_ERR, "Listening error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }
}

// Handles client connections
void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);
    
    add_thread(pthread_self());
    
    char buffer[MAX_CUSTOM_BUFFER];
    char packetBuffer[MAX_CUSTOM_BUFFER];  // Buffer to accumulate a complete packet
    size_t packetSize = 0;

    while (1) {
        ssize_t bytes_recv = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_recv <= 0) {
            break;
        }

        // Accumulate data until a complete packet is received
        memcpy(packetBuffer + packetSize, buffer, bytes_recv);
        packetSize += bytes_recv;

        // Check if a complete packet (ending with '\n') is received
        if (memchr(buffer, '\n', bytes_recv) != NULL) {
            // Write the complete packet to the log file using system call
            pthread_mutex_lock(&list_mutex);
            int fd = open(CUSTOM_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, 0777);
            if (fd == -1) {
                syslog(LOG_INFO, "Couldn't open file");
                pthread_exit(NULL);
            }
            write(fd, packetBuffer, packetSize);
            
            // Read from the log file and send back to the client
	    lseek(fd, 0, SEEK_SET);  // Set the file pointer to the beginning of the file

	    while (1) {
		ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
		if (bytes_read <= 0) {
		    break;
		}

		// Send the contents of the log file back to the client
		send(client_fd, buffer, bytes_read, 0);
	    }

            close(fd);
            pthread_mutex_unlock(&list_mutex);
            
            // Reset the packet buffer for the next packet
            memset(packetBuffer, 0, sizeof(packetBuffer));
            packetSize = 0;

            // Break after handling the complete packet
            break;
        }
    }

    close(client_fd);
    remove_thread(pthread_self());
    pthread_exit(NULL);
}

// Accepts incoming client connections
void accept_clients(int sockfd) {
    while (!sig_exit) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);

        if (*client_fd == -1) {
            syslog(LOG_ERR, "Connection acceptance issue: %m");
            free(client_fd);
            continue;
        }

        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_addr, sizeof(ip_addr));
        syslog(LOG_USER, "Connection accepted from %s", ip_addr);

        // Create a new thread to handle the client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)client_fd) != 0) {
            syslog(LOG_ERR, "Thread creation error");
            free(client_fd);
            continue;
        }

        // Detach the thread to allow automatic cleanup when it exits
        pthread_detach(tid);

        syslog(LOG_USER, "Connection closed from %s", ip_addr);
    }
}

// Daemonizes the process
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Fork error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        exit(EXIT_SUCCESS); // Parent process exits
    }

    if (setsid() == -1) {
        syslog(LOG_ERR, "New session creation error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    chdir("/"); // Change current working directory to root

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1) {
        syslog(LOG_ERR, "/dev/null opening error: %m");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);

    close(dev_null);
}

// Add a thread to the linked list of active threads
void add_thread(pthread_t tid) {
    ThreadNode *new_node = malloc(sizeof(ThreadNode));
    if (new_node == NULL) {
        syslog(LOG_ERR, "Memory allocation error for thread node");
        exit(EXIT_FAILURE);
    }

    new_node->tid = tid;
    new_node->next = NULL;

    pthread_mutex_lock(&list_mutex);

    // Add the new node to the front of the list
    new_node->next = thread_list;
    thread_list = new_node;

    pthread_mutex_unlock(&list_mutex);
}

void remove_thread(pthread_t tid) {
    pthread_mutex_lock(&list_mutex);

    ThreadNode *current = thread_list;
    ThreadNode *prev = NULL;

    // Find the node to remove
    while (current != NULL && current->tid != tid) {
        prev = current;
        current = current->next;
    }

    // If found, remove the node from the list
    if (current != NULL) {
        if (prev == NULL) {
            // If the node is at the beginning of the list
            thread_list = current->next;
        } else {
            // If the node is in the middle or end of the list
            prev->next = current->next;
        }

        free(current);
    }

    pthread_mutex_unlock(&list_mutex);
}

// Join completed threads using pthread_join()
void join_completed_threads() {
    pthread_mutex_lock(&list_mutex);

    ThreadNode *current = thread_list;
    ThreadNode *next_node = NULL;

    while (current != NULL) {
        next_node = current->next;

        // Join the thread and remove it from the list
        pthread_join(current->tid, NULL);
        remove_thread(current->tid);

        current = next_node;
    }

    pthread_mutex_unlock(&list_mutex);
}

// Appends timestamp to the log file every 10 seconds
void *append_timestamp(void *arg) {
    time_t raw_time;
    struct tm *time_info;
    char timestamp_str[128];

    while (!sig_exit) {
        time(&raw_time);
        time_info = localtime(&raw_time);

        // Format the timestamp string according to RFC 2822
        strftime(timestamp_str, sizeof(timestamp_str), "timestamp:%a, %d %b %Y %H:%M:%S %z", time_info);

        // Append the timestamp to the log file with mutex protection
        pthread_mutex_lock(&file_mutex);
        int fd = open(CUSTOM_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, 0777);
        if (fd != -1) {
            write(fd, timestamp_str, strlen(timestamp_str));
            write(fd, "\n", 1);
            close(fd);
        } else {
            syslog(LOG_ERR, "Couldn't open file for timestamp: %m");
        }
        pthread_mutex_unlock(&file_mutex);

        sleep(10);  // Sleep for 10 seconds
    }

    pthread_exit(NULL);
}

// Main function
int main(int argc, char *argv[]) {
    setup_logging();
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    bool daemon = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon = true;
    }

    custom_socket_fd = create_socket();
    set_socket_options(custom_socket_fd);
    bind_socket(custom_socket_fd);

    if (daemon) {
        daemonize();
    }
    
    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, append_timestamp, NULL) != 0) {
        syslog(LOG_ERR, "Timestamp thread creation error");
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    while (!sig_exit) {
        listen_for_connections(custom_socket_fd);
        accept_clients(custom_socket_fd);
    }

    // Allow the timestamp thread to write the final timestamp
    sleep(30);  // Adjust the sleep duration as needed

    // Join completed threads after the main loop
    join_completed_threads();

    // Cleanup resources
    cleanup_resources();

    return EXIT_SUCCESS;
}

