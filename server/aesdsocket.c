//opens a stream socket bound to port 9000
//listens and accepts a connection
//log message
//receives data over connection
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

#define output_file "/var/tmp/aesdsocketdata"
#define BUFFER_MAX 1024

int filefd, sockfd;

void signal_handler() {

	syslog(LOG_ERR, "Caught signal, exiting");

	close(filefd);
	close(sockfd);
	closelog();

	if(remove(output_file) < 0) {
		perror("Delete tmp file");
	}

	exit(0);
}


int main() {

	int port=9000;
	int new_sockfd, rc;
	struct sockaddr_in server_addr, new_addr;
	char buffer[BUFFER_MAX];
	struct sockaddr_in * pV4Addr = (struct sockaddr_in*)&new_addr;
	struct in_addr ipAddr = pV4Addr->sin_addr;
	char str[INET_ADDRSTRLEN];
	socklen_t addr_size;

	int read_byte=0, write_byte=0, recv_byte=0, send_byte=0;
	char *read_buffer = NULL;
	char *write_buffer = NULL;
	int rdbuff_size = 0;

	openlog(NULL, 0, LOG_USER);

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		perror("Socket creation");
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY; 

	rc = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(rc<0) {
		perror("Server bind");
		return -1;
	}

	rc = listen(sockfd, 3);
	if(rc<0) {
		perror("Server listen");
		return -1;
	}
	
		filefd = open(output_file, O_RDWR | O_CREAT, 0766);
	if(filefd < 0) {
		perror("Open file");
		return -1;
	}

	while(1) {

	addr_size = sizeof(new_addr);
	new_sockfd = accept(sockfd, (struct sockaddr*)&new_addr, &addr_size);
	if(new_sockfd<0) {
		perror("Server accept");
		return -1;
	}

	inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
	syslog(LOG_INFO, "Accepted connection from %s", str);

	read_buffer = (char *)malloc((sizeof(char))*BUFFER_MAX);
	if(read_buffer == NULL) {
		perror("Read Malloc failed");
		return -1;
	}

	do {
		recv_byte = recv(new_sockfd, buffer, sizeof(buffer), 0);

		rdbuff_size += recv_byte;
		strcpy(read_buffer, buffer);

	} while((recv_byte>0) || (strchr(buffer, '\n')==NULL));

	write_byte = write(filefd, read_buffer, strlen(read_buffer));
	if(write_byte != strlen(read_buffer)) {
		perror("File write");
		return -1;
	}

	write_buffer = (char *)malloc((sizeof(char))*rdbuff_size);
	if(write_buffer == NULL) {
		perror("Write malloc failed");
		return -1;
	}

	read_byte = read(filefd, write_buffer, rdbuff_size);
	if(read_byte != rdbuff_size) {
		perror("File read");
		return -1;
	}

	send_byte = send(new_sockfd, write_buffer, strlen(write_buffer), 0);
	if(send_byte != strlen(write_buffer)) {
		perror("Socket send");
		return -1;
	}

	free(read_buffer);
	free(write_buffer);

	close(new_sockfd);
	syslog(LOG_INFO, "Closed connection from %s", str);
	}
	return 0;
}
