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

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {

    char name[1000];
    char pass[1000];

    while (true) {
        char readBuffer[1000] = ""; // empty buffer
        char bufOut[1000] = "";
        ssize_t bufferLength = read(fd, readBuffer, 1000);
        readBuffer[bufferLength] = '\0';
        if (strncmp(readBuffer, "user", 4) == 0) {
            // TODO make the +5 dynamic
            strcpy(name, readBuffer + 5);
            name[strlen(name) - 1] = '\0'; // NULL termination for name

            if (is_valid_user(name, NULL)) {
                printf("This is valid user\n");
                strcpy(bufOut, "+OK ");
                strcat(bufOut, name);
                strcat(bufOut, " is a valid mailbox\r\n");
                send_string(fd, "+OK\r\n");
            } else {
                printf("This is invalid user");
                strcpy(bufOut, "-ERR never heard of mailbox ");
                strcat(bufOut, name);
                strcat(bufOut, "\r\n");
                send(fd, bufOut, strlen(bufOut), 0);
            }
            printf("Exit\n");
        } else if (strncmp(readBuffer, "pass", 4) == 0) {
            // TODO +5 dynamic
            strcpy(pass, readBuffer + 5);
            pass[strlen(pass) - 1] = 0;

            if (is_valid_user(name, pass)) {
                printf("valid password\n");
                send_string(fd, "250 OK\r\n");
            } else {
                printf("invalid password\n");
                send_string(fd, "+OK maildrop locked and ready\r\n");
            }
        } else if (strncmp(readBuffer, "stat", 4) == 0) {
            printf("Hello\n");
            struct mail_list *list = load_user_mail(name);
            int num = get_mail_count(list);
            size_t numBytes = get_mail_list_size(list);
            printf("size of mail %d\n", num);
            printf("size of bytes %zu\n", numBytes);
            strcpy(bufOut, "+OK ");
            char temp[100];
            sprintf(temp, "%d", num);
            strcat(bufOut, temp);
            strcat(bufOut, " messages (");
            //sprintf(numBytes, "%zu", numBytes);
            strcat(bufOut, temp);
            strcat(bufOut, ")\r\n");
            send(fd, bufOut, strlen(bufOut), 0);
        } else if (strncmp(readBuffer, "list", 4) == 0) {
            printf("list case\n");
            if (bufferLength > (ssize_t) 5) {
                char mailNum[100];
                strcpy(mailNum, readBuffer + 5);
                mailNum[strlen(mailNum) - 1] = 0;
                int mailCount;
                mailCount = atoi(mailNum);
                struct mail_list *list = load_user_mail(name);
                mail_item_t email = get_mail_item(list, (unsigned) mailCount);
                size_t mailSize = get_mail_item_size(email);
                printf("this is mail size %zu\n", mailSize);
            } else {
                struct mail_list *list = load_user_mail(name);
                int num = get_mail_count(list);
                size_t numBytes = get_mail_list_size(list);
                printf("size of mail %d\n", num);
                printf("size of bytes %zu\n", numBytes);
                strcpy(bufOut, "+OK ");
                for (int i = 0; i < num; i++) {
                    struct mail_item *email = get_mail_item(list, i);
                    size_t mailSize = get_mail_item_size(email);
                    printf("this is mail size %zu\n", mailSize);
                }
            }
        } else if (strncmp(readBuffer, "retr", 4) == 0) {
            char mailNum[100];
            if (bufferLength > 5) {
                strcpy(mailNum, readBuffer + 5);
                mailNum[strlen(mailNum) - 1] = '\0';
                printf(mailNum);
                char *ptr;
                unsigned long mailCount = strtoul(mailNum, &ptr, 10);
                printf("this is mail count %zu\n", mailCount);
                struct mail_list *list = load_user_mail(name);
                struct mail_item *email = get_mail_item(list, (unsigned) mailCount);
                if (email != NULL) {
                    const char *fileName = get_mail_item_filename(email);
                    printf("%s\n", fileName);
                    int c;
                    FILE *file = fopen(fileName, "r");
                    if (file) {
                        while ((c = getc(file)) != EOF)
                            putchar(c);
                        fclose(file);
                    }
                } else {
                    // TODO error
                    printf("Could not find email\n");
                }
            } else {
                // TODO error
                printf("Enter a mail index\n");
            }
        } else if (strncmp(readBuffer, "dele", 4) == 0) {
            char mailNum[100];
            strcpy(mailNum, readBuffer + 5);
            int mailCount;
            mailCount = atoi(mailNum);
            printf("this is mail count %d ", mailCount);
            struct mail_list *list = NULL;
            list = load_user_mail("john.doe@example.com");
            struct mail_item *email = get_mail_item(list, mailCount - 1);
            mark_mail_item_deleted(email);
        } else if (strncmp(readBuffer, "quit", 4) == 0) {
            break;
        } else {
        }
    }
}
