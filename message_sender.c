//
// Created by galam on 01/12/2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "message_slot.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        perror("Wrong input, the arguments number should be 3\n");
        exit(ERROR);
    }

    int fd = open(argv[1], O_RDWR); // O_WDONLY
    if (fd < 0) {
        perror("Error to open the file given\n");
        exit(ERROR);
    }

    int exit_code = ioctl(fd, MSG_SLOT_CHANNEL, atoi(argv[2]));
    if (exit_code != SUCCESS) {
        perror("Error in ioctl\n");
        close(fd);
        exit(ERROR);
    }

    exit_code = write(fd, argv[3], strlen(argv[3]));
    if (exit_code != strlen(argv[3])) {
        perror("Error in writing the message\n");
        close(fd);
        exit(ERROR);
    }

    close(fd);
    return SUCCESS;
}