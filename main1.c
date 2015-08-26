//gcc -o main1 main1.c -lpthread

//----------------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include<signal.h>

#define UDPDATA 1472
#define SLADATA 1456
#define TCPDATA 1472
#define SLAdata 1456
#define SCTPDATA 1472
#define SLAData 1456

#define UDPPORT 10000//thread0
#define TCPPORT 10001//thread1
#define SCTPPORT 10002//thread2
#define FTCPPORT 10003//delivery file feedback

#define UDPPORTm 10004//thread4
#define TCPPORTm 10005//thread5
#define SCTPPORTm 10006//thread6
#define FTCPPORTm 10007//heart-beat measurement feedback
//----------------------------------------------------------------------------------

//global variables for thread3 heart-beat measurement feedback and main function
float fbPLR;
long int fbtimestemps;
long int fbtimestempm;
//----------------------------------------------------------------------------------

//SLA header
struct SLA_header{
	unsigned char typeenddatalen1;
	unsigned char datalen2;
	unsigned short socketseq;
	long int commonseq;
	long int timestemps;
	long int timestempm;
};
//SLA data
struct SLA_data{
	char data[SLADATA];
};
//feedback
struct feedback{
	float PLR;
	long int timestemps;
	long int timestempm;
};
//Thread para
struct thread_para
{
	char *ip[256];
	char *filename[256];
	int start;
	int end;
};
//----------------------------------------------------------------------------------

//help
void usage(char *command)
{
	printf("usage :%s ipaddr heartbeat.txt\n", command);
	exit(0);
}
//----------------------------------------------------------------------------------

