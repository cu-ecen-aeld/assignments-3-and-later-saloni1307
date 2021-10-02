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
#include <sys/stat.h>
#include <sys/socket.h>
#include <syslog.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <linux/fs.h>

#define output_file "/var/tmp/aesdsocketdata"
#define BUFFER_MAX 100
#define MAX_CONNECTS 10
#define MAX_LINES 4096

int filefd, sockfd, new_sockfd;

void signal_handler() {

	syslog(LOG_INFO, "Caught signal, exiting");

	
	close(filefd);
	close(sockfd);
	closelog();


	if(remove(output_file) < 0) {
		perror("Delete tmp file");
		exit(-1);
	}
	exit(0);
}


int main(int argc, char *argv[]) {

	int port=9000;
	int rc;
	struct sockaddr_in server_addr, new_addr;
	char buffer[BUFFER_MAX]; //memset to null
	struct sockaddr_in * pV4Addr = (struct sockaddr_in*)&new_addr;
	struct in_addr ipAddr = pV4Addr->sin_addr;
	char str[INET_ADDRSTRLEN];
	socklen_t addr_size;
	int daemon=0;
	long lines[MAX_LINES];
	long line_number=0;
	
	if(argc == 1) {
		daemon=0;
	}
	else if(argc > 2) {
		perror("Invalid arguments");
		return -1;
	}
	else if(argc == 2) {
		if(strcmp(argv[1], "-d")==0)
			daemon=1;
	}

	memset(buffer, '\0', BUFFER_MAX);

	openlog(NULL, 0, LOG_USER);

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		perror("Socket creation");
		return -1;
	}

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))<0) {
		perror("error in setsockopt");
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

	if(daemon == 1 ) {
		pid_t pid;
		printf("Run as daemon\n\r");
		signal(SIGCHLD, SIG_IGN);
	        signal(SIGHUP, SIG_IGN);
		pid = fork();
		if(pid == -1)
			return -1;
		else if(pid != 0)
			exit(EXIT_SUCCESS);

		umask(0);

		if(setsid() == -1)
			return -1;

		if(chdir("/") == -1)
			return -1;

	//	for(i=sysconf(_SC_OPEN_MAX); i >= 0; i--)
	//		close(i);

		open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
	}


	rc = listen(sockfd, MAX_CONNECTS);
	if(rc<0) {
		perror("Server listen");
		return -1;
	}
	
		filefd = open(output_file, O_RDWR | O_CREAT, 0644);
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

	long write_byte=0, recv_byte=0, send_byte=0, read_byte=0;
	char *read_buffer = NULL;
	char *write_buffer = NULL;
	long rdbuff_size = 0;
	long malloc_size = BUFFER_MAX;
	int break_loop=0;

	read_buffer = (char *)malloc(sizeof(char)*BUFFER_MAX);
	if(read_buffer == NULL) {
		perror("Read Malloc failed");
		return -1;
	}

	do {
		recv_byte = recv(new_sockfd, buffer, sizeof(buffer), 0);

		if(!recv_byte || (strchr(buffer, '\n')!=NULL))
			break_loop=1;

		if((malloc_size - rdbuff_size) < recv_byte) {
				malloc_size += recv_byte;

			read_buffer = (char *)realloc(read_buffer, sizeof(char)*malloc_size);
		}

		memcpy(&read_buffer[rdbuff_size], buffer, recv_byte);
		rdbuff_size += recv_byte;

	} while(break_loop != 1);

	lines[line_number] = rdbuff_size;
	line_number += 1;
	write_byte = write(filefd, read_buffer, rdbuff_size);
	if(write_byte != rdbuff_size) {
		perror("File write");
		return -1;
	}

	lseek(filefd, 0, SEEK_SET);

	for(long i=0; i<line_number; i++) {

		write_buffer = (char *)malloc(sizeof(char)*lines[i]);
	if(write_buffer == NULL) {
		perror("Write malloc failed");
		return -1;
	}
		read_byte = read(filefd, write_buffer, lines[i]);
	if(read_byte != lines[i]) {
		perror("File read");
		return -1;
	}
         
	send_byte = send(new_sockfd, write_buffer, lines[i], 0);
	if(send_byte != lines[i]) {
		perror("Socket send");
		return -1;
	}
	free(write_buffer);
	}
	free(read_buffer);

	close(new_sockfd);

	syslog(LOG_INFO, "Closed connection from %s", str);
	}
	close(filefd);
	closelog();
	close(sockfd);
	return 0;
}
