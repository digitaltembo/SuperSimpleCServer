/*
 * This is a super small simplistic server that I built following the tutorial at
 * http://www.linuxhowtos.org/C_C++/socket.htm and with a few twists of my own.
 * What it does is take a directory and a port and serve files within that directory
 * at localhost:port_number
 * 
 * To compile, simply do gcc supersimpleserver.c
 * To run, simply do ./a.out [directory to serve] [port number]
 *      -The directory to serve is always the first argument,
 *       and defaults to the root directory
 *      -The port number is always the second argument,
 *       and defaults to 80, which depending on your system might need root access
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/stat.h>

//some helpful c string things, including a simple struct
//that holds the c string, its length, and its total capacity
typedef struct mystring{
    char* c;
    unsigned len;
    unsigned cap;
} String;


//returns length of c string
int len(const char* c);
//makes c string lowercase 
void toLower(char* c);
//makes a new String*, either a copy of the inputted string
//or a new one, with a capacity of init_capacity
char* makeCString(const char* c);
String* makeString(const char* c, int init_capacity);
//clears a string
void clear(String* s);
//expands the capacity by a factor of two
int expand(String* s);
//adds a character to the String*
void add(String* s, char c);
//adds a string to the String*
void addCString(String* s, char* c);
void addString(String* s, String* s2);
//returns the c string of a String*
char* str(String* s);

//very dumbly guesses the MIME type of a path based on extension
//Only a few extensions are given specific MIME type: 
//      General web files: .html, .htm, .css, .js, .json
//      Image files: .gif, .jpg, .jpeg, .png, .tiff
//      Font files: .eof, .ttf, .otf, .svg, .woff
//All other files are given the mimetype application/unknown
char* getMIMEType(String* s);

//This is the function that does the brunt of the labor
//The file descriptor sockfd that is passed in can be read
//(reading the request from the client and anything else the client wants to say)
//or written (responding both with the header and the content of a given thing
//baseDirPath is of the base folder from which to serve from (others will be able
//to access all the files in that directory, I think? Or at leaast all the ones that
//program has read permissions on
void process(int sockfd, const char* baseDirPath);

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[1024];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
//   serve everything by default (suuuper secure)
     const char* baseDirPath=(argc<2)?"/":argv[1];
//   serve on 80 by default, also suuper secure
     portno=(argc<3)?80:atoi(argv[2]);
     
//   Opens a socket into the internet! Using magic and system calls
//   sockfd is a file desccriptor, the old-timey c way of interacting with
//   files and stuff. You could make it into a FILE* with a fdopen call, I think
//   or you could just interact with it with the old timey read and write calls
     
//   AF_INET means it is an internet socket, as opposed
//   to a unix socket or some such magic
//   SOCK_STREAM means you treat it as a stream as opposed 
//   to like packets or datagrams
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     
//   bzero is a function i didn't know, simply zeros a given memory space
     bzero((char *) &serv_addr, sizeof(serv_addr));
//   serv_addr describes the full address of the server: address family
     serv_addr.sin_family = AF_INET;
//   ip address
     serv_addr.sin_addr.s_addr = INADDR_ANY;
//   ip port (htons is h to ns, or host to network system, and converts from
//   host byte order to network byte order)
     serv_addr.sin_port = htons(portno);
     
//   bind binds the address to the socket
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
//   This ignores all the zombie children's deaths, so they don't all have to
//   be reaped by the main process
     signal(SIGCHLD,SIG_IGN);
     while(1){
//      blocks until a connection is attempted (5 is max number of connections
//      within the queue, I think?
        listen(sockfd,5);
        clilen = sizeof(cli_addr);
//      accepts the client, reading in the client address
        newsockfd = accept(sockfd, 
                    (struct sockaddr *) &cli_addr, 
                    &clilen);
        pid_t parent;
//      if fork succeeds
        if((parent=fork())>=0){
            if(parent){
//              then the parent closes the client connection
                close(newsockfd);
            }else{
//              and the child closes the general socket, so no more connections
//              to it attempted
                close(sockfd);
//              And it calls process on the newsockfd, which is a file descriptor
//              that will be written to and read from for communication from and to
//              the client
                process(newsockfd, baseDirPath);
//              Then, there is an exit which will not have to be reaped by the parent
//              thanks to the previous SIG_IGN placed on SIGCHLD
                exit(0);
            }
        }
     }
}


void process(int sockfd, const char* baseDirPath){
    char c;
    String* s=makeString(NULL, 25);
    
//  The first word reda from the sockfd is the request type
//  GET, POST, etc. are request types
    while(read(sockfd, &c, 1) && c!=' ')
        add(s, c);
    printf("Request Type: %s\n",str(s));
    clear(s);
    
//  the second word read from the sockfd is the address requested
    while(read(sockfd, &c, 1) && c!=' ')
        add(s, c);
    printf("Request Address: %s\n", str(s));
    
//  what follows in sockfd is a bunch of probably important data from the client
//  It probably starts with the connection type, but I'm just going to assume it's
//  HTTP/1.1, and then it goes into a bunch of MIME style details about the request;
//  like user-agent and host and connection
    
//  but I don't really care about that stuff, so we're just going to read it
//  (reading it is good for the client, I think, it makes them think we care
    char buf[512];
    while(read(sockfd, buf, 512)==512);
    String* filePath=makeString(baseDirPath,0);
    addString(filePath, s);
    struct stat fileInfo;
    
//  stat is a posix system call that gets file information
//  on success, it returns 0, on failure, it returns -1
//  see man 2 stat
    if(stat(str(filePath), &fileInfo)==-1){
        write(sockfd, "HTTP/1.1 404 Not Found\n\n",24);
        close(sockfd);
        return;
    }
//  makes sure the file is a regular file as opposed to a directory
//  or link or something (otherwise, throw a 404 error
    if(!S_ISREG(fileInfo.st_mode)){
        write(sockfd, "HTTP/1.1 404 Not Found\n\n",24);
        close(sockfd);
        return;
    }
    FILE* file=fopen(str(filePath),"r");
//  go ahead and do the same thing if it can't open, though it probably can?
    
//  I probably could have done this at the start, but I already needed the stat
//  to find the length of the file, and this way special treatment can be given
//  to directories
    if(!file){
        write(sockfd, "HTTP/1.1 404 Not Found\n\n",24);
        close(sockfd);
        return;
    }
//  this very dumbly guesses the mime type based on extension
    char* mimeType=getMIMEType(filePath);
//  this is the header to the response. It appears that my browsers find this
//  header sufficient, but there is a lot fo other information that could be encoded
    sprintf(buf, "HTTP/1.1 200 OK\n"
                    "Content-Length: %i\n"
                    "Connection: close\n"
                    "Content-Type: %s\n\n",
//          remember, fileInfo holds a lot of file information, including file size
            (int)fileInfo.st_size,
            mimeType);
    free(mimeType);
    
    write(sockfd, buf, len(buf));
        
    int r;            
//  copies the file onto sockfd
    while((r=fread(buf, 1, 512, file))!=0){
        write(sockfd, buf, r);
//      not entirely sure why this is necessary
        bzero(buf, 512);
    }
    fclose(file);
//  closes the passed in connection
    close(sockfd);
}


char* getMIMEType(String* s){
    char* c=NULL;
    int i;
//  points c to the extension
    for(i=s->len-1;i>0&&i>s->len-10;i--){
        if(str(s)[i]=='.'){
            c=str(s)+i+1;
            break;
        }
    }
    toLower(c);
    if(!c)
        return makeCString("application/unknown");
    switch(c[0]){
       case 'c':
            if(c[1]=='s'&&c[2]=='s'&&c[3]=='\0')
                return makeCString("text/css");
            break;
       case 'e':
           if(c[1]=='o'&&c[2]=='f'&&c[3]=='\0')
               return makeCString("application/vnd.ms-fontobject");
           break;
       case 'g':
           if(c[1]=='i'&&c[2]=='f'&&c[3]=='\0')
               return makeCString("image/gif");
       case 'h':
            if(c[1]=='t'&&c[2]=='m'&&(c[3]=='\0'||(c[4]=='l'&&c[5]=='\0')))
                return makeCString("text/html");
            break;
       case 'j':
            if(c[1]=='s'){
                if(c[2]=='\0')
                    return makeCString("application/javascript");
                if(c[2]=='o')
                    return makeCString("application/json");
            }else if(c[1]=='p'){
                if((c[2]=='g'&&c[3]=='\0')||(c[3]=='e'&&c[4]=='g'&&c[5]=='\0'))
                    return makeCString("image/jpeg");
            }
            break;
       case 'o':
           if(c[1]=='t'&&c[2]=='f'&&c[3]=='\0')
               return makeCString("application/x-font-opentype");
           break;
       case 'p':
           if(c[1]=='n'&&c[2]=='g'&&c[3]=='\0')
               return makeCString("image/png");
           break;
       case 's':
           if(c[1]=='v'&&c[2]=='g'&&c[3]=='\0')
               return makeCString("image/svg+xml");
           break;
       case 't':
           if(c[1]=='i'&&c[2]=='f'&&c[3]=='f'&&c[4]=='\0')
               return makeCString("image/tiff");
           if(c[1]=='t'&&c[2]=='f'&&c[3]=='\0')
               return makeCString("application/x-font-ttf");
           if(c[1]=='x'&&c[2]=='t'&&c[3]=='\0')
               return makeCString("text/plain");
           break;
       case 'w':
           if(c[1]=='o'&&c[2]=='f'&&c[3]=='f'&&c[4]=='\0')
               return makeCString("application/font-woff");
           break;
           
    }
    return makeCString("application/unknown");
}


//so these are just like some c string and mystring functions
//mystrings are just a thing I threw together because I wanted this in
//pure c and I also wanted to hold the length and capacities of the strings
//for easy inexpensive expansion

//len returns the length of a c string
int len(const char* c){
    const char* s=c;
    while(*s) ++s;
    return (s-c);
}

//toLower converts a c string in place to lowercase
void toLower(char* c){
    while(*c!='\0'){
        if(*c>='A'&&*c<='Z')
            *c=*c+32;
        c++;
    }
}

//This returns a pointer to a copy of the inputted c string
char* makeCString(const char* c){
    int l=len(c)+1;
    char* nc=(char*)malloc(l*sizeof(char));
    int i;
    for(i=0;i<l;i++){
        nc[i]=c[i];
    }
    return nc;
}

//makes a String* from an optional char* and capacity
//capacity can be <=0 (in which it is computed)
//  or any number greater than the size of the char*
//the char* can be passed in as NULL, in which the string will be empty
//  or can be a pointer to any string, and that will be copied
String* makeString(const char* c, int init_capacity){
    String* s=(String*)malloc(sizeof(String));
    if(c==NULL){
        
        if(init_capacity<=0)
            init_capacity=5;
        s->c=(char*)malloc(init_capacity*sizeof(char));
        s->len=1;
        s->c[0]='\0';
    }
    else{
        s->len=len(c)+1;
        if(init_capacity<=0||init_capacity<s->len)
            init_capacity=s->len*2;
        s->c=(char*)malloc(init_capacity*sizeof(char));
        int i;
        for(i=0;i<s->len;i++)
            s->c[i]=c[i];
    }
    s->cap=init_capacity;
    return s;
}

//clears a String* (so it has no length)
void clear(String* s){
    s->len=1;
    s->c[0]='\0';
}

//expands a String* by a factor of two, returns 1 on success, 0 on failure
int expand(String* s){
    char* tmp=(char*)malloc(s->cap*2*sizeof(char));
    if(!tmp)
        return 0;
    int i;
    for(i=0;i<s->len;i++)
        tmp[i]=s->c[i];
    free(s->c);
    s->c=tmp;
    return 1;
}

//appends a character to a String*
void add(String* s, char c){
    if(s->len >= s->cap+1)
        if(!expand(s))
            error("Failed Expansion");
    s->c[s->len-1]=c;
    s->c[s->len]='\0';
    s->len++;
}

//appends a c string to a String*
void addCString(String* s, char* c){
    int l=len(c);
    while(s->len+l > s->cap+1)
        if(!expand(s))
            error("Failed Expansion\n");
    int i;
    for(i=0;i<l;i++)
        s->c[s->len+i-1]=c[i];
    s->c[s->len+l-1]='\0';
    s->len+=l;
}
//appends a String* (s2) to a String* (s) [s+=s2]
void addString(String* s, String* s2){
    int l=s2->len;
    while(s->len+l > s->cap+1)
        if(!expand(s))
            error("Failed Expansion\n");
    int i;
    for(i=0;i<l;i++)
        s->c[s->len+i-1]=s2->c[i];
    s->c[s->len+l-1]='\0';
    s->len+=l;
}

//returns the c string at the base of the String*
char* str(String* s){
    return s->c;
}
