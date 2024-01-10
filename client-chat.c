/*
    client-chat : execution basique
    client-chat-bin : execution avec protocole en mode binaire
    client-chat-fileio : execution avec transfert de fichier
    client-chat-usr : execution avec gestion avancée des utilisateurs
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef FILEIO
#include <sys/stat.h>
#include <fcntl.h>
#endif

#define CHECK(op)   do { if ( (op) == -1) { perror (#op); exit (EXIT_FAILURE); } \
                    } while (0)

#define PORT(p) htons(p)


#define START "/HELO"
#define STOP "/QUIT"
#ifdef BIN
#define STARTBIN "1"
#define STOPBIN "0"
#endif
#define SIZE 1024
#ifdef USR
#define N 4
#define GROUPSTART "/GROUP"
#else
#define N 2
#endif

/*  Mauvaise interprétation de la partie 3.1

#ifdef BIN

// Transforme une chaine de caractères de lettres en une chaine de caractères de 0 et 1
// string: message à transformer
// binary: message en binaire sera stocké dans ce paramètre
// return: void 

void stringToBinary(char* string, char* binary)
{
    char c;
    size_t i;
    size_t length = strlen(string);
    binary[0] = '\0';
    for(i = 0; i < length; ++i) {
        c = string[i];
        for(int j = 7; j >= 0; --j){
            if(c & (1 << j)) {
                strcat(binary,"1");
            } else {
                strcat(binary,"0");
            }
        }
    }
}


// Transforme une chaine de caractères de 0 et 1 en une chaine de caractères de lettres
// binary: message à transformer
// string: message sera stocké dans ce paramètre
// return: void

void binaryToString(char* binary, char* string)
{
    char c[8];
    size_t len = strlen(binary);
    size_t i;
    for (i = 0; i < len/8; i++)
    {
        strncpy(c,&binary[i*8],8);
        string[i] = strtol(c,0,2);
    }
    string[i] = '\0';
}
#endif
*/

#ifdef FILEIO

// vérifie si la chaine de caractères est un fichier de type TXT ou non
// string: message à vérifier
// return:  - 1 s'il s'agit d'un fichier
//          - 0 sinon

