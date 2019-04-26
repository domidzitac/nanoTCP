/*
 * Nabil Rahiman
 * NYU Abudhabi
 * email: nr83@nyu.edu
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  400 //milli second

/* Shared variables between functions*/
int window_size = 10;
int last_ack = 0; //The number of the last acked segment
int sockfd; //Socket of the reciever
int serverlen; // Size of serveraddr
int total_packets; //The total number of packets that will be sent
FILE *fp; //FP of our file
struct sockaddr_in serveraddr; //Server address
struct itimerval timer; //Timer for our timeouts.
tcp_packet *sndpkt; //The packet we send
tcp_packet *recvpkt; //The packet we recieve
sigset_t sigmask; //Timeout signals

void send_packets(int start, int end);
tcp_packet * make_send_packet(int index);
void start_timer();
void resend_packets(int sig);

/* Resent packets between last_ack and our windowsize */
void resend_packets(int sig) {
   if (sig == SIGALRM) {
    VLOG(INFO, "Timout happened sending %d to %d", last_ack, last_ack+window_size-1);
		send_packets(last_ack, last_ack+window_size-1);
		start_timer();
   }
}

void start_timer() {
   sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
   setitimer(ITIMER_REAL, &timer, NULL);
}

void stop_timer() {
   sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

/*
* init_timer: Initialize timeer
* delay: delay in milli seconds
* sig_handler: signal handler function for resending unacknoledge packets
*/
void init_timer(int delay, void (*sig_handler)(int))
{
   signal(SIGALRM, resend_packets);
   timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
   timer.it_interval.tv_usec = (delay % 1000) * 1000;
   timer.it_value.tv_sec = delay / 1000;       // sets an initial value
   timer.it_value.tv_usec = (delay % 1000) * 1000;

   sigemptyset(&sigmask);
   sigaddset(&sigmask, SIGALRM);
}

/*
read the packet byte next_seqno from our file and return sndpkt to be sent
*/
tcp_packet * make_send_packet(int index){
	char buffer[DATA_SIZE]; //Buffer after reading packet number packet.
	tcp_packet *sndpkt; //Create the packet.

	fseek(fp, index * DATA_SIZE, SEEK_SET); //Seek to the correct position
	size_t sz = fread(buffer, 1, DATA_SIZE, fp); //Read the next packet.
	//printf("sz=%d\n",sz);
   sndpkt = make_packet(sz);
   memcpy(sndpkt->data, buffer, sz);
   sndpkt->hdr.seqno = index * DATA_SIZE + sz;
   return(sndpkt);
}


/*
  * Function to send a packet of sequence from start:end.
  * If start == -1, then send terminating packet.
*/
void send_packets(int start, int end){
	// make sure end < max_size
	if (start == -1){
		tcp_packet * sndpkt = make_packet(0);
		if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                   ( const struct sockaddr *)&serveraddr, serverlen) < 0)
       {
           error("sendto");
       }
       return;
	}
	int serverlen = sizeof(serveraddr);
	if (end >= total_packets) end = total_packets - 1;
	int i;
	for (i = start; i <= end; i ++){
		/* Create our snpkt */
		tcp_packet * sndpkt = make_send_packet(i);
       /*
        * If the sendto is called for the first time, the system will
        * will assign a random port number so that server can send its
        * response to the src port.
        */
       if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                   ( const struct sockaddr *)&serveraddr, serverlen) < 0)
       {
           error("sendto");
       }
	}
}


int main (int argc, char **argv)
{
   int portno;
   char *hostname;
   char buffer[DATA_SIZE];

   /* check command line arguments */
   if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
       exit(0);
   }
   hostname = argv[1];
   portno = atoi(argv[2]);
   fp = fopen(argv[3], "r");
   fseek(fp, 0L, SEEK_END);
	long sz = ftell(fp);
	total_packets = sz / DATA_SIZE;
	if (sz % DATA_SIZE != 0) total_packets ++;
   if (fp == NULL) {
       error(argv[3]);
   }

   /* socket: create the socket */
   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0)
       error("ERROR opening socket");


   /* initialize server server details */
   bzero((char *) &serveraddr, sizeof(serveraddr));
   serverlen = sizeof(serveraddr);

   /* covert host into network byte order */
   if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
       fprintf(stderr,"ERROR, invalid host %s\n", hostname);
       exit(0);
   }

   /* build the server's Internet address */
   serveraddr.sin_family = AF_INET;
   serveraddr.sin_port = htons(portno);

   assert(MSS_SIZE - TCP_HDR_SIZE > 0);

   //Stop and wait protocol

   init_timer(RETRY, resend_packets);
   send_packets(0, window_size-1);
   start_timer();
   while (1)
   {
       if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
           (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
		{
		    error("recvfrom");
		}
		recvpkt = (tcp_packet *)buffer;
		int ackno = recvpkt->hdr.ackno;
		//if (recvpkt->hdr.ackno % DATA_SIZE != 0) ackno ++;
		printf("total=%d ackno=%d lastack=%d\n",total_packets, ackno, last_ack);
		if (ackno > last_ack){
			send_packets(last_ack+window_size,ackno+window_size-1);
			stop_timer();
			start_timer();
			last_ack = ackno;
			if (ackno >= total_packets){
				send_packets(-1,-1);
				printf("Completed transfer\n");
				break;
			}
		}
       // len = fread(buffer, 1, DATA_SIZE, fp);
       // if ( len <= 0)
       // {
       //     VLOG(INFO, "End Of File has been reached");
       //     sndpkt = make_packet(0);
       //     sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
       //             (const struct sockaddr *)&serveraddr, serverlen);
       //     break;
       // }
       // send_base = next_seqno;
       // next_seqno = send_base + len;
       // sndpkt = make_packet(len);
       // memcpy(sndpkt->data, buffer, len);
       // sndpkt->hdr.seqno = send_base;
       //Wait for ACK
       // do {

       //     VLOG(DEBUG, "Sending packet %d to %s",
       //             send_base, inet_ntoa(serveraddr.sin_addr));
       //     /*
       //      * If the sendto is called for the first time, the system will
       //      * will assign a random port number so that server can send its
       //      * response to the src port.
       //      */
       //     if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
       //                 ( const struct sockaddr *)&serveraddr, serverlen) < 0)
       //     {
       //         error("sendto");
       //     }

       //     start_timer();
       //     //ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
       //     //struct sockaddr *src_addr, socklen_t *addrlen);

       //     if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
       //                 (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
       //     {
       //         error("recvfrom");
       //     }

       //     //if ack > last _ needed _ack:


       //     recvpkt = (tcp_packet *)buffer;
       //     printf("%d \n", get_data_size(recvpkt));
       //     assert(get_data_size(recvpkt) <= DATA_SIZE);
       //     stop_timer();
       //     /*resend pack if dont recv ack */
       // } while(recvpkt->hdr.ackno != next_seqno);

       // free(sndpkt);
   }

   return 0;

}
