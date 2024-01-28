#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CHECK(op)   do { if ( (op) == -1) { perror (#op); exit (EXIT_FAILURE); } \
                    } while (0)

#define PORT(p) htons(p)

#define ERROR "-1"
#define FILEIO "2"
#define START "1"
#define STOP "0"
#define SIZE 1024

// envoies le fichier à l'utilisateur
// msg: fichier à envoyer
// sockfd: le socket à utiliser
// ss: les informations sur l'utilisateur
// return: void

void sendFile(char *msg, int sockfd, struct sockaddr_storage ss)
{
    int fd, index = 0;
    ssize_t nlus;
    size_t length = strlen(msg);
    socklen_t addrlen = sizeof(ss);

    for (size_t i = 0; i < length; i++)
    {
        if(msg[i] == '/')
            index = i;
    }

    // traitement d'erreur

    fd = open(msg, O_RDONLY);
    if(fd == -1 && errno == ENOENT)
    {
        printf("This file doesn't exist.\n");
        CHECK(sendto(sockfd, ERROR, sizeof(ERROR), 0, (struct sockaddr *) &ss, addrlen));
        return;
    }
    else if(fd == -1 && errno == EACCES)
    {
        printf("Access to the file is not allowed.\n");
        CHECK(sendto(sockfd, ERROR, sizeof(ERROR), 0, (struct sockaddr *) &ss, addrlen));
        return;
    }
    else if(fd == -1)
    {
        printf("An error occured.\n");
        CHECK(sendto(sockfd, ERROR, sizeof(ERROR), 0, (struct sockaddr *) &ss, addrlen));
        return;
    }

    // le fichier peut venir d'un autre répertoire

    char filename[length-index];
    if(index == 0)
        strcpy(filename,msg + index);
    else
        strcpy(filename,msg + index+1);

    CHECK(sendto(sockfd,filename,length,0, (struct sockaddr *) &ss, addrlen));

    while( (nlus = read(fd,msg,SIZE)) != 0)
    {
        CHECK(sendto(sockfd,msg,nlus,0, (struct sockaddr *) &ss, addrlen));
    }
    CHECK(sendto(sockfd,"EOF",strlen("EOF"), 0, (struct sockaddr *) &ss, addrlen));

    CHECK(close(fd));
    printf("File \"%s\" was sent successfully.\n",filename);
}

// recoit un fichier d'un utilisateur
// msg: fichier à recevoir
// sockfd: socket à utiliser
// ss: les informations sur l'utilsateur
// return: void

void receiveFile(char *msg, int sockfd, struct sockaddr_storage ss)
{
    int fd;
    ssize_t nlus;
    size_t length = strlen(msg);
    socklen_t addrlen = sizeof(ss);
    char title[length+2];

    fd = open(msg, O_CREAT | O_EXCL | O_WRONLY, 0644);
    
    if(fd == -1 && errno == EEXIST)
    {
        char type[length-strcspn(msg,".")];
        strcpy(type,msg+strcspn(msg,"."));
        char *name = strtok(msg,".");
        int i = 0;

        while (fd == -1 && errno == EEXIST)
        {
            i++;
            sprintf(title, "%s%d%s",name,i,type);
            fd = open(title, O_CREAT | O_EXCL | O_WRONLY, 0644);
        }
    }
    else
        strcpy(title,msg);

    
    
    while( (nlus = recvfrom(sockfd,msg,SIZE,0,(struct sockaddr *) &ss, &addrlen)) > 0)
    {
        if(strncmp(msg,"EOF",strlen("EOF")) == 0)
            break;
        CHECK(write(fd,msg,nlus));
    }
    CHECK(nlus);
    CHECK(close(fd));
    printf("You received the file \"%s\".\n",title);
}

