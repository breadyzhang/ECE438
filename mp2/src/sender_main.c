/*
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#define PACKETSIZE 832
#define NUMSIZE 32
#define DATASIZE 800
#define CWSIZE 256
#define ACKSIZE 32

struct sockaddr_in si_other;
int s;
unsigned int slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

long int getPacketLen(long int head, long int lastPacket, unsigned long long int bytesToTransfer){
  long int packetLen = head == lastPacket ? bytesToTransfer % DATASIZE : DATASIZE;
  if(packetLen == 0){
    return DATASIZE+1;
  }
  else{
    return packetLen;
  }
}
void getPacket(long int offset, char* buf, FILE* fp, int numBytes){
  fseek(fp, DATASIZE*offset, SEEK_SET);
  fread(&buf[NUMSIZE], numBytes, 1, fp);
  sprintf(buf, "%ld", offset);
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

	/* Send data and receive acknowledgements on s*/
    // setting up select function
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_sec = 500000;
    fd_set master;
    fd_set readfds;
    FD_ZERO(&master);
    FD_ZERO(&readfds);
    FD_SET(s, &master);

    // setting up basic numbers for sender
    char buf[PACKETSIZE+1];
    char ack[ACKSIZE+1];
    double sst = 128.0;
    double cw_size = 1.0;
    long int head = 0;
    long int tail = 0;
    int numbytes;
    long int lastPacket = (bytesToTransfer-1)/DATASIZE;
    // long int prevAck = -1;
    int dupACKS = 0;
    // printf("pog\n");
    long int packetLen = getPacketLen(head,lastPacket,bytesToTransfer);
    getPacket(head,buf,fp,packetLen);
    printf("sending %ld\n", head);
    if((numbytes = sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen)) == -1){
      printf("no");
      exit(1);
    }
    int ready = 1;
    printf("lastpacket: %ld\n",lastPacket);
    while(1){
      // clear buffers
      // timeout => panic mode cw = 1, sst = sst/2, resend packets
      readfds = master;
      ready = select(s+1, &readfds, NULL, NULL, &tv);
      if(ready == 0){
        printf("timeout\n");
        cw_size = 1.0;
        tail = head;
        sst = sst/2 == 0 ? 1 : sst/2;
        long int packetLen = getPacketLen(head,lastPacket,bytesToTransfer);
        printf("%ld\n", packetLen);
        getPacket(head, buf, fp, packetLen);
        // printf("%s\n", buf);
        numbytes = sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);
      }
      else{
        // read received packets
        numbytes = recvfrom(s, ack, ACKSIZE, 0, (struct sockaddr*)&si_other, &slen);
        int ackNum = atoi(ack);
        printf("ack: %d\thead: %ld\ttail: %ld\tcw: %f\n", ackNum, head, tail, cw_size);
        // check if ackNum is out of bounds
        if(ackNum < head-1){
          continue;
        }
        // check if ack is a dupAck
        else if(ackNum == head-1){
          dupACKS++;
        }
        // receiver got all of the packets
        else if(ackNum == lastPacket){
          sendto(s,buf,0,0,(struct sockaddr*)&si_other, slen);
          break;
        }
        // dupACKS == 3 so do congestion avoidance
        if(dupACKS == 3){
          printf("3 dupacks\n");
          dupACKS++;
          sst = cw_size/2;
          cw_size = sst;
          cw_size += 3;
          tail = head-1;
          while(tail < (int)cw_size+head-1){
            tail++;
            long int packetLen = getPacketLen(tail,lastPacket,bytesToTransfer);
            getPacket(tail, buf, fp, packetLen);
            sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);
          }
        }
        else if(dupACKS > 3){
          printf("fast recovery\n");
          dupACKS++;
          cw_size++;
          if(ackNum >= head){
            printf("recovered\n");
            dupACKS = 0;
            cw_size = sst;
            while(head <= ackNum){
              cw_size += 1.0/(int)cw_size;
              head++;
            }
            tail = head-1;
            while(tail < (int)cw_size+head-1){
              tail++;
              if(tail > lastPacket){
                tail = lastPacket;
                break;
              }
              int packetLen = tail == lastPacket ? bytesToTransfer%DATASIZE : DATASIZE;
              getPacket(tail, buf, fp, packetLen);
              sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);
            }
          }
          else{
            printf("sending: %ld\n",head);
            long int packetLen = getPacketLen(head,lastPacket,bytesToTransfer);
            getPacket(head, buf, fp, packetLen);
            sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);

            tail++;
            if(tail > lastPacket){
              tail = lastPacket;
              continue;
            }

            printf("sending: %ld\n",tail);
            packetLen = getPacketLen(tail,lastPacket,bytesToTransfer);
            getPacket(tail, buf, fp, packetLen);
            sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);
          }
        }
        // slow start
        else if(cw_size < sst){
          printf("slow start\n");
          while(head <= ackNum && cw_size < sst){
            head++;
            cw_size++;
          }
          while(head <= ackNum){
            cw_size += 1.0/(int)cw_size;
            head++;
          }
          while(tail < (int)cw_size+head-1){
            tail++;
            if(tail > lastPacket){
              tail = lastPacket;
              break;
            }
            long int packetLen = getPacketLen(tail,lastPacket,bytesToTransfer);
            printf("sending: %ld\n", tail);
            getPacket(tail, buf, fp, packetLen);
            // printf("buf: %s\n",buf);
            sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);
          }
        }
        // linear increase
        else{
          printf("congestion avoidance\n");
          while(head <= ackNum){
            cw_size += 1.0/(int)cw_size;
            head++;
          }
          while(tail < (int)cw_size+head-1){
            tail++;
            if(tail > lastPacket){
              tail = lastPacket;
              break;
            }
            long int packetLen = getPacketLen(tail,lastPacket,bytesToTransfer);
            getPacket(tail, buf, fp, packetLen);
            sendto(s, buf, NUMSIZE+packetLen, 0, (struct sockaddr*)&si_other, slen);
          }
        }
      }
      if(cw_size > 200){
        printf("sawtooth\n");
        sst = cw_size/2;
        cw_size = sst;
      }
      // tv.tv_usec = 10000;
    }

    printf("Closing the socket\n");
    close(s);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}
