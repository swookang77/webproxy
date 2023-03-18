#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr =
//     "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
//     "Chrome/109.0.0.0 Safari/537.36\r\n";
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void request(char* buf);
int main(int argc, char **argv) {
  printf("main호출\n");
  int listenfd, connfd ; 
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

   /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  printf("듣기소켓오픈\n");
  while (1) {
    rio_t rio; 
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    Rio_readinitb(&rio,connfd); 
    Rio_readlineb(&rio, buf, MAXLINE); 
    printf("%s",buf);
    request(buf);
    // Close(connfd);  
  }
  return 0;
}

void request(char* buf)
{
  printf("request함수호출");
  int clientfd;
  char *serverhost,*serverport;
  rio_t rio;
  serverhost="3.39.238.173";
  serverport="3000";
  clientfd = Open_clientfd(serverhost,serverport);
  Rio_readinitb(&rio,clientfd);
  Rio_writen(clientfd,buf,strlen(buf)); //요청 라인 보내기 

  sprintf(buf, "Host: 3.39.238.173\r\n");
  sprintf(buf, "%sConnection: close\r\n",buf);
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n",buf);
  Rio_writen(clientfd, buf, strlen(buf));
}
