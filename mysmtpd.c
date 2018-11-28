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

int fsmState = 0;

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

        if (strncasecmp(readBuffer, "helo", 4) == 0) {
            if (fsmState == 0 || fsmState == 1) {
                send_string(fd, "250 OK\r\n");
                fsmState = 1;
            } else {
                // TODO do we need a switch for the error statement? Need a status code
                send_string(fd, "Invalid input, please specify MAIL/RCPT/DATA\r\n");
            }

        } // if helo
        else if (strncmp(readBuffer, "mail", 4) == 0) {
            if (fsmState == 1) {
                printf("This is mail\n");
                char emailAddress[100];

                readEmailAddressFromBuffer(emailAddress, readBuffer);

                printf("%s\n", emailAddress);
                send_string(fd, "250 OK\r\n");
                fsmState = 2;
            } else {
                // TODO status code
                send_string(fd, "Invalid input\r\n");
            }
        } // if mail
        else if (strncmp(readBuffer, "rcpt", 4) == 0) {
            if (fsmState == 2 || fsmState == 3) {
                char emailAddress[100];

                readEmailAddressFromBuffer(emailAddress, readBuffer);
                if (is_valid_user(emailAddress, NULL)) {
                    add_user_to_list(&recipients, emailAddress);
                    printf("recipient is %s\n", emailAddress);
                } else {
                    send_string(fd, "Recipient not in users.txt\r\n");
                };
                // TODO need to 250 OK ??
                fsmState = 3;
            } else {
                send_string(fd, "Invalid input\r\n");
            }

        } // if rcpt
        else if (strncmp(readBuffer, "data", 4) == 0) {
            if (fsmState == 3) {
                send_string(fd, "354 Send message content; end with <CRLF>.<CRLF>\r\n");

                char* data = readData(fd);
                printf("%s", data);

                // saves temp file
                // https://stackoverflow.com/questions/1022487/how-to-create-a-temporary-text-file-in-c
                // TODO include headers
                char fileName[] = "../fileXXXXXX";
                int fileDescriptor = mkstemp(fileName);
                write(fileDescriptor, data, strlen(data));
                save_user_mail(fileName, recipients);
                close(fileDescriptor);
                unlink(fileName);
                fsmState = 1;
            } else {
                send_string(fd, "Invalid input\r\n");
            }

        } // if data
        else if (strncmp(readBuffer, "noop", 4) == 0) {
            printf("This is noop\n");
            send_string(fd, "250 OK\r\n");
        } // if noop
        else if (strncmp(readBuffer, "quit", 4) == 0) {
            send_string(fd, "Goodbye.\r\n");
            break;
        } // if quit
        else if (strncmp(readBuffer, "ehlo", 4) == 0 ||
                 strncmp(readBuffer, "rset", 4) == 0 ||
                 strncmp(readBuffer, "vrfy", 4) == 0 ||
                 strncmp(readBuffer, "expn", 4) == 0 ||
                 strncmp(readBuffer, "help", 4) == 0) {
            send_string(fd, "502 Unsupported command\r\n");
        } // if unsupported command
        else {
            // TODO handle closing client connection
            send_string(fd, "500 Invalid command\r\n");
            printf("Command not recognized\n");
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
        printf("Could not find email source end '>'\n");
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
                send_string(fd, "250 OK\r\n");
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