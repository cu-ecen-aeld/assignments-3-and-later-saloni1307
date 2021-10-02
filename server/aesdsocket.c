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

	printf("Caught signal, exiting\n");
	syslog(LOG_INFO, "Caught signal, exiting");

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
	char buffer[BUFFER_MAX]; //memset to null
	struct sockaddr_in * pV4Addr = (struct sockaddr_in*)&new_addr;
	struct in_addr ipAddr = pV4Addr->sin_addr;
	char str[INET_ADDRSTRLEN];
	socklen_t addr_size;

	memset(buffer, '\0', BUFFER_MAX);

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
	
		filefd = open(output_file, O_RDWR | O_CREAT, 0666);
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
	printf("Accepted connection from %s\n", str);
	syslog(LOG_INFO, "Accepted connection from %s", str);

	int write_byte=0, recv_byte=0, send_byte=0, read_byte=0;
	char *read_buffer = NULL;
	char *write_buffer = NULL;
	int rdbuff_size = 0, wrbuff_size=0;
	int malloc_size = BUFFER_MAX; 

	read_buffer = (char *)malloc((sizeof(char))*BUFFER_MAX);
	memset(read_buffer, '\0', BUFFER_MAX);
	if(read_buffer == NULL) {
		perror("Read Malloc failed");
		return -1;
	}

	do {
		recv_byte = recv(new_sockfd, buffer, sizeof(buffer), 0);

		if((malloc_size - rdbuff_size) < recv_byte) {
			int available_size = (malloc_size - rdbuff_size);
			read_buffer = (char *)realloc(read_buffer, sizeof(char)*(recv_byte-available_size));
		}

		memcpy(&read_buffer[rdbuff_size], buffer, recv_byte);
		rdbuff_size += recv_byte;

	} while((recv_byte>0) || (strchr(buffer, '\n')==NULL));

	printf("Read buffer= %s\n", read_buffer);
	write_byte = write(filefd, read_buffer, rdbuff_size);
	if(write_byte != rdbuff_size) {
		perror("File write");
		return -1;
	}

	write_buffer = (char *)malloc((sizeof(char))*rdbuff_size);
	memset(&write_buffer[wrbuff_size], '\0', rdbuff_size);
	if(write_buffer == NULL) {
		perror("Write malloc failed");
		return -1;
	}

	printf("rdbuff_size = %d\n", rdbuff_size);
	lseek(filefd, 0, SEEK_SET);
	read_byte = read(filefd, write_buffer, rdbuff_size);
	wrbuff_size += rdbuff_size;
	printf("read_byte= %d\n", read_byte);
	if(read_byte != rdbuff_size) {
		perror("File read");
		return -1;
	}
         
	printf("write buffer= %s\n", write_buffer);
	send_byte = send(new_sockfd, write_buffer, rdbuff_size, 0);
	printf("send_byte= %d\n", send_byte);
	if(send_byte != rdbuff_size) {
		perror("Socket send");
		return -1;
	}

	free(read_buffer);
	free(write_buffer);

	close(new_sockfd);
	printf("Closed connection from %s\n", str);
	syslog(LOG_INFO, "Closed connection from %s", str);
	}
	return 0;
}
