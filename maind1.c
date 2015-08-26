//----------------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <netinet/sctp.h>
#include <fcntl.h>
#include <pthread.h>

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

//global variables for sender address
char senderaddr[16];
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
//----------------------------------------------------------------------------------

//help
void usage(char *command)
{
	printf("usage :%s filename\n", command);
	exit(0);
}
//----------------------------------------------------------------------------------

//heart-beat measurement thread
void thread0(void)
{
    /* kill thread0 */
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

	struct sockaddr_in serv_addr;
	struct sockaddr_in clie_addr;
	int udpsock_id, tcpsock_id, link_id, max, sctpsock_id, connSock, flags, write_len;
	char data_recvu[UDPDATA];
	char data_recvt[TCPDATA];
	char tcp_buf[TCPDATA];
	char data_recvs[SCTPDATA];
	int i, recvtn=0;
	int recv_len;
	int clie_addr_len;
	fd_set fd;
	fd_set variablefd;
	int tcpfinish=0, udpfinish=0, sctpfinish=0;
	unsigned short typeenddatalen;
	float PLR, udppackets, receivedudp;//s2
	float tcppackets, sctppackets;//s2
	long int mintimes;//experiment1
	long int mintimem;//experiment1
	long int firststss;//experiment1
	long int firststsm;//experiment1
	long int firstttss;//experiment1
	long int firstttsm;//experiment1
	long int firstutss;//experiment1
	long int firstutsm;//experiment1
	int firsts;//experiment1
	int firstt;//experiment1
	int firstu;//experiment1

    /* get sender address */
	char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage senderaddru;
	socklen_t senderlen;
	senderlen = sizeof senderaddru;

	void *get_in_addr(struct sockaddr *sa){	//get sockaddr,ipv4 or ipv6
		if (sa->sa_family == AF_INET) {
			return &(((struct sockaddr_in*)sa)->sin_addr);
		}
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
		}
//------------------------------------------

	struct sctp_initmsg initmsg;
	struct sctp_sndrcvinfo sndrcvinfo;

	struct SLA_header *headeru = (struct SLA_header *)data_recvu;
	struct SLA_data *datau = (struct SLA_data *)(data_recvu + sizeof(struct SLA_header));
	
	struct SLA_header *headert = (struct SLA_header*)data_recvt;
	struct SLA_data *datat = (struct SLA_data*)(data_recvt + sizeof(struct SLA_header));
	
	struct SLA_header *headers = (struct SLA_header*)data_recvs;
	struct SLA_data *datas = (struct SLA_data*)(data_recvs + sizeof(struct SLA_header));
	struct  timeval    tv;//experiment1
	struct  timezone   tz;//experiment1
//------------------------------------------

	int ret, on=1;//enable address reuse

    /* start heart-beat measurement */
	while(1){
		memset(data_recvu,0,sizeof data_recvu);
		memset(data_recvt,0,sizeof data_recvt);
		memset(data_recvs,0,sizeof data_recvs);
		firsts=0;//experiment1
		firstt=0;//experiment1
		firstu=0;//experiment1
		receivedudp=0;

    /* tcp */
		if ((tcpsock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("Create TCP socket failed\n");
			exit(0);
		}

		ret = setsockopt(tcpsock_id, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));//enable address reuse

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(TCPPORTm);
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(tcpsock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
			perror("Bind TCP socket failed\n");
			exit(0);
		}
		if (-1 == listen(tcpsock_id, 10)) {
			perror("Listen TCP socket failed\n");
			exit(0);
		}
//---------------------------------

    /* udp */
		if ((udpsock_id = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
			perror("Create UDP socket failed\n");
			exit(0);
		}

		ret = setsockopt(udpsock_id, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));//enable address reuse

		memset(&serv_addr,0,sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(UDPPORTm);
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(udpsock_id,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0 ) {
			perror("Bind UDP socket faild\n");
			exit(0);
		}
//---------------------------------

    /* sctp */
		if ((sctpsock_id = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP )) < 0) {
			perror("Create SCTP socket failed\n");
			exit(0);
		}

		ret = setsockopt(sctpsock_id, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));//enable address reuse

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(SCTPPORTm);
		serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
		if (bind( sctpsock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr) ) < 0 ) {
			perror("Bind SCTP socket failed\n");
			exit(0);
		}
		memset( &initmsg, 0, sizeof(initmsg) );
		initmsg.sinit_num_ostreams = 5;
		initmsg.sinit_max_instreams = 5;
		initmsg.sinit_max_attempts = 4;
		setsockopt( sctpsock_id, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg) );
		if (-1 == listen(sctpsock_id, 5)) {
			perror("Listen SCTP socket failed\n");
			exit(0);
		}
