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
        char rec[100][100];
        int recCount=0;

        while(1)
        {

            printf("HELOOOO");
            char readBuffer[1000]=""; // empty buffer
            read(fd,readBuffer,1000);
            if(recCount!=0)
            {
                for(int i=0;i<recCount;i++)
                    printf("%s",rec[recCount]);
            }
            else
                printf("Rec Count is 0");

            printf("%s", readBuffer);
            printf("123");

            char bufOut[100]; // buffer to send client
            if(strncmp(readBuffer,"helo",4)==0) // if helo
            {
                strcpy(bufOut,"250 OK\r\n");
                send(fd,bufOut,strlen(bufOut),0); // send message
            }
            else if(strncmp(readBuffer,"mail",4)==0) // if mail
            {
                printf("This is mail");
                char mail[100];
                int k=0;
                for(int i=11;i<strlen(readBuffer)-2;i++) {   // reading the email id
                    mail[k] = readBuffer[i];
                    k++;
                }
                mail[k]='\0';
                printf("%s",mail);
                strcpy(bufOut,"250 OK\r\n");
                send(fd,bufOut,strlen(bufOut),0); //sending client message

            }
            else if(strncmp(readBuffer,"rcpt",4)==0)
            {
                char mail[100];
                int k=0;
                for(int i=9;i<strlen(readBuffer)-2;i++) // reading receive ID
                {
                    mail[k]=readBuffer[i];
                    k++;
                }
                mail[k]='\0';
                strcpy(rec[recCount],mail); // putting in 2D char array for multiple ids
                recCount++;
                printf("mail is %s",mail);
                printf("this is rec count %d",recCount);
            }
            else if(strncmp(readBuffer,"data",4)==0)
            {   char *data;
                data=(char *) malloc(strlen(readBuffer));
                char *temp;
                temp=(char *) malloc(strlen(readBuffer));
                while(strlen(readBuffer)==1 && readBuffer[0]=='.')
                {   strcpy(temp,data);
                    strcat(temp,readBuffer);
                    data=(char * )realloc(data,2*strlen(temp));
                    strcpy(data,temp);
                }
                data[strlen(data)-1]='\0';
            }
            else if(strncmp(readBuffer,"noop",4)==0)
            {
                printf("This is noop");
            }
            else if(strncmp(readBuffer,"quit",4)==0)
            {
                break;
            }
            else
            {

            }



        }
        // TODO To be implemented
    }


