/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // handles one HTTP transaction
    Close(connfd);  
  }
}
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio; 

  Rio_readinitb(&rio,fd); 
  Rio_readlineb(&rio, buf, MAXLINE); // Rio_readlineb함수를 사용해서 요청 라인을 읽어들임
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); //이제 다른 요청 헤더들을 무시

  is_static = parse_uri(uri,filename,cgiargs);

  if (stat(filename, &sbuf) < 0) { //파일이 디스크 상에 없을 경우
    clienterror(fd, filename, "404", "Not found","Tiny couldn’t find this file");
    return;
  }
  
  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //보통파일인지? 이 파일을 읽을 권한을 가지고 있는지?
      clienterror(fd, filename, "403", "Forbidden","Tiny couldn’t read the file");
      return;
    }
    serve_static(fd,filename,sbuf.st_size); //정적 컨텐츠 제공
  }
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 보통파일인지? 실행가능한지? 
      clienterror(fd, filename, "403", "Forbidden","Tiny couldn’t run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);  //동적 컨텐츠 제공
  }
}

void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg)
{
  char buf[MAXLINE] , body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  Rio_writen(fd, body, strlen(body));

}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {   //요청헤더를 종료하는 줄은 \r\n, 이 부분이랑 연관해서 이 함수가 왜 요청헤더를 무시하게 되는지 고민해보자
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    printf("read_requesthdrs while문 안");
  }
  printf("read_requesthdrs while문 밖");
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { //uri가 cgi-bin을 포함하고 있지 않으면 static content
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else {  /* Dynamic content */
    ptr = index(uri,'?');
    if (ptr) {
      strcpy(cgiargs, ptr+1); 
      *ptr = '\0'; //이거 왜 함 ? 
    }
    else
      strcpy(cgiargs,"");
    strcpy(filename,".");
    strcat(filename,uri);
    return 0;
  }
}