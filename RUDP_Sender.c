#include <arpa/inet.h>
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


#define BUFFER_SIZE (2 * 1024) // 2MB


/*
    generates a random buffer of a given size. max 2MB 

*/
void generate_random_buffer(char *buffer, size_t size);
void print_buffer(const char *buffer, size_t size);



int main(int argc, char* argv[]){

    //check 
    

    if (argc != 5) {
        fprintf(stderr, "Usage: %s -ip <IP_ADDRESS> -p <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    char *ip_address = NULL;
    char *port_number_str = NULL;

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-ip") == 0) {
            ip_address = argv[i + 1];
        } else if (strcmp(argv[i], "-p") == 0) {
            port_number_str = argv[i + 1];
        }
    }

    if (ip_address == NULL || port_number_str == NULL) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    int port_number = atoi(port_number_str);

    printf("IP Address: %s\n", ip_address);
    printf("Port Number: %d\n", port_number);

  struct sockaddr_in serverAddress;
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port_number);
  socklen_t serverAddressLen = sizeof(serverAddress);
  int rval = inet_pton(AF_INET, (const char *)ip_address, &serverAddress.sin_addr);
  if (rval <= 0) {
    printf("inet_pton() failed\n");
    return ERROR;
  }

    struct sockaddr_in clientAddress;
    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_port = htons(port_number);  // Specify the destination port here
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY); // inet_addr("127.0.0.1"); - TRY
    socklen_t clientAddressLength = sizeof(clientAddress);

    int socket = RUDP_socket();
    if (socket == -1) {
        printf("faild to open socket\n");
        return ERROR;
    }
    printf("socket opened\n");



    //connect the socket to ip and port
    int ct = RUDP_connect(socket, ip_address, port_number);
    if (ct <= 0) {
        printf("failed to connect\n");
        return ERROR;
    }
    printf("connected\n");
        /*
        check
    */
    char * hello = "hello";
    
    RUDP * packet = (RUDP*)malloc(sizeof(RUDP));
    if (packet == NULL) {
        return -1;
    }   
    memset(packet, 0, sizeof(RUDP));
    strcpy(packet->data, hello);
    packet->flags.ACK = 1;
    int a = sendto(socket, packet, sizeof(RUDP),  0, NULL, 0);
    if (a < 0) {
        printf("error A!!\n");
    }
    //end of simple test


    //create a buffer.
    char *buffer = (char*)malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Failed to allocate memory\n");
        return 1;
    }

    //with the generious help of ChatGPT
    srand(time(NULL)); // Seed the random number generator
    generate_random_buffer(buffer, BUFFER_SIZE); //create the random buffer of size 2MB
    print_buffer(buffer, sizeof(buffer));
    memcpy(packet->data, buffer, strlen(buffer));
    
    int choice;
    do {
       if (RUDP_send(socket, buffer, BUFFER_SIZE, (struct sockaddr*)&serverAddress, &serverAddressLen, (struct sockaddr*)&clientAddress, &clientAddressLength) < 0) {
        printf("failed to send the packet");
        free(buffer);
        RUDP_close(socket, (struct sockaddr_in*)&clientAddress, clientAddressLength);
        return ERROR;
       }

       printf("Send again? 1 - yes | 0 - no\n");
       scanf("%d", &choice);

    }while (choice == 1);

    free(buffer);
    RUDP_close(socket, (struct sockaddr_in*)&clientAddress, clientAddressLength);
    printf("Finished to send data.\nconnection closed.\n");
    return 0;
}


void generate_random_buffer(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = rand() % 256; // Random value between 0 and 255
    }
}


void print_buffer(const char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
}
