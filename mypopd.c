#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <sys/socket.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);

int fsmState = -1;

char name[1000];
char pass[1000];

struct mail_list *list;
int mailCount = -1;

bool isMaildropLocked = false;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handleStat(struct mail_list* list, int fd) {
    if (fsmState == 2) {
        int currentMailCount = get_mail_count(list);
        size_t numListBytes = get_mail_list_size(list);

        send_string(fd, "+OK %d %zu\r\n", currentMailCount, numListBytes);
    } else {
        send_string(fd, "-ERR Invalid command\r\n");
    }
}

void handleList(int mailCount, struct mail_list* list, int fd) {
    if (fsmState == 2) {
        size_t numListBytes = get_mail_list_size(list);
        // use a separate mail count to be able to loop through all mail items even when deleted
        int currentMailCount = get_mail_count(list);

        send_string(fd, "+OK %d messages (%zu octets)\r\n", currentMailCount, numListBytes);

        // iterate backwards because list is stored like a stack
        for (int i = mailCount - 1; i >= 0; i--) {
            struct mail_item *email = get_mail_item(list, (unsigned) i);
            if  (email != NULL) {
                size_t mailSize = get_mail_item_size(email);

                // return correct index from the front (starting from 1)
                send_string(fd, "%d %zu\r\n", mailCount - i, mailSize);
            }
        }
        send_string(fd, ".\r\n");
    } else {
        send_string(fd, "-ERR Invalid command\r\n");
    }

}

void handleListWithVars(char readBuffer[], int mailCount, struct mail_list* list, int fd) {
    if (fsmState == 2) {
        char mailIndex[100];
        strcpy(mailIndex, readBuffer + 5);
        mailIndex[strlen(mailIndex) - 1] = '\0';
        int mailIndexInt = atoi(mailIndex);

        int reverseMailIndex = mailCount - mailIndexInt;
        // if out of index or 0
        if (reverseMailIndex < 0 || mailCount == reverseMailIndex) {
            send_string(fd, "-ERR no such message, only %d messages in maildrop\r\n", mailCount);
        } else {
            mail_item_t email = get_mail_item(list, (unsigned) reverseMailIndex);
            if (email == NULL) {
                send_string(fd, "-ERR item has been deleted\r\n");
            } else {
                size_t mailSize = get_mail_item_size(email);
                send_string(fd, "+OK %s %zu\r\n", mailIndex, mailSize);
            }
        }
    } else {
        send_string(fd, "-ERR Invalid command\r\n");
    }

}

void handleUser(char *name, char readBuffer[], int fd) {
    // After initialization, or failed USER / PASS command (State 0)
    if (fsmState == 0) {
        strcpy(name, readBuffer + 5);
        name[strlen(name) - 1] = '\0'; // NULL termination for name
        if (is_valid_user(name, NULL)) {
            send_string(fd, "+OK %s is a valid mailbox\r\n", name);
            fsmState = 1;
        } else {
            send_string(fd, "-ERR never heard of mailbox %s\r\n", name);
        }
    } else {
        send_string(fd, "-ERR not in AUTHORIZATION state");
    }
}

void handlePass(char *pass, char readBuffer[], int fd) {
    if (fsmState != 1) {
        send_string(fd, "-ERR unable to lock maildrop\r\n");
    } else {
        strcpy(pass, readBuffer + 5);
        pass[strlen(pass) - 1] = '\0';

        if (isMaildropLocked) {
            send_string(fd, "-ERR maildrop already locked\r\n");
        } else if (is_valid_user(name, pass)) {
            list = load_user_mail(name);
            mailCount = get_mail_count(list);
            isMaildropLocked = true;
            fsmState = 2;
            send_string(fd, "+OK maildrop locked and ready\r\n");
        } else {
            send_string(fd, "-ERR invalid password\r\n");
            fsmState = 0;
        }
    }
}

