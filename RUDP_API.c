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

#define TIMEOUT_SEC 2

/*
    NOTE:
    The network is using IPV4
*/

int checksum(RUDP *packet);
int get_ack(int socket, int sequenceNumber, clock_t time, int timeOut);
int send_ack(int socket, RUDP *packet);
int wait_to_receive(int socket, int time);


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



int RUDP_listen(int socket, int port){

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

    struct sockaddr_in clientAddress;
    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_port = htons(port);  //  
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY); 
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
    // connect to save client address.
    if (connect(socket, (struct sockaddr *)&clientAddress, clientAddressLength) ==-1) {
    printf("connect() failed with error code  : %d", errno);
    free(recv_packet);
    return ERROR;
    }
    

    //send syn-ack packet back to the client
    if (recv_packet->flags.SYN == 1) {
        printf("Client IP: %s\n", inet_ntoa(clientAddress.sin_addr));
        printf("Client Port: %d\n", ntohs(clientAddress.sin_port));
        printf("received SYN packet from the user!\n");
        RUDP *syn_ack_packet = (RUDP*)malloc(sizeof(RUDP));
        if (syn_ack_packet == NULL) {
          printf("malloc failed\n");
          return ERROR;
        }
        memset(syn_ack_packet, 0, sizeof(RUDP));
        syn_ack_packet->flags.SYN = 1;
        syn_ack_packet->flags.ACK = 1;
        int send_ack = sendto(socket, syn_ack_packet, sizeof(RUDP), 0, NULL, 0);
        if (send_ack == -1) {
            fprintf(stderr, "Error code: %d\n", errno); //debugg helper
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
    printf("inet_pton failed %s\n", strerror(errno));
    return ERROR;
  }

  // connect to save server's address.
  if (connect(socket, (struct sockaddr *)&serverAddress,sizeof(serverAddress)) == -1) {
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
        perror("Send failed\n");
        return ERROR;
    }

    // Receive a response packet and check if SYNACK returned, stop receiving after TIMEOUT_SEC seconds.
    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time); // start timer
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
            return -1;
        }

        // clock_gettime(CLOCK_MONOTONIC, &current_time);
     } while ((current_time.tv_sec - start_time.tv_sec) < TIMEOUT_SEC);

    total_attempts++;


   //if fails after several attempts
   printf("Couldn't connect to receiver.\n");
   free(packet);
   free(recv_packet);
   return FAILURE;
 }
 return FAILURE;
}

int sequence_number = 0 ;

