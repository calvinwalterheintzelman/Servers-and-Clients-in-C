#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#define MAXLINE 1024

// class ID is 41

int open_clientfd(char* servername, int port) {
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;

	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
	} // checks for errors

	if ((hp = gethostbyname(servername)) == NULL) {
		return -2; // checks for errors
	}
	
	bzero((char*) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr, (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	// finally, establish connection
	if (connect(clientfd, (const struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) {
		return -1;
	}

	return clientfd;
}


int main(int argc, char** argv) {
	char *servername = argv[1];
	int port = atoi(argv[2]);
	char *pathname = argv[3];
	int clientfd;
	char* get_c = "GET ";
	char* http_c = " HTTP/1.0\r\n\r\n";
	char buf[MAXLINE] = {'\0'};
	strcpy(buf, get_c);
	strcat(buf, pathname);
	strcat(buf, http_c);
	
	clientfd = open_clientfd(servername, port);

	write(clientfd, buf, strlen(buf));
	read(clientfd, buf, MAXLINE);
	fputs(buf, stdout);

	if(buf[9] == '2' && buf[10] == '0' && buf[11] == '0') {
		int char_counter = 0;
		int newl_counter = 0;
		while(newl_counter < 11) {
			if(buf[char_counter] == '\n') {
				newl_counter++;
			}
			char_counter++;
		}

		char new_file_name[MAXLINE] = {'\0'};
		int file_char_counter = 0;
		while(1) {
			if(buf[char_counter] == '\n') {
				break;
			}
			new_file_name[file_char_counter] = buf[char_counter];
			file_char_counter++;
			char_counter++;
		}
		
		//now new_file_name has the correct path
		char new_buf[MAXLINE + 1] = {'\0'};
		strcpy(new_buf, get_c);
		strcat(new_buf, new_file_name);
		strcat(new_buf, http_c);

		clientfd = open_clientfd(servername, port);
		
		write(clientfd, new_buf, strlen(new_buf));
		int check_read = read(clientfd, new_buf, MAXLINE);
		while(check_read != 0) {
			char print_buf[MAXLINE + 1];
			strcpy(print_buf, new_buf);
			print_buf[check_read] = '\0';
			fputs(print_buf, stdout);
			check_read = read(clientfd, new_buf, MAXLINE);
		}

	}

	
	close(clientfd);
	exit(0);
}
