/*
 * Reference code:
 * Nabil Rahiman
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
 #include <sys/time.h>
 #include <assert.h>
 #include <time.h>

 #include "common.h"
 #include "packet.h"

 tcp_packet *recvpkt;
 tcp_packet *sndpkt;
/* This window size should be handshaked with the server */
/* Advertised window */
 int window_size = 1000000;

 int main(int argc, char **argv) {
     int sockfd; /* socket */
     int portno; /* port to listen on */
     int clientlen; /* byte size of client's address */
     struct sockaddr_in serveraddr; /* server's addr */
     struct sockaddr_in clientaddr; /* client addr */
     int optval; /* flag value for setsockopt */
     FILE *fp; /* fp of file to write to */
     char buffer[MSS_SIZE];
     struct timeval tp;

     if (argc != 3) {
         fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
         exit(1);
     }
     portno = atoi(argv[1]);
     fp  = fopen(argv[2], "w");
     if (fp == NULL) {
         error(argv[2]);
     }

     /* socket: create the parent socket */
     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
     if (sockfd < 0)
         error("ERROR opening socket");

     /* setsockopt: Handy debugging trick that lets
      * us rerun the server immediately after we kill it;
      * otherwise we have to wait about 20 secs.
      * Eliminates "ERROR on binding: Address already in use" error.
      */
     optval = 1;
     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
             (const void *)&optval , sizeof(int));

     /*
      * build the server's Internet address
      */
     bzero((char *) &serveraddr, sizeof(serveraddr));
     serveraddr.sin_family = AF_INET;
     serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
     serveraddr.sin_port = htons((unsigned short)portno);

     /*
      * bind: associate the parent socket with a port
      */
     if (bind(sockfd, (struct sockaddr *) &serveraddr,
                 sizeof(serveraddr)) < 0)
         error("ERROR on binding");

     /* Window is a circular queue that remembers buffered recieved packets (out of order)*/
     int* window = (int*)malloc(sizeof(int)*window_size);
     int i;
     for (i = 0; i < window_size; i ++) window[i] = 0; //Initialize
     int window_start = 0; //Head of our queue. "Window"
     int last_ack = 0; //Lask acked packet we need to send back
     clientlen = sizeof(clientaddr);

     while (1) {
       /* Read from socket */
         if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                 (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
             error("ERROR in recvfrom");
         }
         recvpkt = (tcp_packet *) buffer;
         assert(get_data_size(recvpkt) <= DATA_SIZE);
         if ( recvpkt->hdr.data_size == 0) { //Terminating condition
             fclose(fp);
             break;
         }

         /* Log time */
         gettimeofday(&tp, NULL);
         double time = tp.tv_sec+(tp.tv_usec/1000000.0);
         VLOG(DEBUG, "%lu, %d, %d", (long)time, recvpkt->hdr.data_size, recvpkt->hdr.seqno);

         sndpkt = make_packet(0);
         int ackno = (recvpkt->hdr.seqno) / DATA_SIZE; //Convert byte to int segment
         if (ackno >= last_ack){
           /* We recieved some packets out of order. We can remember we saved it.
            Queuesize circular is used because we don't recieve packets out of our window*/
             int index = (window_start + (ackno - last_ack))% window_size;
             if (window[index] == 0){
                window[index] = 1;
                 //Write to the position of the packet we recieved
                 fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
                 size_t sz = fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp); //Writes packet data into fp.
                 fflush(fp);
                 //VLOG(DEBUG, "WROTE AT %d FOR %d with pkt %d (success %d)", recvpkt->hdr.seqno, recvpkt->hdr.data_size, ackno, sz);
             }
             int inc = 0;
             //VLOG(DEBUG, "last_ack=%d ackno=%d index=%d window_start=%d\n", last_ack, ackno, index, window_start);
             while (window[window_start] == 1){ //Check if we have buffered packets
                 window[window_start] = 0;
                 window_start = (window_start + 1) % window_size;
                 inc ++; //This is the number of "buffered packet"
             }
             //VLOG(DEBUG, "After window_start=%d inc=%d\n", window_start, inc);
             last_ack = last_ack + inc; //Send back the ack of cumulative packets we have already
         }
         sndpkt->hdr.ackno = last_ack;
         sndpkt->hdr.ctr_flags = ACK; //This is a ack response

        gettimeofday(&tp, NULL);
         time = tp.tv_sec+(tp.tv_usec/1000000.0);

         if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
                 (struct sockaddr *) &clientaddr, clientlen) < 0) {
             error("ERROR in sendto");
         }
     }

     return 0;
 }
