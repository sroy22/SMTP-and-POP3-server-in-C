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
    struct mail_list *list;

    while (true) {
        char readBuffer[MAX_LINE_LENGTH] = ""; // empty buffer
        char bufOut[1000] = "";
        ssize_t bufferLength = read(fd, readBuffer, 1000);
        readBuffer[bufferLength] = '\0';
        if(strlen(readBuffer)> MAX_LINE_LENGTH) // Todo error code
            printf("ERROR\n");

        if (strncasecmp(readBuffer, "user", 4) == 0) {
            // TODO make the +5 dynamic
            strcpy(name, readBuffer + 5);
            name[strlen(name) - 1] = '\0'; // NULL termination for name
            if(strlen(readBuffer)<=5)
            {
                send_string(fd, "This command requires a parameter\r\n"); // todo exact return statement
            }
            else if (is_valid_user(name, NULL)) {
                printf("This is valid user\n");
                strcpy(bufOut, "+OK ");
                strcat(bufOut, name);
                strcat(bufOut, " is a valid mailbox\r\n");
                send_string(fd, bufOut);
            } else {
                printf("This is invalid user");
                strcpy(bufOut, "-ERR never heard of mailbox ");
                strcat(bufOut, name);
                strcat(bufOut, "\r\n");
                send_string(fd, bufOut);
            }
            printf("Exit\n");
        } else if (strncasecmp(readBuffer, "pass", 4) == 0) {
            // TODO +5 dynamic
            strcpy(pass, readBuffer + 5 + strlen(name)+1 );
            pass[strlen(pass) - 1] = 0;


            if (is_valid_user(name, pass)) {
                printf("valid password\n");
                send_string(fd, "250 OK\r\n");
                list = load_user_mail(name);
            } else {
                printf("invalid password\n");
                send_string(fd, "+OK maildrop locked and ready\r\n");
            }
        } else if (strncasecmp(readBuffer, "stat", 4) == 0) {
            printf("Hello\n");
            int num = get_mail_count(list);
            if (num == 0){
                // TODO
                printf("no emails");
            } else {
                size_t numBytes = get_mail_list_size(list);
                printf("size of mail %d\n", num);
                printf("size of bytes %zu\n", numBytes);
                strcpy(bufOut, "+OK ");
                char temp[100];
                sprintf(temp, "%d", num);
                strcat(bufOut, temp);
                strcat(bufOut, " ");
                sprintf(temp, "%zu", numBytes);
                strcat(bufOut, temp);
                strcat(bufOut, "\r\n");
                send(fd, bufOut, strlen(bufOut), 0);
            }
        } else if (strncasecmp(readBuffer, "list", 4) == 0) {
            printf("list case\n");
            if (bufferLength > (ssize_t) 5) {
                // TODO check mailIndex exists
                char mailIndex[100];
                strcpy(mailIndex, readBuffer + 5);
                mailIndex[strlen(mailIndex) - 1] = 0;
                int mailIndexInt = atoi(mailIndex);
                int mailCount = get_mail_count(list);
                if (mailCount == 0) {
                    // TODO
                } else {
                    int reverseMailIndex = mailCount - mailIndexInt;
                    // if out of index or 0
                    if (reverseMailIndex < 0 || mailCount == reverseMailIndex) {
                        strcpy(bufOut, "-ERR no such message, only ");

                        char numBuffer[1000];
                        sprintf(numBuffer, "%d", mailCount);
                        strcat(bufOut, numBuffer);

                        strcat(bufOut, " messages in maildrop\r\n");
                    } else {
                        mail_item_t email = get_mail_item(list, (unsigned) reverseMailIndex);
                        size_t mailSize = get_mail_item_size(email);
                        printf("this is mail size %zu\n", mailSize);

                        char numBuffer[1000];
                        sprintf(numBuffer, "%zu", mailSize);

                        strcpy(bufOut, "+OK ");
                        strcat(bufOut, mailIndex);
                        strcat(bufOut, " ");
                        strcat(bufOut, numBuffer);
                        strcat(bufOut, "\r\n");
                    }
                    send_string(fd, bufOut);
                }
            } else {
                // TODO list
                struct mail_list *list = load_user_mail(name);
                int num = get_mail_count(list);
                size_t numBytes = get_mail_list_size(list);

                strcpy(bufOut, "+OK ");
                char numBuffer[1000];
                sprintf(numBuffer, "%d", num);
                strcat(bufOut, numBuffer);
                strcat(bufOut, " messages (");
                sprintf(numBuffer, "%zu", numBytes);
                strcat(bufOut, numBuffer);
                strcat(bufOut, " octets)\r\n");
                send_string(fd, bufOut);

                for (int i = num - 1; i >= 0; i--) {
                    memset(bufOut, 0, sizeof bufOut);

                    struct mail_item *email = get_mail_item(list, (unsigned) i);
                    size_t mailSize = get_mail_item_size(email);
                    printf("this is mail size %zu\n", mailSize);

                    sprintf(numBuffer, "%d", num - i);
                    strcat(bufOut, numBuffer);
                    strcat(bufOut, " ");
                    sprintf(numBuffer, "%zu", mailSize);
                    strcat(bufOut, numBuffer);
                    strcat(bufOut, "\r\n");
                    send_string(fd, bufOut);
                }

            }
        } else if (strncasecmp(readBuffer, "retr", 4) == 0) {
            char mailIndex[100];
            if (bufferLength > 5) {
                strcpy(mailIndex, readBuffer + 5);
                mailIndex[strlen(mailIndex) - 1] = '\0';
                printf(mailIndex);

                int mailIndexInt = atoi(mailIndex);
                printf("this is mail count %d\n", mailIndexInt);
                // TODO mailcount == 0
                int mailCount = get_mail_count(list);
                mail_item_t email = get_mail_item(list, (unsigned) (mailCount - mailIndexInt));
                if (email != NULL) {
                    const char *fileName = get_mail_item_filename(email);
                    printf("%s\n", fileName);
                    char ch;
                    char fileBuffer[2000];
                    int k=0;
                    FILE *fp;
                    fp = fopen(fileName, "r"); // read mode

                    while((ch = fgetc(fp)) != EOF) {
                        printf("%c", ch);
                        fileBuffer[k] = ch;
                        k++;
                    }
                    printf("%s\n",fileBuffer);
                    fileBuffer[k]='\r\n';
                    fileBuffer[k]=0;
                    send_string(fd,fileBuffer);
                    fclose(fp);


                    /*int c;
                    FILE *file = fopen(fileName, "r");
                    if (file) {
                        while ((c = getc(file)) != EOF) {
                            putchar(c);
                        }
                        fclose(file);
                    }
                     */
                } else {
                    // TODO error
                    printf("Could not find email\n");
                }
            } else {
                // TODO error
                printf("Enter a mail index\n");
            }
        } else if (strncasecmp(readBuffer, "dele", 4) == 0) { // TODO getting HANGED
            char mailIndex[100];
            strcpy(mailIndex, readBuffer + 5);
            int mailIndexInt = atoi(mailIndex);
            printf("this is mail count %d\n", mailIndexInt);
            // TODO mailCount == 0
            // TODO -ERR already deleted
            int mailCount = get_mail_count(list);
            mail_item_t email = get_mail_item(list, (unsigned) (mailCount - mailIndexInt));

            const char *fileName = get_mail_item_filename(email);
            printf("%s\n", fileName);

            mark_mail_item_deleted(email);
        }
        else if (strncasecmp(readBuffer, "rset", 4) == 0) {
            printf("This is rset case\n");
            // TODO mailCount == 0 RFC
            unsigned  int reset_Count= reset_mail_list_deleted_flag(list);
            printf("%u",reset_Count);
            printf("RSET\n");
        }
        else if (strncasecmp(readBuffer, "quit", 4) == 0) {
            break;
        }
        else if (strncasecmp(readBuffer, "noop", 4) == 0) {
            // TODO FSM
            send_string(fd, "YEET\r\n");
        }
        else if (strncasecmp(readBuffer, "top", 3) == 0 ||
                 strncasecmp(readBuffer, "uidl", 4) == 0 ||
                 strncasecmp(readBuffer, "apop", 4)==0 ) {
            send_string(fd, "502 Unsupported command\r\n");
        }
        else
        {

        }
    }
}