int isTXT(char* string)
{
    size_t length = strlen(string);
    char *fileEnd = ".txt";
    size_t endLength = strlen(fileEnd);

    if(length < endLength)
        return 0;

    char *tmp = string + length - endLength;

    if(strcmp(tmp, fileEnd) == 0)
        return 1;

    return 0;
}

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
        return;
    }
    else if(fd == -1 && errno == EACCES)
    {
        printf("Access to the file is not allowed.\n");
        return;
    }
    else if(fd == -1)
    {
        printf("An error occured.\n");
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

#endif

int main (int argc, char *argv [])
{

    int port_number, sockfd;
    struct sockaddr_storage ss;
    #ifdef USR
    struct sockaddr_storage cs[N-1];
    struct sockaddr_storage empty;
    memset(&empty, 0, sizeof(struct sockaddr_storage));
    #endif
    struct sockaddr_in6 *in6;

    ssize_t nlus;
/*/
#ifdef BIN
    size_t length;
#endif
*/
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
#ifdef BIN
        /*
        char binary[strlen(START)*8+1];
        stringToBinary(START,binary);
        */
        CHECK(sendto(sockfd, STARTBIN, sizeof(STARTBIN), 0, (struct sockaddr*) &ss, addrlen));
#else
        CHECK(sendto(sockfd, START, sizeof(START), 0, (struct sockaddr *) &ss, addrlen));
#ifdef USR
        CHECK(nlus = recvfrom(sockfd, msg, SIZE, 0, (struct sockaddr *) &ss, &addrlen));
        if(strcmp(msg,GROUPSTART) != 0)
        {
            fprintf(stderr,"erreur de connection\n");
            CHECK(close(sockfd));
            exit(EXIT_FAILURE);
        }
#endif
#endif
    }
    else if (errno != 0)
    {
        fprintf(stderr,"erreur de bind\n");
        CHECK(close(sockfd));
        exit(EXIT_FAILURE);
    }
    else
    {
        int nUsers = 1;
        while (nUsers < N)
        {        
            CHECK(nlus = recvfrom(sockfd, msg, SIZE, 0, (struct sockaddr *) &ss, &addrlen));
            #ifdef USR
            cs[nUsers-1] = ss;
            #endif
#ifdef BIN
            if(strcmp(msg,STARTBIN) != 0)
            {
                fprintf(stderr,"erreur de connection\n");
                CHECK(close(sockfd));
                exit(EXIT_FAILURE);
            }
#else
            if(strcmp(msg,START) != 0)
            {
                fprintf(stderr,"erreur de connection\n");
                CHECK(close(sockfd));
                exit(EXIT_FAILURE);
            }
            else
                nUsers++;
#endif
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
        #ifdef USR
        for (size_t i = 0; i < N-1; i++)
        {
            CHECK(sendto(sockfd,GROUPSTART,sizeof(GROUPSTART),0,(struct sockaddr *) &cs[i],sizeof(cs[i])));
        }
        #endif
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
#ifdef BIN
            /*
            length = (strlen(msg)*8) +1;
            char binary[length];
            stringToBinary(msg,binary);
            */
            if(strcmp(msg,STOP) == 0)
            {
                CHECK(sendto(sockfd,STOPBIN,nlus,0, (struct sockaddr *) &ss, addrlen));
                break;
            }
            CHECK(sendto(sockfd,msg,nlus,0, (struct sockaddr *) &ss, addrlen));
#elif FILEIO
            if(isTXT(msg))
                sendFile(msg,sockfd,ss);
            else
            {
                CHECK(sendto(sockfd,msg,nlus,0, (struct sockaddr *) &ss, addrlen));
                if(strcmp(msg,STOP) == 0)
                    break;
            }
#elif USR
            for (size_t i = 0; i < N-1; i++)
                rValue = sendto(sockfd,msg,nlus,0,(struct sockaddr *) &cs[i],sizeof(cs[i]));
            if(rValue == -1)
                CHECK(sendto(sockfd,msg,nlus,0,(struct sockaddr *) &ss,addrlen));
            if(strcmp(msg,STOP) == 0)
                break;           
#else
            CHECK(sendto(sockfd,msg,nlus,0, (struct sockaddr *) &ss, addrlen));
            if(strcmp(msg,STOP) == 0)
                break;
#endif
        }
        
        if(pfd[1].revents & POLLIN)
        {
            CHECK(nlus = recvfrom(sockfd,msg,SIZE,0,(struct sockaddr *) &ss, &addrlen));
            msg[nlus] = '\0';
#ifdef BIN
            /*
            length = (strlen(msg)-1)/8;
            char string[length];
            binaryToString(msg,string);
            */
            if(strcmp(msg,STOPBIN) == 0)
                break;
            printf("%s\n", msg);
#elif FILEIO
            if(isTXT(msg))
                receiveFile(msg,sockfd,ss);
            else
            {
                if(strcmp(msg,STOP) == 0)
                    break;
                printf("%s\n", msg);
            }
#elif USR
            for (size_t i = 0; i < N-1; i++)
                if (memcmp(&cs[i], &ss, sizeof(struct sockaddr_storage)) != 0)
                    sendto(sockfd,msg,nlus,0,(struct sockaddr *) &cs[i],sizeof(cs[i]));
            if(strcmp(msg,STOP) == 0)
                break;
            printf("%s\n", msg);
#else
            if(strcmp(msg,STOP) == 0)
                break;
            printf("%s\n", msg);
#endif
        }
    }
    

    /* close socket */

    CHECK(close(sockfd));

    /* free memory */

    return 0;
}