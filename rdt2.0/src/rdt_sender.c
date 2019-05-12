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
#include <math.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  400 //Retransmit timeout time
#define SLOW_START 0 //Flag 0 = SLOW_START 1 = CONGESTION_AVOIDANCE
#define CONGESTION_AVOIDANCE 1
int max_int(int a, int b){
  if(a>b){
    return a;
  }
  else {
    return b;
  }
}

double max_double(double a, double b){
  if(a>b){
    return a;
  }
  else {
    return b;
  }
}

/* Shared variables between functions*/
double window_size = 1;
int send_base = 0; //This is sendbase
int duplicate_ack = 0; //Number of duplicate acks recieved.
int sockfd; //Socket of the reciever
int serverlen; //Size of serveraddr
int mode = SLOW_START;
int last_sent=-1; //The last send segment
int total_packets; //The total number of packets that will be sent
double ssthresh = 64;
FILE *fp; //FP of our file
struct sockaddr_in serveraddr; //Server address
struct itimerval timer; //Timer for our timeouts.
tcp_packet *sndpkt; //The packet we send
tcp_packet *recvpkt; //The packet we recieve
sigset_t sigmask; //Timeout signals

//Function declarations
void send_packets();
void send_packets_end(int start, int end);
tcp_packet * make_send_packet(int index);
void start_timer();
void stop_timer();
void resend_packets(int sig);

/* Resend packets between send_base and our windowsize */
void resend_packets(int sig) {
   if (sig == SIGALRM) { //Timeout occurs
    ssthresh=max_double(window_size/2,2.0); //ssthresh gets cut in half.
    window_size=1;
    duplicate_ack=0;
    VLOG(INFO, "Timeout happened sending %d to %d",last_sent+1,send_base+(int)floor(window_size)-1);
		send_packets();
		//start_timer();
    last_sent = send_base - 1;
    mode=SLOW_START;
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
void init_timer(int delay, void (*sig_handler)(int)) {
   signal(SIGALRM, resend_packets);
   timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
   timer.it_interval.tv_usec = (delay % 1000) * 1000;
   timer.it_value.tv_sec = delay / 1000;       // sets an initial value
   timer.it_value.tv_usec = (delay % 1000) * 1000;
   sigemptyset(&sigmask);
   sigaddset(&sigmask, SIGALRM);
}

/*
  * Creates a sndpkt given the index of the packet we need to send.
  * Read the packet byte next_seqno from fp and save to sndpkt
  * Populates the headers with the correct byte-level segment number
*/
tcp_packet * make_send_packet(int index){
	char buffer[DATA_SIZE]; //Buffer after reading packet number packet.
	tcp_packet *sndpkt; //Create the packet.
	fseek(fp, index * DATA_SIZE, SEEK_SET); //Seek to the correct position
	size_t sz = fread(buffer, 1, DATA_SIZE, fp); //Read the data
   sndpkt = make_packet(sz); //Create our packet
   memcpy(sndpkt->data, buffer, sz); //Populate the data section with buffer
   sndpkt->hdr.seqno = index * DATA_SIZE; //Use byte-level sequence number

  last_sent=max_int(last_sent,index);

  return(sndpkt);
}

/*
  * Send a series of packets ranging from the start to end indexes.
  * If start == -1 then send a terminating packet.
*/
void send_packets_end(int start, int end){
    if (start == -1) {
    	tcp_packet * sndpkt = make_packet(0);
      VLOG(DEBUG, "SENDING END PACKET WITH SZ %d and %d", get_data_size(sndpkt), TCP_HDR_SIZE + get_data_size(sndpkt));
    	if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                     ( const struct sockaddr *)&serveraddr, serverlen) < 0){
             error("sendto");
      }
      return;
    }
    // make sure end < max_size
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
                   ( const struct sockaddr *)&serveraddr, serverlen) < 0){
           error("sendto");
       }
    }
}

//Send as many packets as our window allows
void send_packets(){
  send_packets_end(last_sent+1,send_base+(int)floor(window_size)-1);
}


int main (int argc, char **argv) {
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
   /* Get the total size of our file */
   fseek(fp, 0L, SEEK_END);
   long sz = ftell(fp);
   total_packets = sz / DATA_SIZE;

   if (total_packets == 0) {
   	printf("File is empty\n");
   	exit(0);
   }

   if (sz % DATA_SIZE != 0) total_packets ++; //Increment if we have partial pkt
   if (fp == NULL) {
       error(argv[3]);
   }

   /* socket: create the socket */
   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0){
     error("ERROR opening socket");
   }

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

   init_timer(RETRY, resend_packets);
   send_packets();
   start_timer();

   while (1){
    VLOG(DEBUG, "START WAITING TO RECEIVE");
     if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
         (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0){
	    error("recvfrom");
		}
    VLOG(DEBUG, "FINISH WAITING");
    stop_timer();
		recvpkt = (tcp_packet *)buffer;
		int ackno = recvpkt->hdr.ackno;
    VLOG(DEBUG, "windowsize=%f ssthresh=%f\n", window_size, ssthresh);
		VLOG(DEBUG, "total=%d ackno=%d lastack=%d\n",total_packets, ackno, send_base);

		if (ackno > send_base){ //If the recieved ack number is larger than our send_base
      VLOG(DEBUG, "IN ACKNO >");
      if (mode==SLOW_START){
        window_size+=(ackno-send_base);
        if(window_size>=ssthresh){
          mode=CONGESTION_AVOIDANCE;
        }
      }
      else {
        window_size+=(ackno-send_base)/window_size;
      }
			duplicate_ack=0;
      /* Transmit new packets between our old head and updated head*/
			send_base = ackno; //Update our send_base and hence our window location
      last_sent = max_int(last_sent, send_base - 1);
            //VLOG(DEBUG, "BEF SENDPKT");
      send_packets();
      //VLOG(DEBUG, "AFT SENDPKT >");

      start_timer();
      //VLOG(DEBUG, "AFT START_TIMER>");

      //printf("ackno %d total_packets %d\n", ackno, total_packets); CURSED LINE
			if (ackno >= total_packets){ //We have reached the end. Send terminating packet
				send_packets_end(-1,-1);
				VLOG(DEBUG, "Completed transfer\n");
				break;
			}
      //VLOG(DEBUG, "END OF ACK >"); // MAGIC LINE
		}
		else if (ackno==send_base){
      VLOG(DEBUG, "IN ACKNO ==");
        duplicate_ack ++;
        if (duplicate_ack >= 3){
          ssthresh=max_double(window_size/2,2);
          window_size = 1;
          last_sent = send_base - 1;
          send_packets();
          //start_timer();
          duplicate_ack = 0;
          mode=SLOW_START;
        }
			}

      //If this packet is the third duplicate ack
     //VLOG(DEBUG, "AT WHILE END");

   }

   return 0;

}
