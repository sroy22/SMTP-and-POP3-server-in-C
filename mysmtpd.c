#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);
static void readEmailAddressFromBuffer(char* emailAddress, char* readBuffer);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {
    user_list_t recipients = create_user_list();

    while (true) {
        printf("Hello world\n");
        char readBuffer[1000] = ""; // empty buffer
        // TODO read returns number of bytes read
        read(fd, readBuffer, 1000);

        printf("%s\n", readBuffer);
        printf("123\n");

        char bufOut[100]; // buffer to send client
        if (strncmp(readBuffer, "helo", 4) == 0) {
            strcpy(bufOut, "250 OK\r\n");
            send(fd, bufOut, strlen(bufOut), 0); // send message
        } // if helo
        else if (strncmp(readBuffer, "mail", 4) == 0) {
            printf("This is mail\n");
            char emailAddress[100];

            readEmailAddressFromBuffer(emailAddress, readBuffer);

            printf("%s\n", emailAddress);
            strcpy(bufOut, "250 OK\r\n");
            send(fd, bufOut, strlen(bufOut), 0); //sending client message

        } // if mail
        else if (strncmp(readBuffer, "rcpt", 4) == 0) {
            char emailAddress[100];

            readEmailAddressFromBuffer(emailAddress, readBuffer);

            add_user_to_list(&recipients, emailAddress);
            printf("recipient is %s\n", emailAddress);
        } // if rcpt
        else if (strncmp(readBuffer, "data", 4) == 0) {
            char *data;
            data = (char *) malloc(strlen(readBuffer));
            char *temp;
            temp = (char *) malloc(strlen(readBuffer));

            // read until '.'
            while (!(strlen(readBuffer) == 1 && readBuffer[0] == '.')) {
                strcpy(temp, data);
                strcat(temp, readBuffer);
                data = (char *) realloc(data, 2 * strlen(temp));
                strcpy(data, temp);
            }
            data[strlen(data) - 1] = '\0';
        } // if data
        else if (strncmp(readBuffer, "noop", 4) == 0) {
            printf("This is noop\n");
            strcpy(bufOut, "250 OK\r\n");
            send(fd, bufOut, strlen(bufOut), 0); //sending client message
        } // if noop
        else if (strncmp(readBuffer, "quit", 4) == 0) {
            break;
        } // if quit
        else {
            printf("Command not recognized");
        }
    }
}

void readEmailAddressFromBuffer(char* emailAddress, char* readBuffer) {
    char *emailSource;
    emailSource = strchr(readBuffer, '<');
    if (emailSource == NULL) {
        // TODO Handle these
        printf("Could not find email source");
    }
    emailSource = emailSource + 1; // get string after <
    char *emailSourceTail = strchr(emailSource, '>');
    if (emailSourceTail == NULL) {
        // TODO handle this
        printf("Could not find email source end '>'");
    }
    int emailSourceLength = (int) emailSourceTail - (int) emailSource;
    memcpy(emailAddress, emailSource, (size_t) emailSourceLength);
    emailAddress[emailSourceLength] = 0;
}