//---------------------------------

		FD_ZERO(&variablefd);
		FD_ZERO(&fd);
		FD_SET(tcpsock_id,&variablefd);
		FD_SET(udpsock_id,&variablefd);
		FD_SET(sctpsock_id,&variablefd);
		tcpfinish=0;
		udpfinish=0;
		sctpfinish=0;
	
		max = (tcpsock_id > udpsock_id ? tcpsock_id : udpsock_id);
		max = (max > sctpsock_id ? max : sctpsock_id);

		clie_addr_len = sizeof(clie_addr);
		while (1){
			if( (tcpfinish==1) && (udpfinish==1) && (sctpfinish==1) ){
				break;		
			}
		
			fd = variablefd;
			if (select(max+1,&fd,NULL,NULL,NULL)<0) {
				perror("select problem");
				exit(1);
			}
//---------------------------------

			if (FD_ISSET(sctpsock_id,&fd) ){
				connSock = accept( sctpsock_id, (struct sockaddr *)NULL, (int *)NULL );
				if (-1 == connSock) {
					perror("Accept socket failed\n");
					exit(0);
				}
				FD_SET(connSock,&variablefd);
				if (connSock > max){
				max=connSock;
				}
			}
			if (FD_ISSET(connSock,&fd) ){
				recv_len = sctp_recvmsg( connSock, data_recvs, SCTPDATA, (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags);
				if(recv_len == 0){
					printf("Finish receive sctp\n");
					sctpfinish=1;
					close(connSock);
					FD_CLR(connSock,&variablefd);
				}else if(recv_len < 0){
					printf("recv_len < 0!\n");
					typeenddatalen = ((headers->typeenddatalen1 & 0xFF)<< 8) | (headers->datalen2 & 0xFF);
				}else{
					typeenddatalen = ((headers->typeenddatalen1 & 0xFF)<< 8) | (headers->datalen2 & 0xFF);
					if ( firsts == 0 ){//experiment1
						firststss=ntohl(headers->timestemps);//experiment1
						firststsm=ntohl(headers->timestempm);//experiment1
						firsts=1;//experiment1
					}//experiment1
					sctppackets=headers->socketseq;
				}
				memset(data_recvs,0,sizeof data_recvs);
			}
//---------------------------------

			if (FD_ISSET(tcpsock_id,&fd) ){//link_id//tcpsock_id
				clie_addr_len = sizeof(clie_addr);
				link_id = accept(tcpsock_id, (struct sockaddr *)&clie_addr, &clie_addr_len);
				if (-1 == link_id) {
					perror("Accept socket failed\n");
					exit(0);
				}
				FD_SET(link_id,&variablefd);
				if (link_id > max){
				max=link_id;
				}
			}
			if (FD_ISSET(link_id,&fd) ){
				recv_len = recv(link_id, tcp_buf, TCPDATA, 0);
				if(recv_len == 0){
					typeenddatalen = ((headert->typeenddatalen1 & 0xFF)<< 8) | (headert->datalen2 & 0xFF);
					printf("Finish receive tcp\n");
					tcpfinish=1;
					close(link_id);
					FD_CLR(link_id,&variablefd);
				}else if(recv_len < 0){
					printf("Recieve Data From Server Failed!\n");
					break;
				}else{
					for(i = 0; i < recv_len; i++){
						data_recvt[recvtn] = tcp_buf[i];
						recvtn++;
						if (recvtn == TCPDATA){
							recvtn = 0;
							typeenddatalen = ((headert->typeenddatalen1 & 0xFF)<< 8) | (headert->datalen2 & 0xFF);
							if ( firstt == 0 ){//experiment1
								firstttss=ntohl(headert->timestemps);//experiment1
								firstttsm=ntohl(headert->timestempm);//experiment1
								firstt=1;//experiment1
							}//experiment1
							tcppackets=headert->socketseq;
							memset(data_recvt,0,sizeof data_recvt);
						}
					}
				memset(tcp_buf,0,sizeof tcp_buf);
				}
			}
//---------------------------------

			if (FD_ISSET(udpsock_id,&fd)){
				if (udpfinish==0){
					recv_len = recvfrom(udpsock_id, data_recvu, UDPDATA, 0,(struct sockaddr *)&senderaddru, &senderlen);
					if(recv_len < 0) {
						printf("Recieve data from client failed!\n");
						break;
					}
					typeenddatalen = ((headeru->typeenddatalen1 & 0xFF)<< 8) | (headeru->datalen2 & 0xFF);
					if ( firstu == 0 ){//experiment1
						firstutss=ntohl(headeru->timestemps);//experiment1
						firstutsm=ntohl(headeru->timestempm);//experiment1
						firstu=1;//experiment1
					}//experiment1
					if ( headeru->typeenddatalen1 >= 32){
						printf("Finish receive udp\n");
						udpfinish=1;
						udppackets=headeru->socketseq;
						close(udpsock_id);
						FD_CLR(udpsock_id,&variablefd);
					}
					receivedudp++;
					memset(data_recvu,0,sizeof data_recvu);
				}
			}
//---------------------------------

		}
		printf("Finish receive once heart-beat\n");
		close(tcpsock_id);
		close(sctpsock_id);

		gettimeofday(&tv,&tz);//experiment1
		if (firstttss < firstutss){//experiment1
			mintimes=firstttss;//experiment1
			mintimem=firstttsm;//experiment1
		}else if (firstutss < firstttss){//experiment1
			mintimes=firstutss;//experiment1
			mintimem=firstutsm;//experiment1
		}else if(firstttsm < firstutsm){//experiment1
			mintimes=firstttss;//experiment1
			mintimem=firstttsm;//experiment1
		}else{//experiment1
			mintimes=firstutss;//experiment1
			mintimem=firstutsm;//experiment1
		}//experiment1
		if (firststss < mintimes){//experiment1
			mintimes=firststss;//experiment1
			mintimem=firststsm;//experiment1
		}else if (mintimes < firststss){//experiment1
		}else if(firststsm < mintimem){//experiment1
			mintimem=firststsm;//experiment1
		}else{//experiment1
		}//experiment1
//---------------------------------

    /* feedback */
		int feedbacktcp_id;//s2
		int send_len;
		int i_ret;
		char feedback_send[12];

		struct feedback *feedback = (struct feedback*)feedback_send;//s2
		memset(feedback_send,0,sizeof feedback_send);//s2

    /* create the socket */
		if ((feedbacktcp_id = socket(AF_INET,SOCK_STREAM,0)) < 0) {
			perror("feedback TCP Create socket failed\n");
			exit(0);
		}
    
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(FTCPPORTm);
		strcpy(senderaddr,inet_ntop(senderaddru.ss_family,get_in_addr((struct sockaddr *)&senderaddru),s,sizeof s));//get sender addr
		inet_pton(AF_INET, senderaddr, &serv_addr.sin_addr);
  
    /* connect the server */
		i_ret = connect(feedbacktcp_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
		if (-1 == i_ret) {
			printf("feedback TCP Connect socket failed\n");
			return -1;
		}

		PLR=(udppackets-1-(receivedudp-1))/(udppackets-1+tcppackets+sctppackets);//udp minus the last one, ending packet
		feedback->PLR = PLR;
		if (tv.tv_sec > mintimes && tv.tv_usec < mintimem){
			feedback->timestemps = tv.tv_sec-1-mintimes;
			feedback->timestempm = tv.tv_usec+1000000-mintimem;
		}else{
			feedback->timestemps = tv.tv_sec-mintimes;
			feedback->timestempm = tv.tv_usec-mintimem;
		}
		send_len = send(feedbacktcp_id, feedback_send, 12, 0);
			if ( send_len < 0 ) {
				perror("TCP Send file failed\n");
				exit(0);
			}
		memset(feedback_send,0,sizeof feedback_send);
		close(feedbacktcp_id);
		printf("feedback TCP Socket Send Finish\n");
		sleep(3);
//---------------------------------
	}
//------------------------------------------
	return 0;
}
//----------------------------------------------------------------------------------

