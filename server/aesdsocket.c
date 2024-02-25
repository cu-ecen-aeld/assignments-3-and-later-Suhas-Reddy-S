#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <sys/stat.h> 

#define MAX_PACKET_SIZE 1024*100

int sockfd; // Global variable to access socket descriptor

void handle_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        // Log the signal
        syslog(LOG_INFO, "Caught signal, exiting");
        
        // Close socket
        close(sockfd);

        // Delete file
        remove("/var/tmp/aesdsocketdata");

        // Terminate the program
        exit(0);
    }
}

void send_file_contents(int sockfd) {
    FILE *file = fopen("/var/tmp/aesdsocketdata", "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char buffer[MAX_PACKET_SIZE];
    ssize_t nread;
    while ((nread = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(sockfd, buffer, nread, 0) < 0) {
            perror("Error sending file contents");
            break;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    int opt;
    int daemon_mode = 0; // Flag to indicate daemon mode

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (daemon_mode) {
        // Run as daemon
        pid_t pid = fork();

        if (pid < 0) {
            perror("Error forking");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            // Parent process exits
            exit(EXIT_SUCCESS);
        }

        // Child process continues
        umask(0);

        // Create a new session
        if (setsid() < 0) {
            perror("Error creating session");
            exit(EXIT_FAILURE);
        }

        // Change the current working directory to root
        if (chdir("/") < 0) {
            perror("Error changing directory");
            exit(EXIT_FAILURE);
        }

        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    struct sigaction action;
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // Register signal handlers
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    int newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    char buffer[MAX_PACKET_SIZE];
    int buffer_index = 0;

    // Step b: Opens a stream socket bound to port 9000
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        return -1;
    }

    // Initialize socket structure
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(9000);

    // Bind the host address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error on binding");
        return -1;
    }

    // Step c: Listen for incoming connections
    listen(sockfd, 5);

    while (1) {
        clilen = sizeof(cli_addr);

        // Accept connection from client
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Error on accept");
            return -1;
        }

        // Log accepted connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Receive data over the connection and append to file
        ssize_t n;
        while ((n = read(newsockfd, buffer + buffer_index, 1)) > 0) {
            // If newline received, append packet to file, reset buffer, and send file contents
            if (buffer[buffer_index] == '\n') {
                buffer[buffer_index] = '\0'; // Null terminate the string
                fprintf(stdout, "Received data: %s\n", buffer);
                FILE *file = fopen("/var/tmp/aesdsocketdata", "a");
                if (file == NULL) {
                    perror("Error opening file");
                    return -1;
                }
                fprintf(file, "%s\n", buffer);
                fclose(file);
                buffer_index = 0;
                // Send file contents to the client
                send_file_contents(newsockfd);
            } else {
                buffer_index++;
                // Discard packet if it exceeds maximum size
                if (buffer_index >= MAX_PACKET_SIZE) {
                    syslog(LOG_WARNING, "Received packet exceeds maximum size, discarding.");
                    buffer_index = 0;
                }
            }
        }

        if (n < 0) {
            perror("Error reading from socket");
        }

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);

        // Close connections
        close(newsockfd);
    }

    close(sockfd);

    return 0;
}

