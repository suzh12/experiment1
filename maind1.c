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
#include<sys/time.h>

#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#include <time.h>
#include <netinet/sctp.h>
#include <fcntl.h>

#define UDPDATA 1472
#define SLADATA 1456
#define UDPPORT 10000

#define TCPDATA 1472
#define SLAdata 1456
#define TCPPORT 10001

#define SCTPDATA 1472
#define SLAData 1456
#define SCTPPORT 10002
//----------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------
void usage(char *command)
{
	printf("usage :%s filename\n", command);
	exit(0);
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
	 /* Create the the file */
	if ((fp = fopen(argv[1], "w")) == NULL) {
		perror("Creat file failed");
		exit(0);
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

	printf("1 udpsid:%d tcpsid:%d sctpsock_id:%d link_id:%d connSock:%d\n", udpsock_id, tcpsock_id, sctpsock_id, link_id, connSock);
	FD_ZERO(&variablefd);
	FD_ZERO(&fd);
	FD_SET(tcpsock_id,&variablefd);
	FD_SET(udpsock_id,&variablefd);
	FD_SET(sctpsock_id,&variablefd);
	
	max = (tcpsock_id > udpsock_id ? tcpsock_id : udpsock_id);
	max = (max > sctpsock_id ? max : sctpsock_id);
	printf("max:%d\n",max);
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
		if (FD_ISSET(sctpsock_id,&fd) ){
			printf("2 udpsid:%d tcpsid:%d sctpsock_id:%d link_id:%d connSock:%d\n", udpsock_id, tcpsock_id, sctpsock_id, link_id, connSock);
			connSock = accept( sctpsock_id, (struct sockaddr *)NULL, (int *)NULL );
			if (-1 == connSock) {
				perror("Accept socket failed\n");
				exit(0);
			}
			FD_SET(connSock,&variablefd);
			if (connSock > max){
			max=connSock;
			}
			printf("3 udpsid:%d tcpsid:%d sctpsock_id:%d link_id:%d connSock:%d\n", udpsock_id, tcpsock_id, sctpsock_id, link_id, connSock);
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
				write_len = fwrite(datas->data, sizeof(char), typeenddatalen%8192, fp);
				if (write_len < typeenddatalen%8192) {
					printf("Write file failed\n");
					break;
				}
			}else{
				typeenddatalen = ((headers->typeenddatalen1 & 0xFF)<< 8) | (headers->datalen2 & 0xFF);

				if ( firsts == 0 ){//experiment1
					firststss=ntohl(headers->timestemps);//experiment1
					firststsm=ntohl(headers->timestempm);//experiment1
					firsts=1;//experiment1
				}//experiment1
				
				write_len = fwrite(datas->data, sizeof(char), typeenddatalen%8192, fp);
				if (write_len < typeenddatalen%8192) {
					printf("Write file failed\n");
					break;
				}
			}
			printf("4 sctpsock_id:%d connSock:%d write:%d\n", sctpsock_id, connSock, typeenddatalen%8192);
			memset(data_recvs,0,sizeof data_recvs);
		}
//-----------------------------------------------------------
		if (FD_ISSET(tcpsock_id,&fd) ){//link_id//tcpsock_id
			printf("5 udpsid:%d tcpsid:%d sctpsock_id:%d link_id:%d connSock:%d\n", udpsock_id, tcpsock_id, sctpsock_id, link_id, connSock);			
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
			printf("6 udpsid:%d tcpsid:%d sctpsock_id:%d link_id:%d connSock:%d\n", udpsock_id, tcpsock_id, sctpsock_id, link_id, connSock);
		}
		if (FD_ISSET(link_id,&fd) ){
			recv_len = recv(link_id, tcp_buf, TCPDATA, 0);
			if(recv_len == 0){
				typeenddatalen = ((headert->typeenddatalen1 & 0xFF)<< 8) | (headert->datalen2 & 0xFF);
		
				write_leng = fwrite(datat->data, sizeof(char), typeenddatalen%8192, fp);
				if (write_leng < typeenddatalen%8192) {
					printf("Write file failed\n");
					break;
				}
				printf("7 tcpsid:%d link_id:%d write:%d len:%d recvlen:%d\n", tcpsock_id, link_id, typeenddatalen%8192, typeenddatalen, recv_len);

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

						write_leng = fwrite(datat->data, sizeof(char), typeenddatalen%8192, fp);
						if (write_leng < typeenddatalen%8192) {
							printf("Write file failed\n");
							break;
						}
						printf("7 tcpsid:%d link_id:%d write:%d len:%d recvlen:%d\n", tcpsock_id, link_id, typeenddatalen%8192, typeenddatalen, recv_len);
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
				if ( firstu == 0 ){//experiment1
					firstutss=ntohl(headeru->timestemps);//experiment1
					firstutsm=ntohl(headeru->timestempm);//experiment1
					firstu=1;//experiment1
				}//experiment1

				int write_length = fwrite(datau->data, sizeof(char), recv_len-16, fp);
				printf("8 udpsid:%d write:%d\n", udpsock_id, recv_len-16);			
				if (write_length < recv_len-16){
					printf("File write failed\n");
					break;
				}
				if ( headeru->typeenddatalen1 >= 32){
					printf("Finish receiver udp\n");
					udpfinish=1;
					close(udpsock_id);
					FD_CLR(udpsock_id,&variablefd);
				}
				memset(data_recvu,0,sizeof data_recvu);
			}
		}
//-----------------------------------------------------------
	}
	printf("Finish receive\n");
	fclose(fp);
	close(tcpsock_id);
	close(sctpsock_id);

	printf("ts:%ld us:%ld ss:%ld\ntm:%ld um:%ld sm:%ld\n", firstttss, firstutss, firststss, firstttsm, firstutsm, firststsm);//experiment1
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
	printf("mt:%ld mm:%ld\n", mintimes, mintimem);//experiment1
	gettimeofday(&tv,&tz);//experiment1
	printf("ct:%ld cm:%ld\n", tv.tv_sec, tv.tv_usec);//experiment1
	if (tv.tv_sec > mintimes && tv.tv_usec < mintimem){//experiment1
		printf("delivery time:%ld.%ld\n", tv.tv_sec-1-mintimes, tv.tv_usec+1000000-mintimem);//experiment1
	}else{//experiment1
		printf("delivery time:%ld.%ld\n", tv.tv_sec-mintimes, tv.tv_usec-mintimem);//experiment1
	}//experiment1
	return 0;
}
