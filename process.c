#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <time.h>

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

int soc2, amIinitiator, pNum, pId, amIbusy;
int dependsOn[10];

void* getMessage(void* arg){
	int from;
	while(1){
		struct sockaddr_in fadd;
		int sz = sizeof(struct sockaddr);
		int rt = recvfrom(soc2, &from, sizeof(int), 0, aCast(&fadd), &sz);
		fprintf(stderr, "Recevied a message from %d\n", ntohs(fadd.sin_port)/1000 -1);
		
		if(amIinitiator) {
			fprintf(stderr, "Deadlock detected.\n");
			exit(0);
		}

		if(!amIbusy){
			fprintf(stderr, "was not busy\n");
			int i;
			for(i=0;i<pNum;++i){
				if(dependsOn[i]){
					struct sockaddr_in sadd = {AF_INET, htons((i+2)*1000), inet_addr("127.0.0.1")};
					sendto(soc2, &from, sizeof(int), 0, aCast(&sadd), sizeof(struct sockaddr));
					fprintf(stderr, "Sent a probe to: %d\n", i+1);
				}
			}
		}
		else{
			fprintf(stderr, "was busy\n");
		}
	}

	pthread_exit(NULL);
}

void* getResource(void* _arg){
	srand(time(NULL));
	while(1){
		sleep(rand() & 2 | rand() & 1);
		int soc = socket(PF_INET, SOCK_STREAM, 0);
		struct sockaddr_in sadd = {AF_INET, htons(1500), inet_addr("127.0.0.1")};
		int rt = connect(soc, aCast(&sadd), sizeof(struct sockaddr));
		send(soc, &pId, sizeof(int), 0);
		recv(soc, &amIbusy, sizeof(int), 0);
		printf("rt: %d status: %s\n", rt, amIbusy==1 ? "Got resource" : "Didn't get it");

		if(!amIbusy){
			if(amIinitiator){
				puts("Initiated a probe");
				int i;
				for(i=0;i<pNum;++i){
					if(dependsOn[i]){
						struct sockaddr_in sadd = {AF_INET, htons((i+2)*1000), inet_addr("127.0.0.1")};
						sendto(soc2, &pId, sizeof(int), 0, aCast(&sadd), sizeof(struct sockaddr));
						printf("sent a probe to %d\n", i+1);
					}
				}
			}
			else {
				puts("was not a initiator");
				fflush(stdout);
			}
		}
	
		else {
			char buf[BUFSIZ];
			recv(soc, buf, BUFSIZ, 0);
			printf("Read: %s\n", buf);
			fflush(stdout);
			sleep(3);
			send(soc, "RELEASE", 8, 0);
		}
		
		close(soc);

		puts("round complete");
		puts("");
	}														

	pthread_exit(NULL);
}


int main(int argc, char const *argv[])
{
	pId = atoi(argv[1]);
	pNum = atoi(argv[2]);
	int port = (pId+1)*1000;

	int i;
	int index = 3;
	for(i=0;i<pNum;i++)
	  dependsOn[i] = atoi(argv[index++]);

	soc2 = createSocket(0, PF_INET, port, INADDR_ANY, 0);
	
	amIinitiator = atoi(argv[index]);

	pthread_t t1, t2;
	pthread_create(&t1, NULL, getMessage, NULL);
	pthread_create(&t2, NULL, getResource, NULL);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	
	return 0;
}