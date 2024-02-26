#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_FILE_LOC "/var/tmp/aesdsocketdata"
#define PORT "9000"
#define MAX_BUFFER_SIZE 256

int sockfd = -1;

// Function to handle cleanup and exit
void cleanupAndExit(int status) {
    syslog(LOG_INFO, "Closing aesdsocket application");
    closelog();
    close(sockfd);
    remove(LOG_FILE_LOC);
    exit(status);
}

// Signal handler function
void signalHandler(int signo) {
    syslog(LOG_USER, "Caught signal, exiting");
    cleanupAndExit(EXIT_SUCCESS);
}

// Setup signal handler
void setupSignalHandler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = signalHandler;

    if (sigaction(SIGTERM, &sa, NULL) != 0 || sigaction(SIGINT, &sa, NULL) != 0) {
        syslog(LOG_ERR, "Error setting up signal handler");
        cleanupAndExit(EXIT_FAILURE);
    }
}

// Function to handle client connection
void handleClientConnection(int client_fd) {
    FILE *fp = fopen(LOG_FILE_LOC, "a+");
    char buffer[MAX_BUFFER_SIZE];

    while (1) {
        ssize_t bytesRecv = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytesRecv <= 0) {
            break;
        }

        fwrite(buffer, bytesRecv, 1, fp);

        if (memchr(buffer, '\n', bytesRecv) != NULL) {
            break;
        }
    }

    fclose(fp);

    fp = fopen(LOG_FILE_LOC, "r");
    while (!feof(fp)) {
        ssize_t bytesRead = fread(buffer, 1, sizeof(buffer), fp);
        if (bytesRead <= 0) {
            break;
        }

        send(client_fd, buffer, bytesRead, 0);
    }

    fclose(fp);
    close(client_fd);
}

// Function to daemonize the process
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Error forking: %m");
        cleanupAndExit(EXIT_FAILURE);
    } else if (pid > 0) {
        exit(EXIT_SUCCESS); // Parent exits
    }

    if (setsid() == -1) {
        syslog(LOG_ERR, "Error creating new session: %m");
        cleanupAndExit(EXIT_FAILURE);
    }

    chdir("/"); // Change working directory to root

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect stdin/out/err to /dev/null
    int devNull = open("/dev/null", O_RDWR);
    if (devNull == -1) {
        syslog(LOG_ERR, "Error opening /dev/null: %m");
        cleanupAndExit(EXIT_FAILURE);
    }

    dup2(devNull, STDIN_FILENO);
    dup2(devNull, STDOUT_FILENO);
    dup2(devNull, STDERR_FILENO);

    close(devNull);
}

int main(int argc, char* argv[]) {
    openlog("AESD Socket", LOG_PID | LOG_NDELAY, LOG_USER);

    bool runAsDaemon = false;

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        runAsDaemon = true;
    }

    setupSignalHandler();

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(PORT));
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Error creating socket: %m");
        cleanupAndExit(EXIT_FAILURE);
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        syslog(LOG_ERR, "Error setting socket options: %m");
        cleanupAndExit(EXIT_FAILURE);
    }

    if (runAsDaemon) {
        daemonize();
    }

    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        syslog(LOG_ERR, "Error binding: %m");
        cleanupAndExit(EXIT_FAILURE);
    }

    if (listen(sockfd, 5) != 0) {
        syslog(LOG_ERR, "Error listening: %m");
        cleanupAndExit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Listening on port %s", PORT);

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        int client_fd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize);

        if (client_fd == -1) {
            syslog(LOG_ERR, "Issue accepting connection: %m");
            continue;
        }

        char ipAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipAddr, sizeof(ipAddr));
        syslog(LOG_USER, "Accepted connection from %s", ipAddr);

        handleClientConnection(client_fd);

        syslog(LOG_USER, "Closed connection from %s", ipAddr);
    }

    cleanupAndExit(EXIT_SUCCESS);
}