int main (int argc, char *argv [])
{

    int port_number, sockfd;
    struct sockaddr_storage ss;
    struct sockaddr_in6 *in6;

    ssize_t nlus;
    socklen_t addrlen = sizeof(ss);
    char host[SIZE], port[SIZE];
    char msg[SIZE] = "";
    int rValue;

    struct pollfd pfd[2];

    /* test arg number */

    if (argc != 2)
    {
        fprintf(stderr,"usage: %s port_number\n",argv[0]);
        exit(EXIT_FAILURE);
    }    

    /* convert and check port number */

    port_number = atoi(argv[1]);
    if (port_number < 10000 || port_number > 65000)
    {
        fprintf(stderr,"erreur numero de port\n");
        exit(EXIT_FAILURE);
    }

    /* create socket */

    CHECK(sockfd = socket(AF_INET6,SOCK_DGRAM,0));

    /* set dual stack socket */

    int value = 0;
    CHECK(setsockopt(sockfd,IPPROTO_IPV6, IPV6_V6ONLY, &value, sizeof value));

    /* set local addr */

    memset(&ss,0,sizeof(ss));
    in6 = (struct sockaddr_in6 *) &ss;
    in6->sin6_family = AF_INET6;
    in6->sin6_port = PORT(port_number);
    in6->sin6_addr = in6addr_any;

    /* check if a client is already present */

    bind(sockfd, (struct sockaddr *) &ss, addrlen);
    if(errno == EADDRINUSE)
    {
        CHECK(sendto(sockfd, START, sizeof(START), 0, (struct sockaddr *) &ss, addrlen));
    }
    else if (errno != 0)
    {
        fprintf(stderr,"erreur de bind\n");
        CHECK(close(sockfd));
        exit(EXIT_FAILURE);
    }
    else
    {

        CHECK(nlus = recvfrom(sockfd, msg, SIZE, 0, (struct sockaddr *) &ss, &addrlen));
        if(strcmp(msg,START) != 0)
        {
            fprintf(stderr,"erreur de connection\n");
            CHECK(close(sockfd));
            exit(EXIT_FAILURE);
        }
        rValue = getnameinfo((struct sockaddr *) &ss, addrlen, host, SIZE, port, SIZE, NI_NUMERICHOST);
        if(rValue != 0)
        {
            fprintf(stderr, "erreur de reception: %s\n", gai_strerror(rValue));
            CHECK(close(sockfd));
            exit(EXIT_FAILURE);
        }
        else
            printf("%s %s\n", host, port);
    }
        
    /* prepare struct pollfd with stdin and socket for incoming data */

    pfd[0].fd = STDIN_FILENO;
    pfd[1].fd = sockfd;

    pfd[0].events = POLLIN;
    pfd[1].events = POLLIN;

    /* main loop */

    while (1)
    {
        CHECK(poll(pfd, 2, -1));

        if(pfd[0].revents & POLLIN)
        {
            CHECK(nlus = read(STDIN_FILENO, msg, SIZE));
            msg[strcspn(msg,"\n")] = '\0';

            if(strcmp(msg,FILEIO) == 0)
            {
                CHECK(sendto(sockfd,msg,nlus,0, (struct sockaddr *) &ss, addrlen));
                printf("What file would you like to send?\n");
                CHECK(nlus = read(STDIN_FILENO, msg, SIZE));
                msg[strcspn(msg,"\n")] = '\0';
                sendFile(msg,sockfd,ss);
            }
            else
            {
                CHECK(sendto(sockfd,msg,nlus,0, (struct sockaddr *) &ss, addrlen));
                if(strcmp(msg,STOP) == 0)
                    break;
            }
        }
        
        if(pfd[1].revents & POLLIN)
        {
            CHECK(nlus = recvfrom(sockfd,msg,SIZE,0,(struct sockaddr *) &ss, &addrlen));
            msg[nlus] = '\0';

            if(strcmp(msg,FILEIO) == 0)
            {
                printf("File incoming...\n");
                CHECK(nlus = recvfrom(sockfd,msg,SIZE,0,(struct sockaddr *) &ss, &addrlen));
                msg[nlus] = '\0';
                if(strcmp(msg,ERROR) == 0)
                    printf("Something went wrong on sender's part.\n");
                else
                    receiveFile(msg,sockfd,ss);
            }
            else
            {
                if(strcmp(msg,STOP) == 0)
                    break;
                printf("%s\n", msg);
            }
        }
    }
    

    /* close socket */

    CHECK(close(sockfd));

    /* free memory */

    return 0;
}