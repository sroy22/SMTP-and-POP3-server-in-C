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

    net_buffer_t nb = nb_create(fd, MAX_LINE_LENGTH);

    while (true) {
        char readBuffer[MAX_LINE_LENGTH] = ""; // empty buffer
        int bytesRead = nb_read_line(nb, readBuffer);

        // if connection close 0; if terminated -1
        if (bytesRead <= 0) {
            break;
        }

        size_t bufferLength = strlen(readBuffer);
        if(bufferLength <= MAX_LINE_LENGTH) {
            if (strncasecmp(readBuffer, "helo ", 5) == 0 && bufferLength > 7) {
                // TODO check there's argument
                if (fsmState == 0 || fsmState == 1) {
                    if (readBuffer[5] != ' ') {
                        send_string(fd, "250 %s\r\n", unameData.__domainname);
                        fsmState = 1;
                    } else {
                        send_string(fd, "500 Invalid command\r\n");
                    }
                } else {
                    send_string(fd, "503 Bad sequence of commands\r\n");
                }
            }
            else if (strncasecmp(readBuffer, "mail from:", 10) == 0 && bufferLength > 12) {
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
            }
            else if (strncasecmp(readBuffer, "rcpt to:", 8) == 0 && bufferLength > 10) {
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
                        send_string(fd, "550 recipient not in users.txt\r\n");
                    };
                } else {
                    send_string(fd, "503 Bad sequence of commands\r\n");
                }

            }
            else if (strncasecmp(readBuffer, "data\r\n", 6) == 0 && bufferLength == 6) {
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

            }
            else if (strncasecmp(readBuffer, "noop\r\n", 6) == 0 && bufferLength == 6) {
                send_string(fd, "250 OK\r\n");
            }
            else if (strncasecmp(readBuffer, "quit\r\n", 6) == 0 && bufferLength == 6) {
                send_string(fd, "221 Goodbye.\r\n");
                destroy_user_list(recipients);
                break;
            }
            else if ((strncasecmp(readBuffer, "ehlo\r\n", 6) == 0 ||
                      strncasecmp(readBuffer, "rset\r\n", 6) == 0 ||
                      strncasecmp(readBuffer, "vrfy\r\n", 6) == 0 ||
                      strncasecmp(readBuffer, "expn\r\n", 6) == 0 ||
                      strncasecmp(readBuffer, "help\r\n", 6) == 0) &&
                     bufferLength == 6) {
                send_string(fd, "502 Unsupported command\r\n");
            } else {
                send_string(fd, "500 Invalid command\r\n");
            }
        } else {
            send_string(fd, "500 Line too long\r\n");
        }
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
    char readBuffer[MAX_LINE_LENGTH] = ""; // empty buffer
    char* data = (char *) malloc(MAX_LINE_LENGTH);

    net_buffer_t dataNB = nb_create(fd, MAX_LINE_LENGTH);
    while (true) {
        if (nb_read_line(dataNB, readBuffer) > 0) {
            size_t bufferLength = strlen(readBuffer);

            // read until '.'
            if (strncasecmp(readBuffer, ".\r\n", 3) == 0 && bufferLength == 3) {
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