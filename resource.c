#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>

#define exit_on_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while(0)

#define aCast(x) (struct sockaddr*)x

int createSocket(int type, int aFm, short pNum, int bAdd, int nListen){
	struct sockaddr_in lAdd;
	int fd, rt, sz = sizeof(struct sockaddr_in);
	{
		int M, socType;
		M = -(type==1);
		socType = (SOCK_STREAM & M) | (SOCK_DGRAM & ~M);
		fd = socket(aFm, socType, 0);
	}
	if(fd==-1) exit_on_error("createSocket:socket");
	
	{
		int state = 1;
 		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &state, sizeof(int));
	}

	bzero(&lAdd, sz);
	lAdd.sin_family = aFm;
	lAdd.sin_port = htons(pNum);
	lAdd.sin_addr.s_addr = bAdd;
	rt = bind(fd, aCast(&lAdd), sz);
	if(rt==-1) exit_on_error("createSocket:bind");
	if(type==1 && nListen > 0) { rt = listen(fd, nListen);  // listen operation not supported for UDP
					if(rt==-1) exit_on_error("createSocket:listen"); }
	return fd;
}

pthread_mutex_t res = PTHREAD_MUTEX_INITIALIZER;

void* on_thread_create(void* _fd){
	pthread_detach(pthread_self());

	int fd = (intptr_t)_fd;
	int pid;
	
	recv(fd, &pid, sizeof(int), 0);
	printf("Thread for pid: %d\n", pid);
	fflush(stdout);

	int state = pthread_mutex_trylock(&res);

	if(state==0){
		int reply = 1;
		send(fd, &reply, sizeof(int), 0);
		char buf[BUFSIZ];
		int fdd = open("blabla.txt", O_RDONLY);
		int rt = read(fdd, buf, BUFSIZ);
		send(fd, buf, rt, 0);
		recv(fd, buf, BUFSIZ, 0);
		printf("Read: %s form %d\n", buf, pid);
		close(fdd);
		fflush(stdout);
		pthread_mutex_unlock(&res);
	}

	else{
		int reply = 0;
		send(fd, &reply, sizeof(int), 0);
	}

	close(fd);
	pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
	int soc = createSocket(1, PF_INET, 1500, INADDR_ANY, 10);
	char buf[BUFSIZ];

	while(1){
		struct sockaddr_in cadd;
		int sz = sizeof(struct sockaddr);
		int cli = accept(soc, aCast(&cadd), &sz);
		pthread_t t;
		pthread_create(&t, NULL, on_thread_create, (void*)(intptr_t)cli);
	}

	return 0;
}