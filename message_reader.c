//
// Created by galam on 01/12/2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "message_slot.h"

int main (int argc, char* argv[] ){
    if (argc < 3) {
        perror("Wrong input, the arguments number should be 2\n");
        exit(ERROR);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("ERROR to open the file given\n");
        exit(ERROR);
    }

    int exit_code = ioctl(fd,MSG_SLOT_CHANNEL, atoi(argv[2]));
    if (exit_code != SUCCESS) {
        perror("Error in ioctl\n");
        close(fd);
        exit(ERROR);
    }

    char * buffer = malloc(sizeof(char) * MAX_MESSAGE_LENGTH);
    if (buffer == NULL) {
        perror("Error allocating buffer\n");
        close(fd);
        exit(ERROR);
    }

    exit_code = read(fd, buffer, MAX_MESSAGE_LENGTH);
    if (exit_code < 0) {
        perror("Error reading message\n");
        close(fd);
        exit(ERROR);
    }
    close(fd);
    if (write(STDOUT_FILENO, buffer, exit_code) == -1){
            perror("Error printing message\n");
    }

    return SUCCESS;
}