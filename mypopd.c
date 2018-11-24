#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <sys/socket.h>

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



    while(1)
{    char readBuffer[1000] = ""; // empty buffer
char bufOut[1000]="";
    read(fd, readBuffer, 1000);
    if (strncmp(readBuffer, "user", 4) == 0)
    {
        printf("User case");
        printf(readBuffer);
        printf("length of buffer %d",strlen(readBuffer));
        strcpy(name,readBuffer+5);
        printf(" length is %d",strlen(name));
        printf(name);
        /*

        if(is_valid_user(name,NULL)==1)
        {
            printf("This is valid user");
            strcpy(bufOut,"+OK ");
            strcat(bufOut,name);
            strcat(bufOut," is a valid mailbox\r\n");
            send(fd, bufOut, strlen(bufOut), 0);
        }
        else {
            printf("This is invalid user");
            strcpy(bufOut,"-ERR never heard of mailbox ");
            strcat(bufOut,name);
            strcat(bufOut,"\r\n");
            send(fd, bufOut, strlen(bufOut), 0);
        }
         */
    }
    else if(strncmp(readBuffer, "pass", 4) == 0)
    {   printf("password case");
        strcpy(pass,readBuffer+5);
        printf("length of pass %d",strlen(pass));
        printf(pass);
        /*
        if(is_valid_user(name,pass)==1)
        {   printf("valid password");
            strcpy(bufOut, "250 OK\r\n");
            send(fd, bufOut, strlen(bufOut), 0);}
        else {
            printf("invalid password");
            strcpy(bufOut,"+OK maildrop locked and ready\r\n");

            send(fd, bufOut, strlen(bufOut), 0);
        }
        */
    }
    else if(strncmp(readBuffer, "stat", 4) == 0)
    {   /*printf("Hello");
        struct mail_list *list = NULL;
        list=load_user_mail("john.doe@example.com");
        int num;
        num=get_mail_count(list);
        size_t numBytes;
        numBytes=get_mail_list_size(list);
        printf("size of mail %d ",num);
        printf("size of bytes %zu",numBytes);
        strcpy(bufOut,"+OK ");
        char temp[100];
        sprintf(temp,"%d",num);
        strcat(bufOut,temp);
        strcat(bufOut," messages (");
        sprintf(numBytes,"%zu",numBytes);
        strcat(bufOut,temp);
        strcat(bufOut,"\r\n");
        send(fd, bufOut, strlen(bufOut), 0);
        */
    }
    else if(strncmp(readBuffer, "list", 4) == 0)
    {
        printf("list case");

        if(strlen(readBuffer)>4)
        {
            char mailNum[100];
            strcpy(mailNum,readBuffer+5);
            int mailCount;
            mailCount=atoi(mailNum);
            struct mail_list *list = NULL;
            list=load_user_mail("john.doe@example.com");
            struct mail_item *email=get_mail_item(list,mailCount-1);
            size_t mailSize= get_mail_item_size(email);
            printf("this is mail size %zu",mailSize);

        }
        else {
            struct mail_list *list = NULL;

            list = load_user_mail("john.doe@example.com");
            int num;
            num = get_mail_count(list);
            size_t numBytes;
            numBytes = get_mail_list_size(list);
            printf("size of mail %d ", num);
            printf("size of bytes %zu", numBytes);
            strcpy(bufOut, "+OK ");
            /*char temp[100];
            sprintf(temp,"%d",num);
            strcat(bufOut,temp);
            strcat(bufOut," messages (");
            sprintf(numBytes,"%zu",numBytes);
            strcat(bufOut,temp);
            strcat(bufOut,"\r\n");
            send(fd, bufOut, strlen(bufOut), 0);
            num=1;
            //memset(bufOut,0,1000); //reset the buffer
            */
            for (int i = 0; i < num; i++) {
                struct mail_item *email = get_mail_item(list, i);
                size_t mailSize = get_mail_item_size(email);
                printf("this is mail size %zu", mailSize);
            }
        }
        /*
        while(list)
        {
            size_t numBytes;

            numBytes=list->item.file_size;   // ERROR HERE
            printf("this is %d mail %zu size",num,numBytes);
            list=list->next;
            num++;
            char temp[100];
            sprintf(temp,"%d",num);
            strcpy(bufOut,temp);
            strcat(bufOut," ");
            sprintf(temp,"%zu",numBytes);
            strcat(bufOut,temp);
            strcat(bufOut,"\r\n");
            send(fd,bufOut,strlen(bufOut),0);
        }
         */
    }
    else if(strncmp(readBuffer, "retr", 4) == 0)
    {
        char mailNum[100];
        strcpy(mailNum,readBuffer+5);
        int mailCount;
        mailCount=atoi(mailNum);
        printf("this is mail count %d ",mailCount);
        struct mail_list *list = NULL;
        list=load_user_mail("john.doe@example.com");
        struct mail_item *email=get_mail_item(list,mailCount-1);
        char* name=get_mail_item_filename(email);
        printf("%s",name);
        int c;
        FILE *file;
        file = fopen(name, "r");
        if (file) {
            while ((c = getc(file)) != EOF)
                putchar(c);
            fclose(file);
        }
    }
    else if(strncmp(readBuffer, "dele", 4) == 0)
    {
        char mailNum[100];
        strcpy(mailNum,readBuffer+5);
        int mailCount;
        mailCount=atoi(mailNum);
        printf("this is mail count %d ",mailCount);
        struct mail_list *list = NULL;
        list=load_user_mail("john.doe@example.com");
        struct mail_item *email=get_mail_item(list,mailCount-1);
        mark_mail_item_deleted(email);

    }
    else
    {

    }

}
}
