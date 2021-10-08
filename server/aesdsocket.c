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

int filefd, sockfd, new_sockfd;

char str[INET_ADDRSTRLEN];
int exit_code = 0, signal_exit_code=0;

void signal_handler(int signo) {

	if(signo == SIGINT || signo == SIGTERM) {
		syslog(LOG_INFO, "Caught signal, exiting");

		if(shutdown(sockfd, SHUT_RDWR)) {
			perror("Socket shutdown");
		}

		exit_code=1;
		signal_exit_code=1;
	}
	exit(0);
}

int cleanup() {
	if(exit_code) {

		close(filefd);

		closelog();

		close(sockfd);

		close(new_sockfd);

		printf("Closed connection from %s\n\r", str);
		syslog(LOG_INFO, "Closed connection from %s", str);
	}

	if(signal_exit_code) {

		if(remove(output_file) < 0) {
			perror("Delete tmp file");
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[]) {

	int port=9000;
	int rc;
	struct sockaddr_in server_addr, new_addr;
	char buffer[BUFFER_MAX];
	struct sockaddr_in * pV4Addr = (struct sockaddr_in*)&new_addr;
	struct in_addr ipAddr = pV4Addr->sin_addr;
	socklen_t addr_size;
	int daemon=0;

	int wrbuff_size=0;

	//check command line arguments
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

	//empty buffer
	memset(buffer, '\0', BUFFER_MAX);

	openlog(NULL, 0, LOG_USER);

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	//open socket connection
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		perror("Socket creation");
		exit_code=1;
	}

	if(!exit_code) {
		//set socket options
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))<0) {
			perror("error in setsockopt");
			exit_code=1;
		}
	}

	if(!exit_code) {
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);
		server_addr.sin_addr.s_addr = INADDR_ANY; 

		//bind socket server
		rc = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
		if(rc<0) {
			perror("Server bind");
			exit_code=1;
		}
	}



	//run the program as a daemon
	if(daemon == 1 && !exit_code) {
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

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		//redirect files to /dev/null
		open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
	}

	if(!exit_code) {
		//listen for connections
		rc = listen(sockfd, MAX_CONNECTS);
		if(rc<0) {
			perror("Server listen");
			exit_code=1;
		}
	}

	if(!exit_code) {
		//open /var/tmp/aesdsocketdata file
		filefd = open(output_file, O_RDWR | O_CREAT, 0644);
		if(filefd < 0) {
			perror("Open file");
			exit_code=1;
		}
	}

	if(!exit_code) {
		
		while(!signal_exit_code) {	//till we get a signal

			addr_size = sizeof(new_addr);

			//accept socket connections
			new_sockfd = accept(sockfd, (struct sockaddr*)&new_addr, &addr_size);
			if(new_sockfd<0) {
				perror("Server accept");
				exit_code=1;
			}

			if(!exit_code) {
				inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
				printf("Accepted connection from %s\n\r", str);
				syslog(LOG_INFO, "Accepted connection from %s", str);
			}
			
			while(1) {	//client sends \r
				long write_byte=0, recv_byte=0, send_byte=0, read_byte=0;
				char *read_buffer = NULL;
				char *write_buffer = NULL;
				long rdbuff_size = 0;
				long malloc_size = BUFFER_MAX;
				int break_loop=0;

				read_buffer = (char *)malloc(sizeof(char)*BUFFER_MAX);
				memset(read_buffer, '\0', BUFFER_MAX);
				if(read_buffer == NULL) {
					perror("Read Malloc failed");
					exit_code=1;
				}

				if(!exit_code) {
					do {
						//receive data over socket and store it in buffer
						recv_byte = recv(new_sockfd, buffer, sizeof(buffer), 0);

						//check for new-line character
						if( (strchr(buffer, '\n')!=NULL))
							break_loop=1;

						//check if allocated buffer size is sufficient
						if((malloc_size - rdbuff_size) < recv_byte) {

							malloc_size += recv_byte;
							//realloc required size
							read_buffer = (char *)realloc(read_buffer, sizeof(char)*malloc_size);
						}

						//copy received buffer in another read_buffer
						memcpy(&read_buffer[rdbuff_size], buffer, recv_byte);
						printf("recv_byte=%ld\n\r", recv_byte);
						rdbuff_size += recv_byte;

						if(recv_byte==0)
							break_loop=0;

					} while(break_loop != 1);
					printf("buffer = %s\n\r", buffer);	
					printf("Read_buffer = %s\n\r", read_buffer);	
				}
				//write from read_buffer in output file
				write_byte = write(filefd, read_buffer, rdbuff_size);
				if(write_byte != rdbuff_size) {
					perror("File write");
					exit_code=1;
				}

				if(!exit_code) {
					//seek the start of the file
					lseek(filefd, 0, SEEK_SET);

					//send contents writen in output file to client line by line
					wrbuff_size += rdbuff_size;
					write_buffer = (char *)malloc(sizeof(char)*(wrbuff_size+1));
					memset(write_buffer, '\0', wrbuff_size+1);
					if(write_buffer == NULL) {
						perror("Write malloc failed");
						exit_code=1;
					}
				}

				if(!exit_code) {
					//store contents os output file in write_buffer
					read_byte = read(filefd, write_buffer, wrbuff_size);
					if(read_byte != wrbuff_size) {
						perror("File read");
						exit_code=1;
					}

					printf("Write_buffer = %s\n\r", write_buffer);
				}
				//send contents of write_buffer to client
				//
				if(!exit_code) {
					send_byte = send(new_sockfd, write_buffer, wrbuff_size, 0);
					if(send_byte != wrbuff_size) {
						perror("Socket send");
						exit_code=1;
					}

					free(write_buffer);

					free(read_buffer);
				}
			}
			exit_code=1;
			if(!cleanup()) {
				return -1;
			}
			printf("Closed all files\n\r");
		}
	}
	exit_code=1;
	signal_exit_code=1;
	if(!cleanup()) {
		return -1;
	}
	printf("Shutdown all connections\n\r");
	return 0;
}

