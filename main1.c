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
#include<sys/time.h>
#include <pthread.h>

#define UDPDATA 1472
#define SLADATA 1456
#define UDPPORT 10000

#define TCPDATA 1472
#define SLAdata 1456
#define TCPPORT 10001

#define SCTPDATA 1472
#define SLAData 1456
#define SCTPPORT 10002

int socknum=3;
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
//Thread para
struct thread_para
{
	char *ip[256];
	char *filename[256];
};
//----------------------------------------------------------------------------------
//help
void usage(char *command)
{
	printf("usage :%s ipaddr filename\n", command);
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
		perror("Open file failed\n");
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
	printf("udp,fn:%s ip:%s\n", threadpa->filename, threadpa->ip);
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
	header->commonseq = udpseq;
	gettimeofday(&tv,&tz);
	header->timestemps = htonl(tv.tv_sec);
	header->timestempm = htonl(tv.tv_usec);
	bzero(data->data, SLADATA);
	for (udplast=1;udplast<=20;udplast++) {
		send_len = send(udpsock_id, data_send, read_len+16, 0);
	}

	fclose(fp);
	printf("UDPsock:%d\n", udpsock_id);
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
		perror("Open file failed\n");
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
		printf("TCP Connect socket failed\n");
		return -1;
	}
	printf("tcp,fn:%s ip:%s\n", threadpa->filename, threadpa->ip);///
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
			bzero(data->data, SLAdata);
			memset(data_send,0,sizeof data_send);
		}
		bzero(data->data, SLAData);
		memset(data_send,0,sizeof data_send);
		commonseq=commonseq+1;
	}

	fclose(fp);
	printf("TCPsock:%d\n", tcpsock_id);
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
		perror("Open file failed\n");
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
	printf("sctp,fn:%s ip:%s\n", threadpa->filename, threadpa->ip);
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
	printf("SCTPsock:%d\n", sctpsock_id);
	close(sctpsock_id);
	printf("SCTP Socket Send Finish\n");
	return 0;
}
//----------------------------------------------------------------------------------

int main(int argc,char **argv)
{
	int ret;
	pthread_t ID0,ID1,ID2;
	struct thread_para threadpara;

	if (argc != 3) {
		usage(argv[0]);
	}
	strcpy(threadpara.ip, argv[1]);
	strcpy(threadpara.filename, argv[2]);

	ret = pthread_create(&ID0, NULL, (void *)thread0, &threadpara);
	if (ret !=0 ){
		printf("create udp-thread0 error！\n");
		exit(1);
	}
	ret = pthread_create(&ID1, NULL, (void *)thread1, &threadpara);
	if (ret !=0 ){
		printf("create tcp-thread1 error！\n");
		exit(1);
	}
	ret = pthread_create(&ID2, NULL, (void *)thread2, &threadpara);
	if (ret !=0 ){
		printf("create tcp-thread2 error！\n");
		exit(1);
	}
	pthread_join(ID0,NULL);
	pthread_join(ID1,NULL);
	pthread_join(ID2,NULL);
	printf("Send Finish\n");
	return 0;
}
