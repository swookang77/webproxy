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
  printf("듣기 소켓 오픈\n");
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // handles one HTTP transaction
    Close(connfd);  
    printf("connfd 닫음\n");
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
  printf("요청라인:%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  printf("Request headers:\n");
  read_requesthdrs(&rio); 

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
  while(strcmp(buf, "\r\n")) {   //buf 와 \r\n이 같으면 0.
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
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

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));

  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // 요청한 파일의 식별자를 얻음
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //요청한 파일을 가상메모리에 매핑
  srcp = (char *)Malloc(filesize);
  Rio_readn(srcfd,srcp,filesize);
  Close(srcfd);  //매핑했으니 이제 식별자 필요없음  
  Rio_writen(fd, srcp, filesize); // 주소 srcp에서 시작하는 filesize byte를 클라이언트의 연결식별자로 복사. (파일 전송)
  // Munmap(srcp, filesize); //매핑된 가상메모리 영역을 free 
  free(srcp);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { //자식 프로세스 fork (자식 프로세스의 context에서 cgi 프로그램을 돌린다)
    setenv("QUERY_STRING", cgiargs, 1);  // QUERY_STRING 환경변수를 받은 인자로 초기화
    Dup2(fd, STDOUT_FILENO); // 자식의 표준 출력을 연결 파일 식별자로 redirect.
    Execve(filename, emptylist, environ); // cgi 프로그램을 로드하고 실행. 
    //everything that the CGI program writes to standard output goes directly to the client process
  }
  Wait(NULL); 
}