#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "RUDP_API.h"

#define MAX_BUFFER_SIZE 1024
#define TIMEOUT_SEC 2

/*
    The network is using IPV4.

*/

int checksum(RUDP *packet);
int wait_for_ack(int socket, int sequenceNumber, clock_t time, int timeOut);
int send_ack(int socket, RUDP *packet);

int sequence_number = 0 ;

/*
  *creating UDP socket
  @returns the socket

*/
int RUDP_socket(){
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        printf("failed to create socket");
        return ERROR;
    }
    return sockfd;
}

int RUDP_connect(int socket, char *ip, int port)
{
  // Setup the server address structure.
  struct sockaddr_in serverAddress;
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  int rval = inet_pton(AF_INET, (const char *)ip, &serverAddress.sin_addr);
  if (rval <= 0) {
    printf("inet_pton() failed\n");
    return ERROR;
  }

  if (connect(socket, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) == -1) {
    perror("connect failed\n");
    return ERROR;
  }

   // send SYN packet
   RUDP *packet = (RUDP*)malloc(sizeof(RUDP));
   memset(packet, 0, sizeof(RUDP));
   packet->flags.SYN = 1;
   int total_attempts = 0;
   RUDP *recv_packet = NULL;

   //TRY CERTAIN AMOUNT OF TIMES - CHANGEABLE
   while (total_attempts < RETRY)
   {
        //send the syn packet
        int send_result = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
        if(send_result == -1){
            perror("send failed");
            return ERROR;
        }

        //receive an response packet and check if SYNACK returned
        clock_t start_time = clock();
        do {
        recv_packet = (RUDP*)malloc(sizeof(RUDP));
        memset(recv_packet, 0, sizeof(RUDP));
        int recvmsg = recvfrom(socket, recv_packet, sizeof(RUDP), 0, NULL, 0);
            if (recvmsg == -1) {   
                fprintf(stderr, "Error code: %d\n", errno);
                free(recv_packet);
                return ERROR;
            }
        //check if we got SYNACK packet
        if (recv_packet->flags.ACK && recv_packet->flags.SYN) {
            printf("connection established\n");
            free(recv_packet);
            free(packet);
            return SUCCESS;
        }
        else {
            printf("The packet is not a connection packet");
        }
        }while ((double)(clock() - start_time) / CLOCKS_PER_SEC < TIMEOUT_SEC);
        total_attempts++;
   }

   //if fails after certain attempts
   printf("couldn't connect to receiver");
   free(packet);
   free(recv_packet);
   return FAILURE;
}



