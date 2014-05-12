#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>


#define MAXPATH 150
#define MAXBUF 1024
#define DEFAULTIP "127.0.0.1"
#define DEFAULTPORT "80"
#define DEFAULTBACK "10"
#define DEFAULTDIR "/home"
#define DEFAULTLPG "/tmp/das-server.log"


char buffer[MAXBUF+1];
char *host=0;
char *port=0;
char *back=0;
char *dirroot=0;
char *logdir=0;
unsigned char daemon_y_n=0;
FILE *logfp;


void prterrmsg(const char *msg);
#define prterrmsg(msg) {perror(msg);abort();}
void wrterrmsg(char *msg);
#define wrterrmsg(msg) {fputs(msg,logfp);fputs(strerror(errno),logfp);fflush(logfp);abort();}

char *dir_up(char *dirpath)
{
    int len;
    static char Path[MAXPATH]={ 0 };
    strncpy(Path, dirpath, strlen(dirpath));
    len=strlen(Path);
    while(Path[len-1]=='/'&&len>1)
        len--;
    while(Path[len-1]!='/'&&len>1)
        len--;
    Path[len]=0;
    return Path;
}

void AllocateMemory(char **s,int l,char *d)
{
    *s=malloc(l+1);
    bzero(*s, l+1);
    memcpy(*s,d,l);
}

void GiveResponse(FILE* client_sock,char* path)
{
    int fd,len,ret;
    char *p,*realPath,*realFilename,*nport;
    struct stat info;
    struct dirent *dirent;
    DIR *dir;
    char fileName[MAXPATH]= {0};
    /*
     * get the current path
     */
    len=strlen(dirroot)+strlen(path)+1;
    realPath=malloc(len+1);
    bzero(realPath, len+1);
    sprintf(realPath, "%s/%s",dirroot,path);

    /*
     * get real port
     */
    len=strlen(port)+1;
    nport=malloc(len+1);
    bzero(nport,len+1);
    sprintf(nport,":%s",port);

    /* fail to get the info of this path  */
    if(stat(realPath, &info))
    {
        fprintf(client_sock, "HTTP/1.1 200 OK\r\nServer: DAS by TangYuhang\r\n \
                Connection: close\r\n\r\n<html><head><title>%d-%s</title></head> \
                <body><font size=+4>Linux DAS server</font><br><hr width=\"100%%\"><br> \
                <center><table border cols=3 width=\"100%%\">",
                errno,strerror(errno));
        fprintf(client_sock,"</table><font color=\"cc0000\" size=+2>Please connect to admin \
               \n %s%s</font></body></html>",path,strerror(errno));
        free(realPath);
        free(nport);
        return;
    }
    /* download file  */
    if(S_ISREG(info.st_mode))
    {
        fd=open(realPath, O_RDONLY); 
        len=lseek(fd, 0, SEEK_END);
        p=malloc(len+1);
        bzero(p, len+1);
        lseek(fd,0,SEEK_SET);
        read(fd, p, len);
        close(fd);
        fprintf(client_sock, "HTTP/1.1 200 OK\r\nServer: DAS by TangYuhang\r\n \
                Connection:keep-alive\r\nContent-type: application/*\r\n \
                Content-Length:%d\r\n\r\n", len ); 
        fwrite(p, len+1, 1, client_sock);
        free(p);
    }
    /* dir  */
    else if(S_ISDIR(info.st_mode))
    {
        dir=opendir(realPath);    
        /**   
        fprintf(client_sock, "HTTP/1.1 200 OK\r\n \
                Server:DAS by TangYuhang\r\nConnection: close\r\n\r\n \
                <html><head><title>%s</title></head> \
                <body><font size=+4>Linux DAS server</font><br><hr width=\"100%%\"><br> \
                <center><table border cols=3 width=\"100%%\">",path);
         **/
        
        
        fprintf(client_sock, "HTTP/1.1 200 OK\r\n\r\n \
                <html><head><title>%s</title></head> \
                <body><font size=+4>Linux DAS server</font><br><hr width=\"100%%\"><br> \
                <center><table border cols=3 width=\"100%%\">",path);
        
        fprintf(client_sock,"<caption><font size=+3>DIR %s</font></caption>\n",path);
        fprintf(client_sock,"<tr><td>name</td><td>size</td><td>modify time</td></tr>\n");
        if(dir==0)
        {
            fprintf(client_sock,"</table><font color=\"cc0000\" size=+2>%s \
                   </font></body></html> ",strerror(errno));
            return;
        }
        /* read the dir  */
        while((dirent=readdir(dir))!=0)
        {
           if(strcmp(path, "/")==0)
               sprintf(fileName, "/%s",dirent->d_name);
           else
               sprintf(fileName,"%s/%s",path,dirent->d_name);
           fprintf(client_sock,"<tr>");
           len=strlen(dirroot)+strlen(fileName)+1;
           realFilename=malloc(len+1);
           bzero(realFilename,len+1);
           sprintf(realFilename, "%s/%s",dirroot,fileName);
           if(stat(realFilename, &info)==0)
           {
               if(strcmp(dirent->d_name,"..")==0)
                   fprintf(client_sock,"<td><a href=\"http://%s%s%s\">(parent)</a></td>",
                           host,atoi(port)==80?"":nport,dir_up(path));
               else
                   fprintf(client_sock, "<td><a href=\"http://%s%s%s\">%s</a></td>",
                           host,atoi(port)==80?"":nport,fileName,dirent->d_name);
               if(S_ISDIR(info.st_mode))
                   fprintf(client_sock,"<td>dir</td>");
               else if(S_ISREG(info.st_mode))
                   fprintf(client_sock,"<td>%d</td>",(int)info.st_size);
               else if(S_ISLNK(info.st_mode))
                   fprintf(client_sock,"<td>link</td>");
               else
                   fprintf(client_sock,"<td>others</td>");
           }
           fprintf(client_sock,"</tr>\n");
           free(realFilename);
        }
        fprintf(client_sock,"</table></center></body></html>");
    }
    free(realPath);
    free(nport);
}

