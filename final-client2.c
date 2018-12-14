#define SEND "client2"
#define RECV "client1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <pthread.h>
#include <time.h>
#define MQ_4 "/mq4"
#define MQ_2 "/mq2"
#define MSG_SIZE 256
#define MAX_MSG 5
#define MAX_LOG 512

pthread_mutex_t  mutex;

int fd;
mqd_t mq4, mq2;
char send_message [MSG_SIZE];
char recv_message [MSG_SIZE];
char log_message [MAX_LOG];
char *ptr = '\0';
time_t ltime;
struct tm *today;
short in_user = 0;

void logg_f(char* str, char* user){
	pthread_mutex_lock(&mutex);
	time(&ltime);
	today = localtime(&ltime);
	ptr = asctime(today);
	ptr[strlen(ptr)-1]='\0';
	sprintf(log_message,"[ %s ] %s : %s \n", ptr, user, str);
	write(fd, log_message, strlen(log_message));
	memset(log_message, '\0', sizeof(log_message));
	pthread_mutex_unlock(&mutex);
}

void *sender(void* args){
	while(1){
		memset(send_message, '\0', sizeof(send_message));
		//printf("> ");
		fgets(send_message, sizeof(send_message), stdin);
		send_message[strlen(send_message)-1] = '\0';

		if( mq_send(*(mqd_t*)args, send_message, strlen(send_message), 0) == -1){
			perror("mq_send()");
		}
		else{
			logg_f(send_message, SEND);
			if( strcmp(send_message, "/q") == 0 ){
				printf(" 채팅을 종료합니다. \n");
				pthread_mutex_destroy(&mutex);
				close(fd);
				mq_close(mq4);
				mq_close(mq2);
				if(in_user){
					mq_unlink(MQ_4);
					mq_unlink(MQ_2);
				}
				exit(0);
			}
			printf("%s : %s \n", SEND, send_message);
		}
	}
}

void *receiver(void* args){
	while(1){
		while(mq_receive(*(mqd_t*)args, recv_message, sizeof(recv_message), 0) > 0 ){
			logg_f(recv_message, RECV);
			if( strcmp(recv_message, "/s") == 0){
				in_user = 0;
				memset(recv_message, '\0', sizeof(recv_message));
				break;
			}

			if( strcmp(recv_message, "/q") == 0 ){
				in_user = 1;
				printf(" 다른 유저가 나갔습니다. \n");
				memset(recv_message, '\0', sizeof(recv_message));
				break;
			}
			printf("%s : %s\n", RECV, recv_message);
			memset(recv_message, '\0', sizeof(recv_message));
		}
	}
}


int main(int argc, char** argv){
	pthread_t	sendthread, recvthread;
	struct mq_attr	attr;
	int  ret, status=0;
	char logname[10];
	sprintf(logname, "%s.txt", "client2log");

	pthread_mutex_init(&mutex, NULL);
	attr.mq_maxmsg = MAX_MSG;
	attr.mq_msgsize = MSG_SIZE;

	mq4 = mq_open(MQ_4, O_RDWR | O_CREAT, 0666, attr);
	mq2 = mq_open(MQ_2, O_CREAT | O_RDWR, 0666, attr);
	fd = open(logname, O_WRONLY | O_CREAT, 0666);

	if((mq4 == (mqd_t)-1) || (mq2 == (mqd_t)-1)){
		perror("메시지 큐를 열수 없습니다.");
		exit(0);
	}

	ret = pthread_create(&sendthread, NULL, sender, (void*)&mq2);
	if( ret < 0 ){
		perror("mq2 생성 오류 : ");
		exit(0);
	}

	ret = pthread_create(&recvthread, NULL, receiver, (void*)&mq4);
	if( ret < 0){
		perror("mq4 생성 오류 : ");
		exit(0);
	}
	printf("대화를 시작합니다. 대화에서 나가고 싶으시다면 /q를 입력해주세요. \n");
	strcpy(send_message, "/s");
	mq_send(mq2, send_message, strlen(send_message), 0);

	pthread_join(sendthread, (void**)&status);
	pthread_join(recvthread, (void**)&status);

	return 0;
}
