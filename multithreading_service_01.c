/*************************************************************************
    > File Name: multithreading_service_01.c
  > Author: AlexChen
  > Mail: longchen2008@126.com 
  > Created Time: Thu 07 Nov 2019 02:52:04 PM UTC
  > gcc -g -o main multithreading_service_01.c -lpthread
 ************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>
#include <ctype.h>

typedef void *(THREAD_BODY) (void *thread_arg);
void *thread_worker(void *ctx);

int thread_start(pthread_t *thread_id, THREAD_BODY *thread_workbody, void *thread_arg);

void print_usage(char *progname)
{
	printf("%s usage: \n", progname);
	printf("-p(--port): sepcify server listen port.\n");
	printf("-h(--Help): print this help information.\n");
	return;
}

int main(int argc, char **argv)
{
	int sockfd = -1;
	int rv =-1;
	struct sockaddr_in servaddr;
	struct sockaddr_in cliaddr;
	socklen_t len;
	int port = 0;
	int clifd;
	int ch;
	int on = 1;
	pthread_t tid;

	struct option opts[] = {
		{"port", required_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	while ((ch = getopt_long(argc, argv, "p:h", opts, NULL)) != -1)
	{
		switch(ch)
		{
			case 'p':
				port = atoi(optarg);
				break;
			case 'h':
				print_usage(argv[0]);
				return 0;
		}
	}

	if (!port)
	{
		print_usage(argv[0]);
		return 0;
	}

	sockfd =socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		printf("Create socket is failed: %s!\n", strerror(errno));
		return -1;
	}

	printf("Ctreate socket [%d] successfully!\n", sockfd);

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	rv = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (rv < 0)
	{
		printf("Socket [%d] bind on port [%d] is failed:%s\n", sockfd, port, strerror(errno));
		return -2;
	}

	listen(sockfd, 13);
	printf("Start to listen on port [%d]\n", port);

	while(1)
	{
		printf("Start accept new client incoming ...\n");

		clifd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
		if (clifd < 0)
		{
			printf("Accept new client is failed: %s\n", strerror(errno));
			continue;
		}

		printf("Accept new client [%s:%d] successfully\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

		thread_start(&tid, thread_worker, &clifd);
	}

	close(sockfd);
	return 0;
}

int thread_start(pthread_t *thread_id, THREAD_BODY *thread_workbody, void *thread_arg)
{
	int rv = -1;
	pthread_attr_t thread_attr;

	if (pthread_attr_init(&thread_attr))
	{
		printf("pthread_attr_init() is failed: %s\n", strerror(errno));
		goto CleanUp;
	}

	if (pthread_attr_setstacksize(&thread_attr, 120*1024))
	{
		printf("pthread_attr_setstacksize() is failed: %s\n", strerror(errno));
		goto CleanUp;
	}

	if (pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED))
	{
		printf("pthread_attr_setdetachstate() is failed: %s\n", strerror(errno));
		goto CleanUp;
	}

	if (pthread_create(thread_id, &thread_attr, thread_workbody, thread_arg))
	{
		printf("Create thread is failed: %s\n", strerror(errno));
		goto CleanUp;
	}

	rv = 0;
CleanUp:
	pthread_attr_destroy(&thread_attr);
	return rv;
}

void *thread_worker(void *ctx)
{
	int clifd;
	int rv;
	char buf[1024];
	int i;

	if (!ctx)
	{
		printf("Invalid input argument in %s()\n", __FUNCTION__);
		pthread_exit(NULL);
	}

	clifd = *(int *)ctx;
	printf("Child thread start to commuicate with socket client...\n");

	while (1)
	{
		memset(buf, 0, sizeof(buf));
		rv = read(clifd, buf, sizeof(buf));
		if(rv < 0)
		{
			printf("Read data from client sockfd [%d] is failed: %s and thread will exit\n", clifd, strerror(errno));
			close(clifd);
			pthread_exit(NULL);
		}
		else if (rv == 0)
		{
			printf("Socket [%d] get disconnected and thread will exit\n", clifd);
			close(clifd);
			pthread_exit(NULL);
		}
		else if (rv > 0)
		{
			printf("Read [%d] bytes data from Server: %s\n", rv, buf);
		}

		for (i = 0; i < rv; i++)
		{
			buf[i] = toupper(buf[i]);
		}

		rv = write(clifd, buf, rv);

		if(rv < 0)
		{
			printf("Write data to client sockfd [%d] is failed: %s and thread will exit\n", clifd, strerror(errno));
			close(clifd);
			pthread_exit(NULL);
		}
	}
}
