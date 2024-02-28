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


#define CUSTOM_PORT "9000"
#define MAX_CUSTOM_BUFFER 1024
#define CUSTOM_LOG_FILE "/var/tmp/mysocketlog"

int custom_socket_fd = -1;

/* Function prototypes */
void cleanup_resources();
void signal_handler(int signo);
void setup_logging();
int create_socket();
void set_socket_options(int sockfd);
void bind_socket(int sockfd);
void listen_for_connections(int sockfd);
void handle_client(int client_fd);
void accept_clients(int sockfd);
void daemonize();

// Cleans up resources on exit
void cleanup_resources() {
    syslog(LOG_INFO, "Cleaning up resources");
    closelog();
    if (custom_socket_fd != -1) {
        close(custom_socket_fd);
        custom_socket_fd = -1;
    }
    remove(CUSTOM_LOG_FILE);
}

// Signal handler for SIGINT and SIGTERM
void signal_handler(int signo) {
    syslog(LOG_USER, "Signal %d caught, exiting", signo);
    cleanup_resources();
    exit(EXIT_SUCCESS);
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
void handle_client(int client_fd) {
    FILE *fp = fopen(CUSTOM_LOG_FILE, "a+");
    char buffer[MAX_CUSTOM_BUFFER];

    while (1) {
        ssize_t bytes_recv = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_recv <= 0) {
            break;
        }

        fwrite(buffer, bytes_recv, 1, fp);

        if (memchr(buffer, '\n', bytes_recv) != NULL) {
            break;
        }
    }

    fclose(fp);

    fp = fopen(CUSTOM_LOG_FILE, "r");
    while (!feof(fp)) {
        ssize_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
        if (bytes_read <= 0) {
            break;
        }

        send(client_fd, buffer, bytes_read, 0);
    }

    fclose(fp);
    close(client_fd);
}

// Accepts incoming client connections
void accept_clients(int sockfd) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);

        if (client_fd == -1) {
            syslog(LOG_ERR, "Connection acceptance issue: %m");
            continue;
        }

        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_addr, sizeof(ip_addr));
        syslog(LOG_USER, "Connection accepted from %s", ip_addr);

        handle_client(client_fd);

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

    listen_for_connections(custom_socket_fd);
    accept_clients(custom_socket_fd);

    cleanup_resources();
    return EXIT_SUCCESS;
}

