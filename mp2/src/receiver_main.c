/*
 * File:   receiver_main.c
 * Author:
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define PACKETSIZE 832
#define DATASIZE 800
#define NUMSIZE 32
#define BUFFERSIZE 512
#define ACKSIZE 32

struct sockaddr_in si_me, si_other;
int s;
unsigned int slen;

void diep(char *s) {
    perror(s);
    exit(1);
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */
  // open destinationFile
  FILE* fp;
  fp = fopen(destinationFile,"w+");
  char packetBuffer[BUFFERSIZE][PACKETSIZE+1];
  int packetOcc[BUFFERSIZE];
  // clear arrays
  int i = 0;
  for(i = 0; i < BUFFERSIZE; i++){
    packetOcc[i] = -1;
    bzero(packetBuffer[i], PACKETSIZE+1);
  }
  packetOcc[0] = 0;
  int head = 0; // head of buffer
  int numbytes;
  long int lastPacket = -1;
  int lastSize = 0;
  char buf[PACKETSIZE];
  // start TCP protocol
  char ack[ACKSIZE];
  printf("connected\n");
  while((numbytes = recvfrom(s,buf,PACKETSIZE+1,0,(struct sockaddr*)&si_other, &slen)) > 0){
    char packetNum[ACKSIZE+1];
    bzero(packetNum, ACKSIZE+1);
    strncpy(packetNum,buf,ACKSIZE);
    char packet[DATASIZE+1];
    bzero(packet, DATASIZE+1);
    int num = atoi(packetNum);
    printf("received: %d head: %d size: %d\n", num,head,numbytes);
    if(numbytes < PACKETSIZE){
      printf("last packet\n");
      lastPacket = num;
      lastSize = numbytes - NUMSIZE;
    }
    else if(numbytes > PACKETSIZE){
      printf("last packet\n");
      lastPacket = num;
      lastSize = DATASIZE;
    }
    // received packet is head of buffer
    if(num == head){
      // write packets to file
      strncpy(packetBuffer[head%BUFFERSIZE],&buf[ACKSIZE],numbytes-NUMSIZE);
      packetOcc[head%512] = num;
      printf("head: %d\n",head);
      do{
        if(head == lastPacket){
          // printf("last packet %ld with size %d \nwith %s\n",lastPacket, lastSize,packetBuffer[head%BUFFERSIZE]);
          fwrite(packetBuffer[head%BUFFERSIZE], 1, lastSize,fp);
        }
        else{
          // printf("saving packet %d with size %d as:\n%s\n", head, DATASIZE, packetBuffer[head&BUFFERSIZE]);
          fwrite(packetBuffer[head%BUFFERSIZE], 1, DATASIZE,fp);
        }
        // printf("%s\n", packetBuffer[head%BUFFERSIZE]);
        packetOcc[head%BUFFERSIZE] = -1;
        bzero(packetBuffer[head%BUFFERSIZE], PACKETSIZE);
        head++;
      }while(packetOcc[head%BUFFERSIZE] != -1);
      // send latest ack to socket
      sprintf(ack, "%d", head-1);
      printf("sending ack: %s\n", ack);
      if((numbytes=sendto(s,ack,ACKSIZE,0,(struct sockaddr*)&si_other,slen))<0){
        printf("oh");
        exit(1);
      }
    }
    // received ooo packet
    else if(num > head && num-head < BUFFERSIZE){
      strncpy(packetBuffer[num%BUFFERSIZE], buf, DATASIZE);
      packetOcc[num%512] = num;
      sprintf(ack, "%d", head-1);
      printf("sending ack: %s\n", ack);
      if((numbytes=sendto(s,ack,ACKSIZE,0,(struct sockaddr*)&si_other,slen))<0){
        printf("oh");
        exit(1);
      }
    }
    // received oob ooo packet
    else{
      printf("out of bounds\n");
      sprintf(ack, "%d", head-1);
      if((numbytes=sendto(s,ack,ACKSIZE,0,(struct sockaddr*)&si_other,slen))<0){
        printf("oh");
        exit(1);
      }
    }
  }
  printf("closing socket and saving file\n");
  fclose(fp);
  close(s);
  return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);

    return 0;
}
