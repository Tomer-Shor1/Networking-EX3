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
    NOTE:
    The network is using IPV4
*/

int checksum(RUDP *packet);
int wait_for_ack(int socket, int sequenceNumber, clock_t time, int timeOut, struct sockaddr *clientAddress, socklen_t *clientAddressLen);
int send_ack(int socket, RUDP *packet, struct sockaddr_in *clientAddress, socklen_t clientAddressLen);

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
  socklen_t serverAddressLen = sizeof(serverAddress);
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
   RUDP *recv_packet;
   recv_packet = (RUDP*)malloc(sizeof(RUDP));
    memset(recv_packet, 0, sizeof(RUDP));
   int total_attempts = 0;

   //receive synack 
  while (total_attempts < RETRY)
{
    // Send the SYN packet
    int send_result = sendto(socket, packet, sizeof(RUDP), 0, (struct sockaddr*)&serverAddress, serverAddressLen);
    if (send_result == -1) {
        perror("send failed");
        return ERROR;
    }

    // Receive a response packet and check if SYNACK returned
    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    do {
        int recvmsg = recvfrom(socket, recv_packet,sizeof(RUDP), 0, (struct sockaddr*)&serverAddress, &serverAddressLen);
        if (recvmsg == -1) {
            fprintf(stderr, "Error code: %d\n", errno);
            free(recv_packet);
            return ERROR;
        } else if (recvmsg == 0) {
            fprintf(stderr, "Connection closed by remote peer\n");
            free(recv_packet);
            return ERROR;
        }

        printf("Received packet\n");
        // Check if we got SYNACK packet
        if (recv_packet->flags.ACK == 1 && recv_packet->flags.SYN == 1) {
            printf("Connection established\n");
            free(recv_packet);
            free(packet);
            return SUCCESS;
        } else {
            printf("The packet is not a connection packet\n");
        }

        // clock_gettime(CLOCK_MONOTONIC, &current_time);
     } while ((current_time.tv_sec - start_time.tv_sec) < TIMEOUT_SEC);

    total_attempts++;


   //if fails after several attempts
   printf("couldn't connect to receiver");
   free(packet);
   free(recv_packet);
   return FAILURE;
 }
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
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_port = htons(port);  // Specify the destination port here
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY); // inet_addr("127.0.0.1"); - TRY
    socklen_t clientAddressLength = sizeof(clientAddress);


    RUDP *recv_packet;
    recv_packet = (RUDP*)malloc(sizeof(RUDP));
    if (recv_packet == NULL) {
      printf("failed to allocate memory for receiving packet");
      return ERROR;
    }
    memset(recv_packet, 0, sizeof(RUDP));
    int recv_l = recvfrom(socket, recv_packet, sizeof(RUDP), 0, (struct sockaddr*)&clientAddress, &clientAddressLength);
    if (recv_l == -1) {
        printf("receiving failed");
        free(recv_packet);
        return ERROR;   
    }
    

    //send syn-ack packet back to the client
    if (recv_packet->flags.SYN == 1) {
        printf("Client IP: %s\n", inet_ntoa(clientAddress.sin_addr));
        printf("Client Port: %d\n", ntohs(clientAddress.sin_port));
        printf("received synpacket\n");
        RUDP *syn_ack_packet = (RUDP*)malloc(sizeof(RUDP));
        if (syn_ack_packet == NULL) {
          printf("malloc failed\n");
          return ERROR;
        }
        memset(syn_ack_packet, 0, sizeof(RUDP));
        syn_ack_packet->flags.SYN = 1;
        syn_ack_packet->flags.ACK = 1;
        printf("the size of the synack packet is %ld d and the syn flag is %d and ack flag is %d\n" , sizeof(RUDP), syn_ack_packet->flags.ACK, syn_ack_packet->flags.SYN);
        int send_ack = sendto(socket, syn_ack_packet, sizeof(RUDP), 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
        if (send_ack == -1) {
            fprintf(stderr, "Error code: %d\n", errno);
            printf("sending ack packet failed\n");
            free(syn_ack_packet);
            return ERROR;
        }

        //if everything worked fine, free the packets and return SUCCESS.
        free(recv_packet);
        free(syn_ack_packet);
        return SUCCESS;
    }

    return FAILURE;
}



