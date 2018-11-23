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
static char* readData(int fd);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {
    // TODO restart this list when user tries to send new email
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
            strcpy(bufOut, "354 Send message content; end with <CRLF>.<CRLF>\r\n");
            send(fd, bufOut, strlen(bufOut), 0); //sending client message

            char* data = readData(fd);
            printf("%s", data);
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
            // TODO handle closing client connection
            printf("Command not recognized");
        }
    }
}

void readEmailAddressFromBuffer(char* emailAddress, char* readBuffer) {
    char *emailHead;
    emailHead = strchr(readBuffer, '<');
    if (emailHead == NULL) {
        // TODO Handle these
        printf("Could not find email source");
    }
    emailHead = emailHead + 1; // get string after <
    char *emailTail = strchr(emailHead, '>');
    if (emailTail == NULL) {
        // TODO handle this
        printf("Could not find email source end '>'");
    }
    int emailLength = (int) emailTail - (int) emailHead;
    memcpy(emailAddress, emailHead, (size_t) emailLength);
    emailAddress[emailLength] = 0; // end with 0/NULL
}

char* readData(int fd) {
    char readBuffer[1000] = ""; // empty buffer
    char* data = (char *) malloc(1000);

    // read until '.'
    while (true) {
        if (read(fd, readBuffer, 1000) > 0)
        {
            size_t bufferLength = strlen(readBuffer);

            // bufferLength 2 because null termination
            if (bufferLength == 2 && readBuffer[0] == '.') {
                break;
            }

            data = (char *) realloc(data, strlen(data) + bufferLength + 1);
            strcat(data, readBuffer);

            // clear readBuffer for next line
            memset(readBuffer, 0, sizeof readBuffer);
        }
    }
    return data;
}