int RUDP_wait_for_connection(int socket, int port){

    //initilization of server adress.
    struct sockaddr_in serverAddress = {0};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind the server
    int rc = bind(socket, (const struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (rc == -1)  {
        printf("bind failed");
        close(socket);
        return ERROR;
    }

    //receive syn packet from client
    struct sockaddr_in clientAddress;
    memset(&clientAddress, 0, sizeof(clientAddress));


    RUDP *recv_packet;
    recv_packet = (RUDP*)malloc(sizeof(RUDP));
    memset(recv_packet, 0, sizeof(RUDP));
    int recv_l = recv(socket, recv_packet, sizeof(RUDP), 0);
    if (recv_l == -1) {
        printf("receiving failed");
        free(recv_packet);
        return ERROR;   
    }

    //make connection 
    if (connect(socket, (const struct sockaddr*)&clientAddress, sizeof(clientAddress)) == -1) {
        printf("connection with client failed");
        free(recv_packet);
        return ERROR;
    }


    //send syn-ack packet back to the client
    if (recv_packet->flags.SYN == 1) {
        RUDP *ack_packet = (RUDP*)malloc(sizeof(RUDP));
        memset(ack_packet, 0, sizeof(RUDP));
        ack_packet->flags.SYN = 1;
        ack_packet->flags.ACK = 1;
        int send_ack = sendto(socket, ack_packet, sizeof(RUDP), 0, NULL, 0);
        if (send_ack == -1) {
            printf("sending ack packet failed");
            free(ack_packet);
            return ERROR;
        }

        //if everything worked fine, free the packets and return SUCCESS.
        free(recv_packet);
        free(ack_packet);
        return SUCCESS;
    }

    return FAILURE;
}



int RUDP_send(int socket, char *data, int data_length) {
    //caclate how many packets needs to be sent.
    int number_of_packets = data_length / PACKET_MAX_SIZE;
    int last_packet_size = data_length % PACKET_MAX_SIZE;


    RUDP *packet = (RUDP*)malloc(sizeof(RUDP));

  // send the packets
  for (int i = 0; i < number_of_packets; i++) {
    memset(packet, 0, sizeof(RUDP));
    packet->seq_num = i;     // set the sequence number
    packet->flags.DATA = 1;  // set the DATA flag 
    // if its the last packet, set the FIN flag
    if (i == number_of_packets - 1 && last_packet_size == 0) {
      packet->flags.FIN = 1;
    }
    // put the data in the packet
    memcpy(packet->data, data + i * PACKET_MAX_SIZE, PACKET_MAX_SIZE);
    // set the length of the packet
    packet->length = PACKET_MAX_SIZE;
    // calculate the checksum of the packet
    packet->checksum = checksum(packet);
    do {  // send the packet and wait for ack
      int send_packet = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
      if (send_packet == -1) {
        printf("packet send failed");
        return ERROR;
      }
      // wait for ack 
    } while (wait_for_ack(socket, i, clock(), TIMEOUT_SEC) <= 0);
  }


  // if we have a last packet, send it
  if (last_packet_size > 0) {
    memset(packet, 0, sizeof(RUDP));
    // set the fields of the packet - simillar to before
    packet->seq_num = number_of_packets;
    packet->flags.DATA = 1;
    packet->flags.FIN = 1;
    memcpy(packet->data, data + number_of_packets * PACKET_MAX_SIZE,
           last_packet_size);
    packet->length = last_packet_size;
    packet->checksum = checksum(packet);
    do {  // send the packet and wait for ack
      int send_packet = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
      if (send_packet == -1) {
        printf("packet send failed");
        free(packet);
        return ERROR;
      }
    } while (wait_for_ack(socket, number_of_packets, clock(), TIMEOUT_SEC) <= 0);
    free(packet);
  }
  return SUCCESS;
}


/*
  waits for ack
  @param socket - the socket to receive from
  @param sequenceNumber - the sequence numeber of the packet
  @param time - timeOut - stop witing after timeOut ends.
  @return 1 if receives ack, -1 if didnt receive ack after timeOut time
*/

int wait_for_ack(int socket, int sequenceNumber, clock_t time, int timeOut){
    RUDP *recv_packet = (RUDP*)malloc(sizeof(RUDP));
    memset(recv_packet, 0, sizeof(RUDP));
    while ((double)(clock()-time)/CLOCKS_PER_SEC < timeOut) {
        int rc = recvfrom(socket, recv_packet, sizeof(RUDP), 0, NULL, 0);
        if (rc == -1) {
            printf("failed to receive the packet");
            free(recv_packet);
            return ERROR;   
        }
        if (recv_packet->flags.ACK ==1 && recv_packet->seq_num == sequenceNumber) {
            free(recv_packet);
            return SUCCESS;
        }
    }
    free(recv_packet);
    return FAILURE;
}

/*
  a way to guarantee packet's data transfered appropriatly.
*/
int checksum(RUDP *packet) {
  int sum = 0;
  for (int i = 0; i < 10 && i < PACKET_MAX_SIZE; i++) {
    sum += packet->data[i];
  }
  return sum;
}


int RUDP_receive(int socket, char **data, int *data_length) {
    RUDP *recv_packet = (RUDP*)malloc(sizeof(RUDP));
    memset(recv_packet, 0, sizeof(RUDP));

    int rc = recvfrom(socket, recv_packet, sizeof(RUDP), 0, NULL, 0);
    if (rc == -1) {
      printf("receiving failed");
      free(recv_packet);
      return ERROR;
    }
    //check if the packet data is the same by checksum method.
    if (checksum(recv_packet) != recv_packet->checksum) {
      printf("The packet didnt received correctly.");
      return -1;
    }

    //if received connection request
    if (recv_packet->flags.ACK ==1) {
      printf("received connection packet request");
      free(recv_packet);
      return 0;
    }
    
    //send ack packet
    if (send_ack(socket, recv_packet) == -1) {
      printf("failed to send ack packet");
      free(recv_packet);
      return ERROR;
    }

    //receive the packet and save it
    if (recv_packet->seq_num == sequence_number) {  //if last data packet
      if (recv_packet->flags.FIN == 1 && recv_packet->flags.DATA == 1) {
        *data = (char*)malloc(sizeof(recv_packet->length));
        memcpy(data, recv_packet->data, recv_packet->length);
        *data_length = recv_packet->length;
        free(recv_packet);
        sequence_number = 0;
        //set long timeout to finish
        struct timeval timeout;
        timeout.tv_sec = 100000;  // 100000 seconds timeout
        timeout.tv_usec = 0;

        if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket timeout");
        return ERROR;
        }

        return 2;
      }
      if (recv_packet->flags.DATA == 1) {   //data packet
      *data = (char*)malloc(sizeof(recv_packet->length));
      memcpy(data, recv_packet->data, recv_packet->length);
      *data_length = recv_packet->length;
      free(recv_packet);
      sequence_number++;
      return 1;
      }
    }

  if (recv_packet->flags.DATA == 1) {
    free(recv_packet);
    return 0;
  }
  if (recv_packet->flags.FIN == 1) { //if close request
    free(recv_packet);
    printf("received close packet\n");
    //send ack and wait for response
    struct timeval timeout;
      timeout.tv_sec = 10;  // 10 seconds timeout
      timeout.tv_usec = 0;

      if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket timeout");
        return ERROR;
      }
    RUDP *packet = (RUDP*)malloc(sizeof(RUDP));
    printf("waiting for response\n");
    time_t fin_time = time(NULL);
    while ((double)(time(NULL) - fin_time) < TIMEOUT_SEC) {
      memset(packet, 0, sizeof(RUDP));
      recvfrom(socket, packet, sizeof(RUDP), 0, NULL, 0);
      if (packet->flags.FIN == 1) {
        if (send_ack(socket, packet) == -1) {
          free(packet);
          return ERROR;
        }
        fin_time = time(NULL);
      }
    }

    free(packet);
    close(socket);
    return -2;
  }
  free(recv_packet);
  return 0;
} 


