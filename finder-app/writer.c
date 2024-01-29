#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        syslog(LOG_ERR, "Usage: %s <file_path> <string_to_add>", argv[0]);
        closelog();
        exit(1);
    }

    char *writefile = argv[1];
    char *writestr = argv[2];

    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    int fd = creat(writefile, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Error opening file '%s': %m", writefile);
        closelog();
        exit(1);
    }

    int write_status = write(fd, writestr, strlen(writestr));
    if (write_status == -1) {
        syslog(LOG_ERR, "Error writing to file '%s': %m", writefile);
        close(fd);
        closelog();
        exit(1);
    }

    if (close(fd) == -1) {
        syslog(LOG_ERR, "Error closing file '%s': %m", writefile);
        closelog();
        exit(1);
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    closelog();
    return 0;
}
