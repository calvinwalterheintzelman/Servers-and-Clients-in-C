#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

//use port 2041
#define MAXLINE 1024

int open_listenfd(int port) { 
	int listenfd, optval=1; 
	struct sockaddr_in serveraddr; 
	/* Create a socket descriptor */ 
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		return -1; 

	/* Eliminates "Address already in use" error from bind. */ 
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,  
		 (const void *)&optval , sizeof(int)) < 0) 
		return -1;

	/* Listenfd will be an endpoint for all requests to port 
	on any IP address for this host */ 
	bzero((char *) &serveraddr, sizeof(serveraddr)); 
	serveraddr.sin_family = AF_INET;  
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);  
	serveraddr.sin_port = htons((unsigned short)port);  
	if (bind(listenfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		return -1;
	}

	/* Make it a listening socket ready to accept 
	connection requests */ 
	if (listen(listenfd, MAXLINE) < 0) {
		return -1;
	}
	return listenfd; 
}

int main(int argc, char **argv) {
	int listenfd, connfd, port, clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *haddrp;
	port = atoi(argv[1]); /* the server listens on a port passed on the command line */
	char buf[MAXLINE + 1] = {'\0'};
	size_t n;
	char* http_okay = "HTTP/1.0 200 OK\r\n\r\n";
	char* http_not_found = "HTTP/1.0 404 Not Found\r\n\r\n";
	char* http_forbidden = "HTTP/1.0 403 Forbidden\r\n\r\n";
	char* response;	

	listenfd = open_listenfd(port); 
	while (1) {

		clientlen = sizeof(clientaddr); 
		connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
		hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
			       sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		haddrp = inet_ntoa(clientaddr.sin_addr);

		//write(0, hi, 4); //0 is for stdout
		read(connfd, buf, MAXLINE);
		char *path_start = strtok(buf, " "); //GET
		path_start = strtok(NULL, " "); //path
		int cipher_num = atoi(strtok(NULL, " ")); //number

		FILE *fp = fopen(path_start, "r");
		if(fp == NULL) {
			if(errno == 2) {
				response = http_not_found;
			} else {
				response = http_forbidden;
			}
			write(connfd, response, strlen(response));
		} else {
			response = http_okay;
			write(connfd, response, strlen(response));
			int c = getc(fp); 
			while(c != EOF) {
				if(c >= 65 && c <= 90) {
					c = c - cipher_num;
					if(c < 65) {
						c += 26;
					}
				} else if(c >= 97 && c <= 122) {
					c = c - cipher_num;
					if(c < 97) {
						c += 26;
					}
				}
				write(connfd, &c, 1);
				c = getc(fp);
			}

			fclose(fp);
		}

		close(connfd);

	}
	return 0;
}
