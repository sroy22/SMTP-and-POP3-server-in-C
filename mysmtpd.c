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

int fsmState = -1;
int queueCount = 0;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {
    user_list_t recipients;

    struct utsname unameData;
    uname(&unameData);
    send_string(fd, "220 %s SMTP Ready\r\n", unameData.__domainname);

    fsmState = 0;

    while (true) {
        char readBuffer[MAX_LINE_LENGTH] = ""; // empty buffer
        // TODO ask if 1000 is right? or should it be MAX_LINE_LENGTH
        ssize_t bytesRead = read(fd, readBuffer, 1000);
        size_t bufferLength = strlen(readBuffer);
        readBuffer[bufferLength - 1] = '\0';
        if(bufferLength <= MAX_LINE_LENGTH) {
            if ((strncasecmp(readBuffer, "helo", 4) == 0 && bufferLength == 5) ||
                 strncasecmp(readBuffer, "helo ", 5) == 0) {
                if (fsmState == 0 || fsmState == 1) {
                    send_string(fd, "250 %s\r\n", unameData.__domainname);
                    fsmState = 1;
                } else {
                    // TODO do we need a switch for the error statement? Need a status code
                    send_string(fd, "503 Bad sequence of commands\r\n");
                }

            } // if helo
            else if (strncasecmp(readBuffer, "mail from:", 10) == 0 && bufferLength > 10) {
                if (fsmState == 1) {
                    char emailAddress[100] = "";

                    readEmailAddressFromBuffer(emailAddress, readBuffer);
                    if (strlen(emailAddress) != 0) {
                        send_string(fd, "250 OK\r\n");
                        recipients = create_user_list();
                        fsmState = 2;
                    } else {
                        send_string(fd, "555 MAIL FROM parameters not recognized \r\n");
                    }
                } else {
                    send_string(fd, "503 Bad sequence of commands\r\n");
                }
            } // if mail
            else if (strncasecmp(readBuffer, "rcpt to:", 8) == 0 && bufferLength > 8) {
                if (fsmState == 2 || fsmState == 3) {
                    char emailAddress[100] = "";

                    readEmailAddressFromBuffer(emailAddress, readBuffer);
                    if (strlen(emailAddress) == 0) {
                        send_string(fd, "555 RCPT TO parameters not recognized \r\n");
                    } else if (is_valid_user(emailAddress, NULL)) {
                        add_user_to_list(&recipients, emailAddress);
                        send_string(fd, "250 OK\r\n");
                        fsmState = 3;
                    } else {
                        // TODO ask for error code for this one
                        send_string(fd, "553 Mailbox name not allowed; recipient not in users.txt\r\n");
                    };
                } else {
                    send_string(fd, "503 Bad sequence of commands\r\n");
                }

            } // if rcpt
            else if (strncasecmp(readBuffer, "data", 4) == 0) {
                if (fsmState == 3) {
                    send_string(fd, "354 Send message content; end with <CRLF>.<CRLF>\r\n");

                    char* data = readData(fd);

                    // saves temp file
                    // https://stackoverflow.com/questions/1022487/how-to-create-a-temporary-text-file-in-c
                    char fileName[] = "../fileXXXXXX";
                    int fileDescriptor = mkstemp(fileName);
                    write(fileDescriptor, data, strlen(data));
                    save_user_mail(fileName, recipients);
                    close(fileDescriptor);
                    unlink(fileName);

                    destroy_user_list(recipients);
                    fsmState = 1;

                    send_string(fd, "250 OK: queued as %d\r\n", ++queueCount);
                } else {
                    send_string(fd, "503 Bad sequence of commands\r\n");
                }

            } // if data
            else if (strncasecmp(readBuffer, "noop", 4) == 0) {
                send_string(fd, "250 OK\r\n");
            } // if noop
            else if (strncasecmp(readBuffer, "quit", 4) == 0) {
                send_string(fd, "221 Goodbye.\r\n");
                destroy_user_list(recipients);
                break;
            } // if quit
            else if (strncasecmp(readBuffer, "ehlo", 4) == 0 ||
                     strncasecmp(readBuffer, "rset", 4) == 0 ||
                     strncasecmp(readBuffer, "vrfy", 4) == 0 ||
                     strncasecmp(readBuffer, "expn", 4) == 0 ||
                     strncasecmp(readBuffer, "help", 4) == 0) {
                send_string(fd, "502 Unsupported command\r\n");
            } // if unsupported command
            else if (bytesRead <= 0) {
                // TODO how to close connection?
                // Properly closes connection if read (or nb_read_line) returns <= 0
            } else {
                send_string(fd, "500 Invalid command\r\n");
            }
        } else {
            send_string(fd, "500 Line too long\r\n");
        }
        // TODO Properly terminates message with line containing only period
        // make a wrapper for this and just break if it fails?
    }
}

void readEmailAddressFromBuffer(char* emailAddress, char* readBuffer) {
    char *emailHead = strchr(readBuffer, '<');
    if (emailHead == NULL) {
        return;
    }
    emailHead = emailHead + 1; // get string after <
    char *emailTail = strchr(emailHead, '>');
    if (emailTail != NULL) {
        size_t emailLength = emailTail - emailHead;
        memcpy(emailAddress, emailHead, emailLength);
        emailAddress[emailLength] = '\0';
    }
}

char* readData(int fd) {
    char readBuffer[1000] = ""; // empty buffer
    char* data = (char *) malloc(1000);

    while (true) {
        if (read(fd, readBuffer, 1000) > 0) {
            size_t bufferLength = strlen(readBuffer);

            // read until '.'
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