void handleRetr(char readBuffer[], int mailCount, struct mail_list* list, int fd) {
    if (fsmState == 2) {
        char mailIndex[100];
        strcpy(mailIndex, readBuffer + 5);
        mailIndex[strlen(mailIndex) - 1] = '\0';
        int mailIndexInt = atoi(mailIndex);
        mail_item_t email = get_mail_item(list, (unsigned) (mailCount - mailIndexInt));

        if (email != NULL) {
            const char *fileName = get_mail_item_filename(email);

            char ch;
            int k=0;
            char fileBuffer[2000];
            FILE *fp = fopen(fileName, "r"); // read mode
            while((ch = fgetc(fp)) != EOF) {
                printf("%c", ch);
                fileBuffer[k] = ch;
                k++;
            }
            fclose(fp);

            fileBuffer[k]= '\r\n';
            fileBuffer[k+1]=0;
            send_string(fd, fileBuffer);
            send_string(fd, ".\r\n");
        } else {
            send_string(fd, "-ERR Could not find email\r\n");
        }
    } else {
        send_string(fd, "-ERR Invalid command\r\n");
    }

}

void handleDele(char readBuffer[], int mailCount, struct mail_list* list, int fd) {
    if (fsmState == 2) {
        char mailIndex[100];
        strcpy(mailIndex, readBuffer + 5);
        mailIndex[strlen(mailIndex) - 1] = '\0';
        int mailIndexInt = atoi(mailIndex);
        int reverseMailIndex = mailCount - mailIndexInt;
        // if out of index or 0
        if (reverseMailIndex < 0 || mailCount == reverseMailIndex) {
            send_string(fd, "-ERR index out of bounds, only %d messages in maildrop\r\n", mailCount);
        } else {
            mail_item_t email = get_mail_item(list, (unsigned) reverseMailIndex);
            if (email == NULL) {
                send_string(fd, "-ERR item has already been deleted\r\n");
            } else {
                mark_mail_item_deleted(email);
                send_string(fd, "+OK message %s deleted\r\n", mailIndex);
            }
        }
    } else {
        send_string(fd, "-ERR Invalid command");
    }
}

void handle_client(int fd) {
    send_string(fd, "+OK POP3 server ready\r\n");

    fsmState = 0;

    while (true) {
        char readBuffer[MAX_LINE_LENGTH] = ""; // empty buffer
        ssize_t bufferLength = read(fd, readBuffer, 1000);
        readBuffer[bufferLength] = '\0';

        if(bufferLength > MAX_LINE_LENGTH) {
            send_string(fd, "-ERR Maximum line exceeded\r\n");
        }

        if (strncasecmp(readBuffer, "top", 3) == 0 ||
            strncasecmp(readBuffer, "uidl", 4) == 0 ||
            strncasecmp(readBuffer, "apop", 4)==0 ) {
            send_string(fd, "-ERR Unsupported command\r\n");
        } else if (bufferLength <= 5) {
            if (strncasecmp(readBuffer, "stat", 4) == 0) {
                handleStat(list, fd);
            } else if (strncasecmp(readBuffer, "list", 4) == 0) {
                handleList(mailCount, list, fd);
            } else if (strncasecmp(readBuffer, "rset", 4) == 0) {
                if (fsmState == 2) {
                    reset_mail_list_deleted_flag(list);
                    size_t listSize = get_mail_list_size(list);
                    send_string(fd, "+OK maildrop has %d messages (%zu octets)\r\n", mailCount, listSize);
                } else {
                    send_string(fd, "-ERR Invalid command\r\n");
                }

            } else if (strncasecmp(readBuffer, "quit", 4) == 0) {
                if (fsmState == 2) {
                    destroy_mail_list(list);
                }
                send_string(fd, "+OK Goodbye\r\n");
                break;
            }
            else if (strncasecmp(readBuffer, "noop", 4) == 0) {
                if (fsmState == 2) {
                    send_string(fd, "+OK\r\n");
                } else {
                    send_string(fd, "-ERR Invalid command\r\n");
                }
            } else {
                send_string(fd, "-ERR Invalid command\r\n");
            }
        } else {
            // xxxx_ because user input must have > 5 length and must have a space, otherwise needs param
            if (strncasecmp(readBuffer, "user ", 5) == 0) {
                handleUser(name, readBuffer, fd);
            } else if (strncasecmp(readBuffer, "pass ", 5) == 0) {
                handlePass(pass, readBuffer, fd);
            } else if (strncasecmp(readBuffer, "list ", 5) == 0) {
                handleListWithVars(readBuffer, mailCount, list, fd);
            } else if (strncasecmp(readBuffer, "retr ", 5) == 0) {
                handleRetr(readBuffer, mailCount, list, fd);
            } else if (strncasecmp(readBuffer, "dele ", 5) == 0) {
                handleDele(readBuffer, mailCount, list, fd);
            } else {
                send_string(fd, "-ERR Invalid command\r\n");
            }
        }
    }
}