int main(int argc,char **argv){
	
	struct sockaddr_in serv_addr;
	struct sockaddr_in clie_addr;
	int udpsock_id, tcpsock_id, link_id, max, sctpsock_id, connSock, flags, write_len;
	char data_recvu[UDPDATA];
	char data_recvt[TCPDATA];
	char tcp_buf[TCPDATA];
	char data_recvs[SCTPDATA];
	int i, recvtn=0;
	int recv_len;
	int write_leng;
	int clie_addr_len;
	fd_set fd;
	fd_set variablefd;
	int tcpfinish=0, udpfinish=0, sctpfinish=0;
	FILE *fp;
	unsigned short typeenddatalen;
	float PLR, udppackets, receivedudp=0;//s2
	float tcppackets, sctppackets;//s2
	long int mintimes;//experiment1
	long int mintimem;//experiment1
	long int firststss;//experiment1
	long int firststsm;//experiment1
	long int firstttss;//experiment1
	long int firstttsm;//experiment1
	long int firstutss;//experiment1
	long int firstutsm;//experiment1
	int firsts=0;//experiment1
	int firstt=0;//experiment1
	int firstu=0;//experiment1

	static char filebuf[196606][1456];//file buffer 65535*3+1
	static int filelen[196606];//lenth of each packet

	pthread_t ID0;
	int ret, res;
	ret=pthread_create(&ID0,NULL,(void *) thread0,NULL);
	if(ret!=0){
		printf ("Create pthread error!\n");
		exit (1);
	}

	struct sctp_initmsg initmsg;
	struct sctp_sndrcvinfo sndrcvinfo;

	struct SLA_header *headeru = (struct SLA_header *)data_recvu;
	struct SLA_data *datau = (struct SLA_data *)(data_recvu + sizeof(struct SLA_header));
	
	struct SLA_header *headert = (struct SLA_header*)data_recvt;
	struct SLA_data *datat = (struct SLA_data*)(data_recvt + sizeof(struct SLA_header));
	
	struct SLA_header *headers = (struct SLA_header*)data_recvs;
	struct SLA_data *datas = (struct SLA_data*)(data_recvs + sizeof(struct SLA_header));
	struct  timeval    tv;//experiment1
	struct  timezone   tz;//experiment1

	memset(data_recvu,0,sizeof data_recvu);
	memset(data_recvt,0,sizeof data_recvt);
	memset(data_recvs,0,sizeof data_recvs);
//----------------------------------------------------------------------------------

	if (argc != 2) {
		usage(argv[0]);
	}	
//----------------------------------------------------------------------------------

    /* tcp */
	if ((tcpsock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Create TCP socket failed\n");
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(TCPPORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(tcpsock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
		perror("Bind TCP socket failed\n");
		exit(0);
	}
	if (-1 == listen(tcpsock_id, 10)) {
		perror("Listen TCP socket failed\n");
		exit(0);
	}
//----------------------------------------------------------------------------------

    /* udp */
	if ((udpsock_id = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		perror("Create UDP socket failed\n");
		exit(0);
	}
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(UDPPORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(udpsock_id,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0 ) {
		perror("Bind UDP socket faild\n");
		exit(0);
	}
//----------------------------------------------------------------------------------

    /* sctp */
	if ((sctpsock_id = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP )) < 0) {
		perror("Create SCTP socket failed\n");
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SCTPPORT);
	serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
	if (bind( sctpsock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr) ) < 0 ) {
		perror("Bind SCTP socket failed\n");
		exit(0);
	}
	memset( &initmsg, 0, sizeof(initmsg) );
	initmsg.sinit_num_ostreams = 5;
	initmsg.sinit_max_instreams = 5;
	initmsg.sinit_max_attempts = 4;
	setsockopt( sctpsock_id, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg) );
	if (-1 == listen(sctpsock_id, 5)) {
		perror("Listen SCTP socket failed\n");
		exit(0);
	}
//----------------------------------------------------------------------------------

	FD_ZERO(&variablefd);
	FD_ZERO(&fd);
	FD_SET(tcpsock_id,&variablefd);
	FD_SET(udpsock_id,&variablefd);
	FD_SET(sctpsock_id,&variablefd);
	
	max = (tcpsock_id > udpsock_id ? tcpsock_id : udpsock_id);
	max = (max > sctpsock_id ? max : sctpsock_id);
	clie_addr_len = sizeof(clie_addr);
	while (1){
		if( (tcpfinish==1) && (udpfinish==1) && (sctpfinish==1) ){
			break;		
		}
		
		fd = variablefd;
		if (select(max+1,&fd,NULL,NULL,NULL)<0) {
			perror("select problem");
			exit(1);
		}
//-----------------------------------------------------------

		if (FD_ISSET(sctpsock_id,&fd) ){//connSock//sctpsock_id
			connSock = accept( sctpsock_id, (struct sockaddr *)NULL, (int *)NULL );
			if (-1 == connSock) {
				perror("Accept socket failed\n");
				exit(0);
			}
			FD_SET(connSock,&variablefd);
			if (connSock > max){
			max=connSock;
			}
		}
		if (FD_ISSET(connSock,&fd) ){
			recv_len = sctp_recvmsg( connSock, data_recvs, SCTPDATA, (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags);
			if(recv_len == 0){
				printf("Finish receiver sctp\n");
				sctpfinish=1;
				close(connSock);
				FD_CLR(connSock,&variablefd);
			}else if(recv_len < 0){
				printf("recv_len < 0!\n");
				typeenddatalen = ((headers->typeenddatalen1 & 0xFF)<< 8) | (headers->datalen2 & 0xFF);
				strcpy(filebuf[headers->commonseq],datas->data);
				filelen[headers->commonseq]=typeenddatalen%8192;
			}else{
				typeenddatalen = ((headers->typeenddatalen1 & 0xFF)<< 8) | (headers->datalen2 & 0xFF);

				if ( firsts == 0 ){//experiment1
					firststss=ntohl(headers->timestemps);//experiment1
					firststsm=ntohl(headers->timestempm);//experiment1
					firsts=1;//experiment1
				}//experiment1
				sctppackets=headers->socketseq;
				strcpy(filebuf[headers->commonseq],datas->data);
				filelen[headers->commonseq]=typeenddatalen%8192;
			}
			memset(data_recvs,0,sizeof data_recvs);
		}
//-----------------------------------------------------------

		if (FD_ISSET(tcpsock_id,&fd) ){//link_id//tcpsock_id
			clie_addr_len = sizeof(clie_addr);
			link_id = accept(tcpsock_id, (struct sockaddr *)&clie_addr, &clie_addr_len);
			if (-1 == link_id) {
				perror("Accept socket failed\n");
				exit(0);
			}
			FD_SET(link_id,&variablefd);
			if (link_id > max){
			max=link_id;
			}
		}
		if (FD_ISSET(link_id,&fd) ){
			recv_len = recv(link_id, tcp_buf, TCPDATA, 0);
			if(recv_len == 0){
				typeenddatalen = ((headert->typeenddatalen1 & 0xFF)<< 8) | (headert->datalen2 & 0xFF);
				strcpy(filebuf[headert->commonseq],datat->data);
				filelen[headert->commonseq]=typeenddatalen%8192;
				printf("Finish receiver tcp\n");
				tcpfinish=1;
				close(link_id);
				FD_CLR(link_id,&variablefd);
			}else if(recv_len < 0){
				printf("Recieve Data From Server Failed!\n");
				break;
			}else{
				for(i = 0; i < recv_len; i++){
					data_recvt[recvtn] = tcp_buf[i];
					recvtn++;
					if (recvtn == TCPDATA){
						recvtn = 0;
						typeenddatalen = ((headert->typeenddatalen1 & 0xFF)<< 8) | (headert->datalen2 & 0xFF);
						if ( firstt == 0 ){//experiment1
							firstttss=ntohl(headert->timestemps);//experiment1
							firstttsm=ntohl(headert->timestempm);//experiment1
							firstt=1;//experiment1
						}//experiment1
						tcppackets=headert->socketseq;
						strcpy(filebuf[headert->commonseq],datat->data);
						filelen[headert->commonseq]=typeenddatalen%8192;
						memset(data_recvt,0,sizeof data_recvt);
					}
				}
			}
		}
//-----------------------------------------------------------

		if (FD_ISSET(udpsock_id,&fd)){
			if (udpfinish==0){
				recv_len = recvfrom(udpsock_id, data_recvu, UDPDATA, 0,(struct sockaddr *)&clie_addr, &clie_addr_len);
				if(recv_len < 0) {
					printf("Recieve data from client failed!\n");
					break;
				}
				typeenddatalen = ((headeru->typeenddatalen1 & 0xFF)<< 8) | (headeru->datalen2 & 0xFF);
				
				if ( headeru->typeenddatalen1 >= 32){
					printf("Finish receiver udp\n");
					udpfinish=1;
					udppackets=headeru->socketseq;
					close(udpsock_id);
					FD_CLR(udpsock_id,&variablefd);
				}else{
					if ( firstu == 0 ){//experiment1
						firstutss=ntohl(headeru->timestemps);//experiment1
						firstutsm=ntohl(headeru->timestempm);//experiment1
						firstu=1;//experiment1
					}//experiment1
					strcpy(filebuf[headeru->commonseq],datau->data);
					filelen[headeru->commonseq]=recv_len-16;
				}
				receivedudp++;
				memset(data_recvu,0,sizeof data_recvu);
			}
		}
//-----------------------------------------------------------
	}
	printf("Finish receive delivery file 1\n");
	close(tcpsock_id);
	close(sctpsock_id);
//-----------------------------------------------------------

    /* delivery file re-assembly */
	if ((fp = fopen(argv[1], "w")) == NULL) {
		perror("Creat file failed");
		exit(0);
	}
	for(i=1;i<=udppackets-1+tcppackets+sctppackets;i++){
		int write_length = fwrite(filebuf[i], sizeof(char), filelen[i], fp);
		if (write_length < filelen[i]){
			printf("File write failed\n");
			break;
		}
	}
	fclose(fp);
//-----------------------------------------------------------

    /* get delivery start and end time */
	gettimeofday(&tv,&tz);//experiment1
	if (firstttss < firstutss){//experiment1
		mintimes=firstttss;//experiment1
		mintimem=firstttsm;//experiment1
	}else if (firstutss < firstttss){//experiment1
		mintimes=firstutss;//experiment1
		mintimem=firstutsm;//experiment1
	}else if(firstttsm < firstutsm){//experiment1
		mintimes=firstttss;//experiment1
		mintimem=firstttsm;//experiment1
	}else{//experiment1
		mintimes=firstutss;//experiment1
		mintimem=firstutsm;//experiment1
	}//experiment1
	if (firststss < mintimes){//experiment1
		mintimes=firststss;//experiment1
		mintimem=firststsm;//experiment1
	}else if (mintimes < firststss){//experiment1
	}else if(firststsm < mintimem){//experiment1
		mintimem=firststsm;//experiment1
	}else{//experiment1
	}//experiment1
//-----------------------------------------------------------

    /* feedback */
	int feedbacktcp_id;//s2
	int send_len;
	int i_ret;
	char feedback_send[12];

	struct feedback *feedback = (struct feedback*)feedback_send;//s2
	memset(feedback_send,0,sizeof feedback_send);//s2

    /* create the socket */
	if ((feedbacktcp_id = socket(AF_INET,SOCK_STREAM,0)) < 0) {
		perror("feedback TCP Create socket failed\n");
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(FTCPPORT);
	inet_pton(AF_INET, senderaddr, &serv_addr.sin_addr);
  
    /* connect the server */
	i_ret = connect(feedbacktcp_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	if (-1 == i_ret) {
		printf("feedback TCP Connect socket failed\n");
		return -1;
	}

	PLR=(udppackets-1-(receivedudp-1))/(udppackets-1+tcppackets+sctppackets);//minus the last one, ending packet
	printf("udppackets:%f tcppackets:%f sctppackets:%f\n",udppackets-1,tcppackets,sctppackets);
	feedback->PLR = PLR;
	if (tv.tv_sec > mintimes && tv.tv_usec < mintimem){
		feedback->timestemps = tv.tv_sec-1-mintimes;
		feedback->timestempm = tv.tv_usec+1000000-mintimem;
	}else{
		feedback->timestemps = tv.tv_sec-mintimes;
		feedback->timestempm = tv.tv_usec-mintimem;
	}

	send_len = send(feedbacktcp_id, feedback_send, 12, 0);
		if ( send_len < 0 ) {
			perror("TCP Send file failed\n");
			exit(0);
		}
	memset(feedback_send,0,sizeof feedback_send);
	close(feedbacktcp_id);
	printf("feedback TCP Socket Send Finish\n");
//-----------------------------------------------------------

    /* kill heart-beat measurement */
	res = pthread_cancel(ID0);
	printf("kill thread0(heart-beat measurement), resID:%d\n", res);
	pthread_join(ID0,NULL);
//------------------------------------------

	return 0;
}