int RUDP_receive(int socket, char **data, int *data_length) {
  //initilize the packet that will store the data received.
  RUDP *packet = malloc(sizeof(RUDP));
  if (packet == NULL) {
      printf("failed to allocate memory. error reason : %s", strerror(errno));
      return ERROR;   
  }
  memset(packet, 0, sizeof(RUDP));

  int recvLen = recvfrom(socket, packet, sizeof(RUDP) - 1, 0, NULL, 0);
  if (recvLen == -1) {
    printf("receiving failed error : %s", strerror(errno));
    free(packet);
    return ERROR;
  }
  // check if the packet arrived completely with checksum function.
  if (checksum(packet) != packet->checksum) {
    free(packet);
    return ERROR;
  }
  if (send_ack(socket, packet) == -1) {  //send ack
    free(packet);
    return ERROR;
  }
  if (packet->flags.SYN == 1) {  // connection request
    printf("received connection request\n");
    free(packet);
    return 0;
  }
  if (packet->seq_num == sequence_number) {
    if (packet->seq_num == 0 && packet->flags.DATA == 1) {
      wait_to_receive(socket, TIMEOUT_SEC * 5); //make sure that an ack will arive 
    }
    if (packet->flags.FIN == 1 && packet->flags.DATA == 1) {  // last packet
      *data = malloc(packet->length);  // Allocate memory for data
      if (data == NULL) {
      printf("failed to allocate memory. error reason : %s", strerror(errno));
      return ERROR;
      }
      memcpy(*data, packet->data, packet->length);
      *data_length = packet->length;
      free(packet);
      sequence_number = 0;
      wait_to_receive(socket, 5); 
      return 2;
    }
    if (packet->flags.DATA == 1) {     // data packet
      *data = malloc(packet->length);  // Allocate memory for data
      if (data == NULL) {
      printf("failed to allocate memory. error reason : %s", strerror(errno));
      return ERROR;
      }
      memcpy(*data, packet->data, packet->length);
      *data_length = packet->length;
      free(packet);
      sequence_number++;
      return 1;
    }
  } else if (packet->flags.DATA == 1) {
    free(packet);
    return 0;
  }
  if (packet->flags.FIN == 1) {  // close request
    free(packet);
    // send ack and wait for TIMEOUT*10 seconds to check if the sender closed
    printf("received close request\n");
    wait_to_receive(socket, TIMEOUT_SEC * 5);

    packet = malloc(sizeof(RUDP));
    if (packet == NULL) {
      printf("failed to allocate memory. error reason : %s", strerror(errno));
      return ERROR;
    }
    time_t FIN_send_time = time(NULL);
    printf("waiting for close\n");
    while ((double)(time(NULL) - FIN_send_time) < TIMEOUT_SEC) {
      memset(packet, 0, sizeof(RUDP));
      recvfrom(socket, packet, sizeof(RUDP) - 1, 0, NULL, 0);
      if (packet->flags.FIN == 1) {
        if (send_ack(socket, packet) == -1) {
          free(packet);
          return ERROR;
        }
        FIN_send_time = time(NULL);
      }
    }
    free(packet);
    close(socket);
    return -2;
  }
  free(packet);
  return 0;
}