/*
  Sends ack packet.
  @param socket - the socket to send the ack packet on.
  @param packet - pointer to the received packet.
  @return 1 if success, -1 if fails to send the ack packet.
*/
int send_ack(int socket, RUDP *packet){
    RUDP *ack_packet = (RUDP*)malloc(sizeof(RUDP));
    memset(ack_packet, 0, sizeof(RUDP));
    ack_packet->flags.ACK = 1;

    //if received SYN packet, send SYN ACK 
    if (packet->flags.SYN == 1) {
      ack_packet->flags.SYN = 1;
    }

    //if received FIN request 
    if (packet->flags.FIN == 1) {
      ack_packet->flags.FIN = 1;
    }

    //if received data packet
    if (packet->flags.DATA == 1) {
      ack_packet->flags.DATA = 1;
    }

    ack_packet->checksum = checksum(packet);
    ack_packet->seq_num = packet->seq_num;

    //send the ackpacket
    int send_packet = sendto(socket, ack_packet, sizeof(RUDP), 0,  NULL, 0);
    if (send_packet == -1) {
      printf("failed to send the ack packet");
      free(ack_packet);
      return ERROR;
    }
    free(ack_packet);
    return SUCCESS; 
}


// close the connection
int RUDP_close(int socket) {
  RUDP *close_packet = (RUDP*)malloc(sizeof(RUDP));
  memset(close_packet, 0, sizeof(RUDP));
  close_packet->flags.FIN = 1;
  close_packet->seq_num = -1;
  close_packet->checksum = checksum(close_packet);
  do {
    int sendResult = sendto(socket, close_packet, sizeof(RUDP), 0, NULL, 0);
    if (sendResult == -1) {
      printf("send failed");
      free(close_packet);
      return -1;
    }
  } while (wait_for_ack(socket, -1, clock(), TIMEOUT_SEC) <= 0);
  close(socket);
  free(close_packet);
  return SUCCESS;
}






   