int RUDP_send(int socket, char *data, int data_length, struct sockaddr *serverAddress, socklen_t *serverAddressLen, struct sockaddr *clientAddress, socklen_t *clientAddressLen) {
    //caclate how many packets needs to be sent.
    int number_of_packets = data_length / PACKET_MAX_SIZE;
    int last_packet_size = data_length % PACKET_MAX_SIZE;


    RUDP *packet = (RUDP*)malloc(sizeof(RUDP));
    if (packet == NULL) {
      printf("failed to allocate memory\n");
      return ERROR;
    }

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
    printf("The packet data size: %ld", strlen(packet->data));
    do {  // send the packet and wait for ack
      int send_packet = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
      //int send_packet = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
      if (send_packet == -1) {
        perror("sendto failed");
       // printf("packet send failed\n");
        return ERROR;
      }
      printf("Send part of the file!\n");
      // wait for ack code includes a loop to retry sending the pa
    } while (wait_for_ack(socket, i, clock(), TIMEOUT_SEC, (struct sockaddr*)&clientAddress, clientAddressLen) <= 0);
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
    } while (wait_for_ack(socket, number_of_packets, clock(), TIMEOUT_SEC, (struct sockaddr*)&clientAddress, clientAddressLen) <= 0);
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

int wait_for_ack(int socket, int sequenceNumber, clock_t time, int timeOut, struct sockaddr *clientAddress, socklen_t *clientAddressLen){
    RUDP *recv_packet = (RUDP*)malloc(sizeof(RUDP));
    if (recv_packet == NULL) {
      printf("failed to allocate memory");
      return -1;
    }
    memset(recv_packet, 0, sizeof(RUDP));
    while ((double)(clock()-time)/CLOCKS_PER_SEC < timeOut) {
        int rc = recvfrom(socket, recv_packet, sizeof(RUDP)-1, 0, NULL, 0);
        if (rc == -1) {
            printf("failed to receive the packet");
            free(recv_packet);
            return ERROR;   
        }
        printf("Received a packet!\n");
        if (recv_packet->flags.ACK ==1 && recv_packet->seq_num == sequenceNumber) {
            free(recv_packet);
            printf("Returning success on ack\n");
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


int RUDP_receive(int socket,int port, char **data, int *data_length) {
    struct sockaddr_in clientAddress;
    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_port = htons(port);  // Specify the destination port here
    clientAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    char buffer[sizeof(RUDP)]; // Assuming RUDP is a fixed-size struct
    socklen_t clientAddressLength = sizeof(clientAddress);
    memset(buffer, 0, sizeof(buffer));

    RUDP *recv_packet = (RUDP*)malloc(sizeof(RUDP));
    if (recv_packet == NULL) {
      printf("memory allocate failed\n");
      return ERROR;
    }
    memset(recv_packet, 0, sizeof(RUDP));

    int rc = recvfrom(socket, buffer, sizeof(buffer), 0, NULL, 0);
    if (rc == -1) {
      fprintf(stderr, "Error code: %d\n", errno);
      printf("receiving failed\n");

      free(recv_packet);
      return ERROR;
    }
    printf("received packet from the client! the massage is %s\n", buffer);

    memcpy(recv_packet, buffer, sizeof(RUDP));
    //check if the packet data is the same by checksum method.
    if (checksum(recv_packet) != recv_packet->checksum) {
      printf("The packet didnt received correctly.");
      return -1;
    }
    printf("the data is %s", recv_packet->data);

    //if received connection request
    if (recv_packet->flags.ACK ==1) {
      printf("received connection packet request");
      free(recv_packet);
      return 0;
    }
    
    //send ack packet
    if (send_ack(socket, recv_packet, (struct sockaddr_in*)&clientAddress, sizeof(clientAddress)) == -1) {
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
    printf("\nreceived close packet\n");
    //send ack and wait for response
    struct timeval timeout;
      timeout.tv_sec = 3;  // 10 seconds timeout
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
        if (send_ack(socket, packet, (struct sockaddr_in*)&clientAddress, sizeof(clientAddress)) == -1) {
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
int send_ack(int socket, RUDP *packet, struct sockaddr_in *clientAddress, socklen_t clientAddressLen){
    RUDP *ack_packet = (RUDP*)malloc(sizeof(RUDP));
    if (ack_packet == NULL) {
      printf("failed to allocate memory");
      return ERROR;
    }
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
    int send_packet = sendto(socket, ack_packet, sizeof(RUDP), 0, NULL, 0);
    if (send_packet == -1) {
      printf("failed to send the ack packet");
      free(ack_packet);
      return ERROR;
    }
    free(ack_packet);
    return SUCCESS; 
}


// close the connection
int RUDP_close(int socket, struct sockaddr_in *clientAddress, socklen_t clientAddressLen) {
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
  } while (wait_for_ack(socket, -1, clock(), TIMEOUT_SEC, (struct sockaddr*)&clientAddress, &clientAddressLen) <= 0);
  close(socket);
  free(close_packet);
  return SUCCESS;
}






   





