/*******************************************************/
/* AESD Assignment 5 - socket server code              */
/*******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <syslog.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <linux/fs.h>
#include <pthread.h>
#include <time.h>

#include "queue.h"

#define output_file "/var/tmp/aesdsocketdata"
#define BUFFER_MAX 100
#define MAX_CONNECTS 10

int filefd, sockfd, new_sockfd;

char buffer[BUFFER_MAX];

char *str;
int signal_exit_code = 0;

pthread_mutex_t data_mutex;

struct thread_data
{

	pthread_t mythread;
	int thread_sockfd;

	bool thread_complete;
};

// SLIST.
struct slist_data_s
{
	struct thread_data thread_param;
	SLIST_ENTRY(slist_data_s)
	entries;
};

void signal_handler(int signo)
{

	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_INFO, "Caught signal, exiting");
		printf("Caught Signal, Exiting\n\r");

		if (shutdown(sockfd, SHUT_RDWR))
		{
			perror("Socket shutdown");
		}

		signal_exit_code = 1;
	}
}

int cleanup()
{

	printf("in cleanup\n\r");

	pthread_mutex_destroy(&data_mutex);

	close(filefd);

	closelog();

	close(sockfd);

	close(new_sockfd);

	if (signal_exit_code)
	{

		if (remove(output_file) < 0)
		{
			perror("Delete tmp file");
			return -1;
		}
	}

	return 0;
}

static inline void timespec_add(struct timespec *result,
								const struct timespec *ts_1, const struct timespec *ts_2)
{
	result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
	result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
	if (result->tv_nsec > 1000000000L)
	{
		result->tv_nsec -= 1000000000L;
		result->tv_sec++;
	}
}

static void timer_thread()
{

	time_t rawtime;
	struct tm *info;
	int timer_size, timer_write;
	char *buff = malloc(sizeof(char) * BUFFER_MAX);

	time(&rawtime);

	info = localtime(&rawtime);

	timer_size = strftime(buff, BUFFER_MAX, "timestamp:%Y %b %a - %H:%M:%S\n", info);
	if(timer_size == 0) {
		perror("timestamp");
		goto exit_timer;
	}

	if (pthread_mutex_lock(&data_mutex) != 0)
	{
		perror("timer data lock!\n");
		goto exit_timer;
	}
	else
	{
		timer_write = write(filefd, buff, timer_size);
		if(timer_write != timer_size) {
			perror("writing timestamp in file");
			goto exit_timer;
		}

		if (pthread_mutex_unlock(&data_mutex) != 0)
		{
			perror("unlock timer data!\n");
			goto exit_timer;
		}
	}

exit_timer:
	free(buff);
}

void *thread_func(void *thread_param)
{

	struct thread_data *ind_param = (struct thread_data *)thread_param;
	int th_newfd = ind_param->thread_sockfd;

	long write_byte = 0, recv_byte = 0, send_byte = 0, read_byte = 0;
	char *read_buffer = NULL;
	char *write_buffer = NULL;
	long rdbuff_size = 0;
	long wrbuff_size = 0;
	long malloc_size = BUFFER_MAX, realloc_size = BUFFER_MAX;
	int break_loop = 0;
	long cur_pos = 0, end_pos = 0, required_memory = 0;
	long bytes = 0;
	char *new_line = NULL;

	printf("Executing thread with fd = %d\n\r", th_newfd);

	read_buffer = (char *)malloc(sizeof(char) * BUFFER_MAX);
	memset(read_buffer, '\0', BUFFER_MAX);
	if (read_buffer == NULL)
	{
		perror("Read Malloc failed");
		goto exit;
	}

	do
	{
		//receive data over socket and store it in buffer
		recv_byte = recv(th_newfd, buffer, sizeof(buffer), 0);

		//check for new-line character
		if (!recv_byte || (strchr(buffer, '\n') != NULL))
			break_loop = 1;

		//check if allocated buffer size is sufficient
		if ((malloc_size - rdbuff_size) < recv_byte)
		{

			malloc_size += recv_byte;
			//realloc required size
			read_buffer = (char *)realloc(read_buffer, sizeof(char) * malloc_size);
		}

		//copy received buffer in another read_buffer
		memcpy(&read_buffer[rdbuff_size], buffer, recv_byte);

		rdbuff_size += recv_byte;

	} while (break_loop != 1);

	break_loop = 0;

	pthread_mutex_lock(&data_mutex);

	//write from read_buffer in output file
	write_byte = write(filefd, read_buffer, rdbuff_size);
	if (write_byte != rdbuff_size)
	{
		perror("File write");
		goto exit;
	}

	end_pos = lseek(filefd, 0, SEEK_END);

	//seek the start of the file
	lseek(filefd, 0, SEEK_SET);

	pthread_mutex_unlock(&data_mutex);

	while (bytes != end_pos)
	{
		wrbuff_size = 0;
		required_memory = end_pos - cur_pos; //get the required memory size for one line

		//send contents writen in output file to client line by line
		write_buffer = (char *)malloc(sizeof(char) * BUFFER_MAX);

		if (write_buffer == NULL)
		{
			perror("Write malloc failed");
			goto exit;
		}

		pthread_mutex_lock(&data_mutex);

		do
		{

			//store contents os output file in write_buffer
			read_byte = read(filefd, write_buffer + wrbuff_size, sizeof(char));

			if (realloc_size < required_memory)
			{
				realloc_size += required_memory;
				write_buffer = (char *)realloc(write_buffer, sizeof(char) * realloc_size);
			}

			wrbuff_size += read_byte;

			if (wrbuff_size > 1)
				new_line = strchr(write_buffer, '\n');

		} while (new_line == NULL);

		pthread_mutex_unlock(&data_mutex);
		bytes += wrbuff_size;

		cur_pos = lseek(filefd, 0, SEEK_CUR);

		//send contents of write_buffer to client
		send_byte = send(th_newfd, write_buffer, wrbuff_size, 0);
		if (send_byte != wrbuff_size)
		{
			perror("Socket send");
			goto exit;
		}

		free(write_buffer);
	}

	free(read_buffer);
	close(th_newfd);

	printf("Closed connection from %s\n\r", str);
	syslog(LOG_INFO, "Closed connection from %s", str);

	ind_param->thread_complete = true;

exit:
	pthread_exit(NULL);

	return ind_param;
}

int main(int argc, char *argv[])
{

	int port = 9000;
	int rc;
	struct sockaddr_in server_addr, new_addr;
	socklen_t addr_size;
	int daemon = 0;

	struct sigevent sev;
	struct timespec start_time;
	timer_t timerid;

	int clock_id = CLOCK_MONOTONIC;
	memset(&sev, 0, sizeof(struct sigevent));

	struct slist_data_s *datap = NULL;

	//check command line arguments
	if (argc == 1)
	{
		daemon = 0;
	}

	else if (argc > 2)
	{
		perror("Invalid arguments");
		return -1;
	}

	else if (argc == 2)
	{
		if (strcmp(argv[1], "-d") == 0)
			daemon = 1;
	}

	openlog(NULL, 0, LOG_USER);

	SLIST_HEAD(slisthead, slist_data_s)
	head;
	SLIST_INIT(&head);

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if (pthread_mutex_init(&data_mutex, NULL) != 0)
	{
		printf("\n mutex init failed\n");
		goto cleanup;
	}

	//open socket connection
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("Socket creation");
		goto cleanup;
	}

	//set socket options
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
	{
		perror("error in setsockopt");
		goto cleanup;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	//bind socket server
	rc = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (rc < 0)
	{
		perror("Server bind");
		goto cleanup;
	}

	//run the program as a daemon
	if (daemon == 1)
	{
		pid_t pid;
		printf("Run as daemon\n\r");

		signal(SIGCHLD, SIG_IGN);
		signal(SIGHUP, SIG_IGN);

		pid = fork();
		if (pid == -1)
			return -1;
		else if (pid != 0)
			exit(EXIT_SUCCESS);

		umask(0);

		if (setsid() == -1)
			return -1;

		if (chdir("/") == -1)
			return -1;

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		//redirect files to /dev/null
		open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
	}

	//listen for connections
	rc = listen(sockfd, MAX_CONNECTS);
	if (rc < 0)
	{
		perror("Server listen");
		goto cleanup;
	}

	//open /var/tmp/aesdsocketdata file
	filefd = open(output_file, O_RDWR | O_CREAT, 0644);
	if (filefd < 0)
	{
		perror("Open file");
		goto cleanup;
	}

	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = timer_thread;

	if (timer_create(clock_id, &sev, &timerid) != 0)
	{
		perror("timer create");
		goto cleanup;
	}

	if (clock_gettime(clock_id, &start_time) != 0)
	{
		perror("clock get time");
		goto cleanup;
	}
	else
	{
		struct itimerspec itimerspec;
		itimerspec.it_interval.tv_sec = 10;
		itimerspec.it_interval.tv_nsec = 1000000;
		timespec_add(&itimerspec.it_value, &start_time, &itimerspec.it_interval);
		if (timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL) != 0)
		{
			perror("timer set time");
			goto cleanup;
		}
	}

	while (!signal_exit_code)
	{ //till we get a signal

		addr_size = sizeof(new_addr);

		//accept socket connections
		new_sockfd = accept(sockfd, (struct sockaddr *)&new_addr, &addr_size);
		if (new_sockfd < 0)
		{
			perror("Server accept");
			goto cleanup;
		}

		str = inet_ntoa(new_addr.sin_addr);
		printf("Accepted connection from %s\n\r", str);
		syslog(LOG_INFO, "Accepted connection from %s", str);

		datap = malloc(sizeof(struct slist_data_s));
		(datap->thread_param).thread_sockfd = new_sockfd;
		(datap->thread_param).thread_complete = false;

		SLIST_INSERT_HEAD(&head, datap, entries);

		if (pthread_create(&((datap->thread_param).mythread), NULL, &thread_func, (void *)&(datap->thread_param)) != 0)
		{
			printf("error in pthread_create\n");
			goto cleanup;
		}

		SLIST_FOREACH(datap, &head, entries)
		{
			if ((datap->thread_param).thread_complete == true)
			{
				pthread_join((datap->thread_param).mythread, NULL);
			}
		}
	}

cleanup:

	SLIST_FOREACH(datap, &head, entries)
	{
		pthread_join((datap->thread_param).mythread, NULL);
	}

	while (!SLIST_EMPTY(&head))
	{
		datap = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head, entries);
		free(datap);
	}

	if (cleanup() < 0)
	{
		return -1;
	}
	printf("Closed all files\n\r");

	return 0;
}