int RUDP_send(int socket, char *data, int data_length) {
  int packets_num = data_length / PACKET_MAX_SIZE;       // calculate the number of packets needed to send the data
  int last_packet_size = data_length % PACKET_MAX_SIZE; // calculate the size of the last packet

  RUDP *packet = malloc(sizeof(RUDP));
  if (packet == NULL) {
    printf("failed to allocate memory\n");
    return ERROR;
  }

  // send the packets
  for (int i = 0; i < packets_num; i++) {
    memset(packet, 0, sizeof(RUDP));
    packet->seq_num = i;     // set the sequence number
    packet->flags.DATA = 1;  // set the DATA flag
    if (i == packets_num - 1 && last_packet_size == 0) {  // if its the last packet, set the FIN flag
      packet->flags.FIN = 1;
    }
    memcpy(packet->data, data + i * PACKET_MAX_SIZE, PACKET_MAX_SIZE);
    packet->length = PACKET_MAX_SIZE;
    packet->checksum = checksum(packet);
    do {  // send the packet and wait for ack
      int sd = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
      if (sd == -1) {
        printf("sendto() failed with error code  : %d", errno);
        return ERROR;
      }
      // wait for ack and retransmit if needed
    } while (get_ack(socket, i, clock(), TIMEOUT_SEC) <= 0);
  }
  // if we have a last packet, send it
  if (last_packet_size > 0) {
    memset(packet, 0, sizeof(RUDP));
    // set the fields of the packet
    packet->seq_num = packets_num;
    packet->flags.DATA = 1;
    packet->flags.FIN = 1;
    memcpy(packet->data, data + packets_num * PACKET_MAX_SIZE,last_packet_size);
    packet->length = last_packet_size;
    packet->checksum = checksum(packet);
    do {  
      int sd = sendto(socket, packet, sizeof(RUDP), 0, NULL, 0);
      if (sd == -1) {
        printf("sending faild. error reason : %s", strerror(errno));
        free(packet);
        return ERROR;
      }
    } while (get_ack(socket, packets_num, clock(), TIMEOUT_SEC) <= 0);
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

int get_ack(int socket, int sequenceNumber, clock_t time, int timeOut){
    RUDP *recv_packet = (RUDP*)malloc(sizeof(RUDP));
    if (recv_packet == NULL) {
      printf("failed to allocate memory");
      return -1;
    }
    memset(recv_packet, 0, sizeof(RUDP));
    while ((double)(clock()-time)/CLOCKS_PER_SEC < timeOut) {   
        int rf = recvfrom(socket, recv_packet, sizeof(RUDP), 0, NULL, 0);
        if (rf == -1) {
            printf("failed to receive the packet");
            free(recv_packet);
            return ERROR;   
        }
        if (recv_packet->flags.ACK ==1 && recv_packet->seq_num == sequenceNumber) { 
            free(recv_packet);
            // printf("received ack successfuly\n");
            return SUCCESS;
        }
    }
    free(recv_packet);
    return FAILURE;
}

/*
  a way to guarantee packet's data transfered appropriatly.
  *this is a very simple checksum fucntion that ensures data's completeness.
*/
int checksum(RUDP *packet) {
  int sum = 0;
  for (int i = 0; i < 50 && i < PACKET_MAX_SIZE; i++) {
    sum += packet->data[i];
  }
  return sum;
}


/*
  this fucntion prints a ramdom generated buffer - in our project is was used mostly for debugging
*/
void print_buffer(const char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
}


/*
  This function receives a packet and sends ack according to the packet received. 
  @param socket - the socket to send the ack packet on.
  @param packet - pointer to the received packet.
  @return 1 if success, -1 if fails to send the ack packet.
*/
int send_ack(int socket, RUDP *packet){
    RUDP *ack_packet = (RUDP*)malloc(sizeof(RUDP));
    if (ack_packet == NULL) {
      printf("failed to allocate memory");
      return ERROR;
    }
    memset(ack_packet, 0, sizeof(RUDP));

    // set the ack_packet fields
    ack_packet->flags.ACK = 1; 
    if (packet->flags.SYN == 1) {
      ack_packet->flags.SYN = 1;
    }
    if (packet->flags.DATA == 1) {
      ack_packet->flags.DATA = 1;
    }
    if (packet->flags.FIN == 1) {
      ack_packet->flags.FIN = 1;
    }
    ack_packet->checksum = checksum(packet);
    ack_packet->seq_num = packet->seq_num;
    int send_packet = sendto(socket, ack_packet, sizeof(RUDP), 0, NULL, 0);
    if (send_packet == -1) {
      printf("failed to send the ack packet error code: %s\n", strerror(errno) );
      free(ack_packet);
      return ERROR;
    }
    free(ack_packet);
    return SUCCESS; 
}


// close the connection
int RUDP_close(int socket) {
  RUDP *close_packet = (RUDP*)malloc(sizeof(RUDP));
  if (close_packet == NULL) {
    printf("failed to allocate memory. error reason : %s\n", strerror(errno));
    return ERROR;
  }
  memset(close_packet, 0, sizeof(RUDP));
  close_packet->flags.FIN = 1;
  close_packet->seq_num = -1;
  close_packet->checksum = checksum(close_packet);
  do {
    int sendResult = sendto(socket, close_packet, sizeof(RUDP), 0, NULL, 0);
    if (sendResult == -1) {
      printf("send failed. %s\n", strerror(errno));
      free(close_packet);
      return ERROR;
    }
  } while (get_ack(socket, -1, clock(), TIMEOUT_SEC) <= 0);
  close(socket);
  free(close_packet);
  return SUCCESS;
}


/*
  *this fucntion waits time amount of seconds to receive a packet before fails.
  @param socket - the socket which will be timed out.
  @param time - time is second to wait for a packet to arrive.
  *returns 1 if receives a packet in the given time, else -1.
*/
int wait_to_receive(int socket, int time) {
  // set timeout for the socket
  struct timeval timeout;
  timeout.tv_sec = time;
  timeout.tv_usec = 0;

  if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
      0) {
    perror("Error setting timeout for socket");
    return ERROR;
  }
  return SUCCESS;
}


   