void getoption(int argc,char **argv)
{
    int c,len;
    char *p=0;
    opterr=0;
    while(1){
        int option_index=0;
        static struct option long_options[]={
            {
                "host",1,0,0
            },
            {
                "port",1,0,0
            },
            {
                "back",1,0,0
            },
            {
                "dir",1,0,0
            },
            {
                "log",1,0,0
            },
            {
                "daemon",0,0,0
            },
            {
                0,0,0,0
            }
        };
        c=getopt_long(argc, argv, "H:P:B:D:L", long_options, &option_index);
        if(c==-1 || c=='?')
            break;
        if(optarg)
            len=strlen(optarg);
        else
            len=0;

        if((!c && !(strcasecmp(long_options[option_index].name, "host"))) || c=='H')
            p=host=malloc(len+1);
        else if((!c && !(strcasecmp(long_options[option_index].name, "port"))) || c=='P')
            p=port=malloc(len+1);
        else if((!c && !(strcasecmp(long_options[option_index].name, "back"))) || c=='B')
            p=back=malloc(len+1);
        else if((!c && !(strcasecmp(long_options[option_index].name, "dir"))) || c=='D')
            p=dirroot=malloc(len+1);
        else if((!c && !(strcasecmp(long_options[option_index].name, "log"))) || c=='L')
            p=logdir=malloc(len+1);
        else if((!c && !(strcasecmp(long_options[option_index].name, "daemon"))))
        {
            daemon_y_n=1;
            continue;
        }
        else
            break;
        bzero(p,len+1);
        memcpy(p,optarg,len);
    }
}

static void sig_chld(int signo)
{
    pid_t pid;
    int stat;
    while((pid=waitpid(-1, &stat, WNOHANG))>0)
        ;
    return;
}

int main(int argc,char **argv)
{
    struct sockaddr_in addr;
    int sock_fd,addrlen;
    getoption(argc,argv);
    if(!host){
        addrlen=strlen(DEFAULTIP);
        AllocateMemory(&host,addrlen,DEFAULTIP);
    }
    if(!port){
        addrlen=strlen(DEFAULTPORT);
        AllocateMemory(&port,addrlen,DEFAULTPORT);
    }
    if(!back){
        addrlen=strlen(DEFAULTBACK);
        AllocateMemory(&back,addrlen,DEFAULTBACK);
    }
    if(!dirroot){
        addrlen=strlen(DEFAULTDIR);
        AllocateMemory(&dirroot,addrlen,DEFAULTDIR);
    }
    if(!logdir){
        addrlen=strlen(DEFAULTLPG);
        AllocateMemory(&logdir,addrlen,DEFAULTLPG);
    }
    if(daemon_y_n)
    {
        if(fork())
            exit(0);
        if(fork())
            exit(0);
        close(0),close(1),close(2);
        logfp=fopen(logdir,"a+");
        if(!logfp)
            exit(0);
    }
    signal(SIGCHLD, sig_chld);
    sock_fd=socket(AF_INET, SOCK_STREAM, 0);
    addrlen=1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &addrlen, sizeof(addrlen));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(atoi(port));
    addr.sin_addr.s_addr=inet_addr(host);
    addrlen=sizeof(struct sockaddr_in);

    bind(sock_fd, (struct sockaddr*)&addr, addrlen);
    listen(sock_fd, atoi(back));
    while(1)
    {
        int len;
        socklen_t socklen;
        int new_fd;
        socklen=sizeof(struct sockaddr_in);
        new_fd=accept(sock_fd, (struct sockaddr*)&addr, &socklen);
        bzero(buffer, sizeof(buffer));
        sprintf(buffer, "connect from: %s:%d\n",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));
        printf("%s\n",buffer);
        if(!fork())
        {
            close(sock_fd);
            bzero(buffer,sizeof(buffer));
            if((len=recv(new_fd, buffer, MAXBUF, 0))>0)
            {
                FILE* clientFP=fdopen(new_fd, "w");
                char req[MAXPATH+1]="";
                sscanf(buffer, "GET %s HTTP",req);
                bzero(buffer,sizeof(buffer));
                GiveResponse(clientFP,req);
                fclose(clientFP);
            }
            exit(0);
        }
        close(new_fd);
    }
    close(sock_fd);
    return 0;
}
