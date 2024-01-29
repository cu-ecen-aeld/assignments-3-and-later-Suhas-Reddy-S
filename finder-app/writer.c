/**
 * @file    writer.c
 * @brief   This file implements a writer service that writes a string to a txt file. It takes two arguements, argument 
 *          count and an array of strings. The array of strings must contain three parameters: command, filepath and 
 *          the String to add.
 * 
 * @author  Suhas-Reddy-S
 **/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        // There must be three parameters including the object alias or else throws an error.
        syslog(LOG_ERR, "Usage: %s <file_path> <string_to_add>", argv[0]);
        closelog();
        exit(1);
    }
    
    // Assign file path and string to be inserted to local variables.
    char *writefile = argv[1];
    char *writestr = argv[2];

    // Opens logger under writer.
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    // Opens or creates a file with RW permission for owner and R Only permission for others
    int fd = creat(writefile, 0644); 
    if (fd == -1) {
        // Throws an error if creation fails
        syslog(LOG_ERR, "Error opening file '%s': %m", writefile);
        closelog();
        exit(1);
    }

    // Stores result after write operation
    int write_status = write(fd, writestr, strlen(writestr));
    if (write_status == -1) {
        // Throws an error if write operation fails
        syslog(LOG_ERR, "Error writing to file '%s': %m", writefile);
        close(fd); // Explicitly closes the file in case of error
        closelog();
        exit(1);
    }
    
    // Closes file and throws an error if it fails
    if (close(fd) == -1) {
        syslog(LOG_ERR, "Error closing file '%s': %m", writefile);
        closelog();
        exit(1);
    }

    // Logs the end of writing and closes the log
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    closelog();
    
    return 0;
}