//UDP socket thread
void *thread0(void *argc)
{
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;
	
	FILE *fp;
	struct sockaddr_in serv_addr;
	char data_send[UDPDATA];
	int udpsock_id;
	int read_len;
	int send_len;
	int udpi_ret;
	int udpseq,udplast,commonseq;

	struct SLA_header *header = (struct SLA_header*)data_send;
	struct SLA_data *data = (struct SLA_data*)(data_send + sizeof(struct SLA_header));

	struct  timeval    tv;
	struct  timezone   tz;

	memset(data_send,0,sizeof data_send);

    /* open the file */
	if ((fp = fopen(threadpa->filename,"r")) == NULL) {
		perror("Open file failed t0\n");
		exit(0);
    }
    /* create the udp socket */
	if ((udpsock_id = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("UDP Create socket failed");
		exit(0);
	}
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(UDPPORT);
	inet_pton(AF_INET, threadpa->ip, &serv_addr.sin_addr);
    /* connect the server */
	udpi_ret = connect(udpsock_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	if (-1 == udpi_ret) {
		perror("UDP Connect socket failed!\n");
		exit(0);
	}
    /* transport the file */
	bzero(data->data, SLADATA);
	udpseq=1;
	commonseq=1;
	while ( (read_len = fread(data->data, sizeof(char), SLADATA, fp)) > 0 ) {
		if ( (commonseq >= threadpa->start) && (commonseq <= threadpa->end) ){
			if ( read_len < SLADATA ) {
				header->typeenddatalen1 = read_len/256+32;
			} else {
				header->typeenddatalen1 = read_len/256;
			}
			header->datalen2 = read_len%256;
			header->socketseq = udpseq;
			header->commonseq = commonseq;

			gettimeofday(&tv,&tz);

			header->timestemps = htonl(tv.tv_sec);
			header->timestempm = htonl(tv.tv_usec);

			if ( read_len < SLADATA ) {
				for (udplast=1;udplast<=20;udplast++) {
				send_len = send(udpsock_id, data_send, read_len+16, 0);
				}
				break;
			}
			send_len = send(udpsock_id, data_send, read_len+16, 0);
			if ( send_len < 0 ) {
				perror("UDP Send data failed\n");
				exit(0);
			}
			udpseq=udpseq+1;
			bzero(data->data, SLADATA);
			memset(data_send,0,sizeof data_send);
		}
		bzero(data->data, SLAData);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}

    /* ending packet */
	header->typeenddatalen1 = 0x25;
	header->datalen2 = 0x00;
	header->socketseq = udpseq;
	header->commonseq = commonseq;
	gettimeofday(&tv,&tz);
	header->timestemps = htonl(tv.tv_sec);
	header->timestempm = htonl(tv.tv_usec);
	bzero(data->data, SLADATA);
	for (udplast=1;udplast<=20;udplast++) {
		send_len = send(udpsock_id, data_send, read_len+16, 0);
	}

	fclose(fp);
	close(udpsock_id);
	printf("UDP Socket Send finish\n");
}
//----------------------------------------------------------------------------------

//TCP socket thread
void *thread1(void *argc) 
{
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;

	struct sockaddr_in serv_addr;
	int tcpsock_id;
	int read_len;
	int send_len;
	FILE *fp;
	int i_ret, tcpseq, commonseq;
	unsigned short typeenddatalen;
	char data_send[TCPDATA];

	struct SLA_header *header = (struct SLA_header*)data_send;
	struct SLA_data *data = (struct SLA_data*)(data_send + sizeof(struct SLA_header));

	struct  timeval    tv;
	struct  timezone   tz;

	memset(data_send,0,sizeof data_send);
    
    /* open the file */
	if ((fp = fopen(threadpa->filename,"r")) == NULL) {
		perror("Open file failed t1\n");
		exit(0);
	}
    
    /* create the socket */
	if ((tcpsock_id = socket(AF_INET,SOCK_STREAM,0)) < 0) {
		perror("TCP Create socket failed\n");
		exit(0);
	}
    
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(TCPPORT);
	inet_pton(AF_INET, threadpa->ip, &serv_addr.sin_addr);
  
    /* connect the server */
	i_ret = connect(tcpsock_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	if (-1 == i_ret) {
		printf("TCP Connect socket failed t1\n");
		return -1;
	}
    /* transported the file */
	bzero(data->data, SLAdata);
	tcpseq=1;
	commonseq=1;
	while ((read_len = fread(data->data, sizeof(char), SLAdata, fp)) >0 ) {
		if ( (commonseq >= threadpa->start) && (commonseq <= threadpa->end) ){
			typeenddatalen = 16384 + read_len;
			header->typeenddatalen1=(((typeenddatalen) >> 8) & 0xFF);
			header->datalen2=((typeenddatalen) & 0xFF);
			header->socketseq = tcpseq;
			header->commonseq = commonseq;
			
			gettimeofday(&tv,&tz);
			header->timestemps = htonl(tv.tv_sec);
			header->timestempm = htonl(tv.tv_usec);
			send_len = send(tcpsock_id, data_send, read_len+16, 0);
			if ( send_len < 0 ) {
				perror("TCP Send file failed\n");
				exit(0);
			}
			tcpseq=tcpseq+1;
			bzero(data->data, SLAdata);
			memset(data_send,0,sizeof data_send);
		}
		bzero(data->data, SLAData);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}

	fclose(fp);
	close(tcpsock_id);
	printf("TCP Socket Send Finish\n");
	return 0;
}
//----------------------------------------------------------------------------------

//SCTP socket thread
void *thread2(void *argc) 
{
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;

	struct sockaddr_in serv_addr;
	int sctpsock_id, sctpseq, read_len, send_len, i_ret, commonseq;
	FILE *fp;
	unsigned short typeenddatalen;
	char data_send[SCTPDATA];

	struct SLA_header *header = (struct SLA_header*)data_send;
	struct SLA_data *data = (struct SLA_data*)(data_send + sizeof(struct SLA_header));

	struct  timeval    tv;
	struct  timezone   tz;

	memset(data_send,0,sizeof data_send);
    
    /* open the file */
	if ((fp = fopen(threadpa->filename,"r")) == NULL) {
		perror("Open file failed t2\n");
		exit(0);
	}
    
    /* create the socket */
	if ((sctpsock_id = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP )) < 0) {
		perror("Create SCTP socket failed\n");
		exit(0);
	}
    
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SCTPPORT);
	inet_pton(AF_INET, threadpa->ip, &serv_addr.sin_addr);
  
    /* connect the server */
	i_ret = connect( sctpsock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr) );
	if (-1 == i_ret) {
		printf("Connect SCTP socket failed\n");
		return -1;
	}
    /* transported the file */
	bzero(data->data, SLAData);
	sctpseq=1;
	commonseq=1;
	while ((read_len = fread(data->data, sizeof(char), SLAData, fp)) >0 ) {
		if ( (commonseq >= threadpa->start) && (commonseq <= threadpa->end) ){
			typeenddatalen = 32768 + read_len;
			header->typeenddatalen1=(((typeenddatalen) >> 8) & 0xFF);
			header->datalen2=((typeenddatalen) & 0xFF);
			header->socketseq = sctpseq;
			header->commonseq = commonseq;
			
			gettimeofday(&tv,&tz);
			header->timestemps = htonl(tv.tv_sec);
			header->timestempm = htonl(tv.tv_usec);
			
			send_len = sctp_sendmsg( sctpsock_id, data_send, read_len+16, NULL, 0, 0, 0, 0, 0, 0 );			
			if ( send_len < 0 ) {
				perror("SCTP Send file failed\n");
				exit(0);
			}
			sctpseq=sctpseq+1;
			bzero(data->data, SLAData);
			memset(data_send,0,sizeof data_send);
		}
		bzero(data->data, SLAData);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}

	fclose(fp);
	close(sctpsock_id);
	printf("SCTP Socket Send Finish\n");
	return 0;
}
//----------------------------------------------------------------------------------

