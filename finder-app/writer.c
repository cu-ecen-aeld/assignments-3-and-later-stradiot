#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>


int main (int argc, char * argv[]){
    openlog("writer", LOG_PERROR, LOG_USER);

    if (argc < 3) {
	syslog(LOG_ERR, "Invalid number of arguments %d", argc - 1);
        exit(1);
    }
    const char *writefile = argv[1];
    const char *writestr = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    FILE *file = fopen(writefile, "w");
    if (file == NULL){
	syslog(LOG_ERR, "The file could not be opened: %s", strerror(errno));
	exit(1);
    }
    fputs(writestr, file);
    
    if (fclose(file) == EOF){
	syslog(LOG_ERR, "The file could not be closed: %s", strerror(errno));
    }

    closelog();

    return 0;
}