//heart-beat measurement thread
void *thread3(void *argc) 
{

//UDP heart-beat measurement thread
void *thread4(void *argc)
{
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;
	
	FILE *fp;
	struct sockaddr_in serv_addr;
	char data_send[UDPDATA];
	int udpsock_id;
	int read_len;
	int send_len;
	int udpi_ret;
	int udpseq,udplast,commonseq;
	int socknum=3;

	struct SLA_header *header = (struct SLA_header*)data_send;
	struct SLA_data *data = (struct SLA_data*)(data_send + sizeof(struct SLA_header));

	struct  timeval    tv;
	struct  timezone   tz;

	memset(data_send,0,sizeof data_send);

    /* open the file */
	if ((fp = fopen(threadpa->filename,"r")) == NULL) {
		perror("Open file failed t4\n");
		exit(0);
	}
    /* create the udp socket */
	if ((udpsock_id = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("UDP Create socket failed");
		exit(0);
	}
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(UDPPORTm);
	inet_pton(AF_INET, threadpa->ip, &serv_addr.sin_addr);
    /* connect the server */
	udpi_ret = connect(udpsock_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	if (-1 == udpi_ret) {
		perror("UDP Connect socket failed!\n");
		exit(0);
	}
    /* transport the file */
	bzero(data->data, SLADATA);
	udpseq=1;
	commonseq=1;
	while ( (read_len = fread(data->data, sizeof(char), SLADATA, fp)) > 0 ) {
		if ( (commonseq%socknum) == 1 ){
			if ( read_len < SLADATA ) {
				header->typeenddatalen1 = read_len/256+32;
			} else {
				header->typeenddatalen1 = read_len/256;
			}
			header->datalen2 = read_len%256;
			header->socketseq = udpseq;
			header->commonseq = commonseq;

			gettimeofday(&tv,&tz);

			header->timestemps = htonl(tv.tv_sec);
			header->timestempm = htonl(tv.tv_usec);

			if ( read_len < SLADATA ) {
				for (udplast=1;udplast<=20;udplast++) {
				send_len = send(udpsock_id, data_send, read_len+16, 0);
				}
				break;
			}
			send_len = send(udpsock_id, data_send, read_len+16, 0);
			if ( send_len < 0 ) {
				perror("UDP Send data failed\n");
				exit(0);
			}
			udpseq=udpseq+1;
		}
		bzero(data->data, SLADATA);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}

    /* ending packet */
	header->typeenddatalen1 = 0x25;
	header->datalen2 = 0x00;
	header->socketseq = udpseq;
	header->commonseq = udpseq;
	gettimeofday(&tv,&tz);
	header->timestemps = htonl(tv.tv_sec);
	header->timestempm = htonl(tv.tv_usec);
	bzero(data->data, SLADATA);
	for (udplast=1;udplast<=20;udplast++) {
		send_len = send(udpsock_id, data_send, read_len+16, 0);
	}

	fclose(fp);
	close(udpsock_id);
}
//------------------------------------------

//TCP heart-beat measurement thread
void *thread5(void *argc) 
{
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;

	struct sockaddr_in serv_addr;
	int tcpsock_id;
	int read_len;
	int send_len;
	FILE *fp;
	int i_ret, tcpseq, commonseq;
	unsigned short typeenddatalen;
	char data_send[TCPDATA];
	int socknum=3;

	struct SLA_header *header = (struct SLA_header*)data_send;
	struct SLA_data *data = (struct SLA_data*)(data_send + sizeof(struct SLA_header));

	struct  timeval    tv;
	struct  timezone   tz;

	memset(data_send,0,sizeof data_send);
    
    /* open the file */
	if ((fp = fopen(threadpa->filename,"r")) == NULL) {
		perror("Open file failed t5\n");
		exit(0);
	}
    /* create the socket */
	if ((tcpsock_id = socket(AF_INET,SOCK_STREAM,0)) < 0) {
		perror("TCP Create socket failed\n");
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(TCPPORTm);
	inet_pton(AF_INET, threadpa->ip, &serv_addr.sin_addr);
    /* connect the server */
	i_ret = connect(tcpsock_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	if (-1 == i_ret) {
		printf("TCP Connect socket failed t5\n");
		return -1;
	}
    /* transported the file */
	bzero(data->data, SLAdata);
	tcpseq=1;
	commonseq=1;
	while ((read_len = fread(data->data, sizeof(char), SLAdata, fp)) >0 ) {
		if ( (commonseq%socknum) == 0 ){
			typeenddatalen = 16384 + read_len;
			header->typeenddatalen1=(((typeenddatalen) >> 8) & 0xFF);
			header->datalen2=((typeenddatalen) & 0xFF);
			header->socketseq = tcpseq;
			header->commonseq = commonseq;
			
			gettimeofday(&tv,&tz);

			header->timestemps = htonl(tv.tv_sec);
			header->timestempm = htonl(tv.tv_usec);
			send_len = send(tcpsock_id, data_send, read_len+16, 0);
			if ( send_len < 0 ) {
				perror("TCP Send file failed\n");
				exit(0);
			}
			tcpseq=tcpseq+1;
		}
		bzero(data->data, SLAdata);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}
	fclose(fp);
	close(tcpsock_id);
	return 0;
}
//------------------------------------------

//SCTP heart-beat measurement thread
void *thread6(void *argc) 
{
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;

	struct sockaddr_in serv_addr;
	int sctpsock_id, sctpseq, read_len, send_len, i_ret, commonseq;
	FILE *fp;
	unsigned short typeenddatalen;
	char data_send[SCTPDATA];
	int socknum=3;

	struct SLA_header *header = (struct SLA_header*)data_send;
	struct SLA_data *data = (struct SLA_data*)(data_send + sizeof(struct SLA_header));

	struct  timeval    tv;
	struct  timezone   tz;

	memset(data_send,0,sizeof data_send);
    
    /* open the file */
	if ((fp = fopen(threadpa->filename,"r")) == NULL) {
		perror("Open file failed t6\n");
		exit(0);
	}
    /* create the socket */
	if ((sctpsock_id = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP )) < 0) {
		perror("Create SCTP socket failed\n");
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SCTPPORTm);
	inet_pton(AF_INET, threadpa->ip, &serv_addr.sin_addr);
    /* connect the server */
	i_ret = connect( sctpsock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr) );
	if (-1 == i_ret) {
		printf("Connect SCTP socket failed\n");
		return -1;
	}
    /* transported the file */
	bzero(data->data, SLAData);
	sctpseq=1;
	commonseq=1;
	while ((read_len = fread(data->data, sizeof(char), SLAData, fp)) >0 ) {
		if ( (commonseq%socknum) == 2 ){
			typeenddatalen = 32768 + read_len;
			header->typeenddatalen1=(((typeenddatalen) >> 8) & 0xFF);
			header->datalen2=((typeenddatalen) & 0xFF);
			header->socketseq = sctpseq;
			header->commonseq = commonseq;
			
			gettimeofday(&tv,&tz);

			header->timestemps = htonl(tv.tv_sec);
			header->timestempm = htonl(tv.tv_usec);
			
			send_len = sctp_sendmsg( sctpsock_id, data_send, read_len+16, NULL, 0, 0, 0, 0, 0, 0 );			
			if ( send_len < 0 ) {
				perror("SCTP Send file failed\n");
				exit(0);
			}
			sctpseq=sctpseq+1;
			bzero(data->data, SLAData);
			memset(data_send,0,sizeof data_send);
		}
		bzero(data->data, SLAData);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}

	fclose(fp);
	close(sctpsock_id);
	return 0;
}
//------------------------------------------

    /* thread3 mian */
	struct thread_para *threadpa;
	threadpa = (struct thread_para *) argc;//threadpa->filename; threadpa->ip; threadpa->start; threadpa->end;

	int ret;
	pthread_t ID4,ID5,ID6;
	struct thread_para threadpara;//for thread4,5,6

	strcpy(threadpara.ip, threadpa->ip);
	strcpy(threadpara.filename, threadpa->filename);

    /* kill thread3 */
	int res;
	res = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (res != 0) {
		perror("Thread pthread_setcancelstate failed");
	}
	res = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	if (res != 0) {
		perror("Thread pthread_setcanceltype failed");
	}
//------------------------------------------

	int on=1;//enable address reuse
	char feedback_recv[12];
	struct feedback *feedback = (struct feedback*)feedback_recv;//s2
	struct sockaddr_in serv_addr;
	struct sockaddr_in clie_addr;
	int clie_addr_len, feedbacktcp_id, feedbacklink_id, recv_len;

	while(1){
    /* start heart-beat measurement */
		ret = pthread_create(&ID4, NULL, (void *)thread4, &threadpara);
		if (ret !=0 ){
			printf("create udp-thread4 error！\n");
			exit(1);
		}
		ret = pthread_create(&ID5, NULL, (void *)thread5, &threadpara);
		if (ret !=0 ){
			printf("create tcp-thread5 error！\n");
			exit(1);
		}
		ret = pthread_create(&ID6, NULL, (void *)thread6, &threadpara);
		if (ret !=0 ){
			printf("create tcp-thread6 error！\n");
			exit(1);
		}
		pthread_join(ID4,NULL);
		pthread_join(ID5,NULL);
		pthread_join(ID6,NULL);
//------------------------------------------

    /* feedback tcp */
		clie_addr_len = sizeof(clie_addr);
		if ((feedbacktcp_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("Create feedback TCP socket failed\n");
			exit(0);
		}

		ret=setsockopt(feedbacktcp_id, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));//enable address reuse

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(FTCPPORTm);
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(feedbacktcp_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
			perror("Bind TCP socket failed 1\n");
			exit(0);
		}
		if (-1 == listen(feedbacktcp_id, 10)) {
			perror("Listen TCP socket failed\n");
			exit(0);
		}
		clie_addr_len = sizeof(clie_addr);
		feedbacklink_id = accept(feedbacktcp_id, (struct sockaddr *)&clie_addr, &clie_addr_len);
		if (-1 == feedbacklink_id) {
			perror("Accept socket failed\n");
			exit(0);
		}
		memset(feedback_recv,0,sizeof feedback_recv);
		while(recv_len = recv(feedbacklink_id, feedback_recv, 12, 0)){
			fbtimestemps=feedback->timestemps;
			fbtimestempm=feedback->timestempm;
			fbPLR=feedback->PLR;
		}
		close(feedbacklink_id);
		close(feedbacktcp_id);
//------------------------------------------
		sleep(5);
	}

	return 0;
}//thread3 finish
//----------------------------------------------------------------------------------

int main(int argc,char **argv)
{
	int ret,res;
	char dt[16],pl[16];
	float deliverytime, plr;
	pthread_t ID0,ID1,ID2,ID3;//ID0,1,2:delivery file //ID3:heart-beat measurement
	struct thread_para threadpara;
	struct thread_para threadpara0;
	struct thread_para threadpara1;
	struct thread_para threadpara2;

    /* start heart-beat measurement */
	if (argc != 3) {
		usage(argv[0]);
	}
	strcpy(threadpara.ip, argv[1]);
	strcpy(threadpara.filename, argv[2]);

	ret = pthread_create(&ID3, NULL, (void *)thread3, &threadpara);
	if (ret !=0 ){
		printf("create heart-beat measurement error！\n");
		exit(1);
	}

    /* input the delivery file */
	printf("Please input delivery file\n");
	gets(threadpara0.filename);
	strcpy(threadpara1.filename, threadpara0.filename);
	strcpy(threadpara2.filename, threadpara0.filename);
	strcpy(threadpara0.ip, argv[1]);
	strcpy(threadpara1.ip, argv[1]);
	strcpy(threadpara2.ip, argv[1]);
	printf("Please input required delivery time(1.123456)\n");
	gets(dt);
	printf("Please input required packet loss rate(0.023456)\n");
	gets(pl);

    /* kill heart-beat measurement */
	res = pthread_cancel(ID3);
	printf("kill thread3(heart-beat measurement), resID:%d\n", res);
	pthread_join(ID3,NULL);

    /* evaluate user's requirement and network conditions */
	deliverytime=atof(dt);
	plr=atof(pl);
//	printf("delivery time:%f PLR:%f\n", deliverytime, plr);
	printf("heart-beat measurement 2:%ld %ld %f\n", fbtimestemps, fbtimestempm, fbPLR);
	if( (deliverytime*1000000)<(fbtimestemps*1000000+fbtimestempm) || plr<fbPLR ){
		printf("can not meet user's requirement\n");
		exit(0);
	}
    /* assign the usage of sockets */
	threadpara0.start = 1;
	threadpara0.end = 236;
	threadpara1.start = 237;
	threadpara1.end = 472;
	threadpara2.start = 473;
	threadpara2.end = 900;
    /* start transfer the file, udp */
	ret = pthread_create(&ID0, NULL, (void *)thread0, &threadpara0);
	if (ret !=0 ){
		printf("create udp-thread0 error！\n");
		exit(1);
	}
    /* start transfer the file, tcp */
	ret = pthread_create(&ID1, NULL, (void *)thread1, &threadpara1);
	if (ret !=0 ){
		printf("create tcp-thread1 error！\n");
		exit(1);
	}
    /* start transfer the file, sctp */
	ret = pthread_create(&ID2, NULL, (void *)thread2, &threadpara2);
	if (ret !=0 ){
		printf("create sctp-thread2 error！\n");
		exit(1);
	}
	pthread_join(ID0,NULL);
	pthread_join(ID1,NULL);
	pthread_join(ID2,NULL);
	printf("delivery file Send Finish\n");

    /* feedback tcp */
	char feedback_recv[12];
	struct feedback *feedback = (struct feedback*)feedback_recv;//s2
	struct sockaddr_in serv_addr;
	struct sockaddr_in clie_addr;
	int clie_addr_len, feedbacktcp_id, feedbacklink_id, recv_len;

	clie_addr_len = sizeof(clie_addr);
	if ((feedbacktcp_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Create feedback TCP socket failed\n");
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(FTCPPORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(feedbacktcp_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
		perror("Bind TCP socket failed 2\n");
		exit(0);
	}
	if (-1 == listen(feedbacktcp_id, 10)) {
		perror("Listen TCP socket failed\n");
		exit(0);
	}
	clie_addr_len = sizeof(clie_addr);
	feedbacklink_id = accept(feedbacktcp_id, (struct sockaddr *)&clie_addr, &clie_addr_len);
	if (-1 == feedbacklink_id) {
		perror("Accept socket failed\n");
		exit(0);
	}
	while( recv_len = recv(feedbacklink_id, feedback_recv, 12, 0) ){
		printf("delivery time:%ld %ld PLR:%f\n", feedback->timestemps, feedback->timestempm, feedback->PLR);
	}
	printf("Finish receive feedback tcp\n");
	close(feedbacklink_id);
	close(feedbacktcp_id);
    /* feedback finish */

	return 0;
}
