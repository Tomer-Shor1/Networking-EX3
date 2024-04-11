#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "RUDP_API.h"

#define BUFFER_SIZE (2 * 1024 * 1024) // 2MB


/*
    generates a random buffer of a given size. max 2MB 

*/
void generate_random_buffer(char *buffer, size_t size);



int main(int argc, char* argv[]){
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

    int socket = RUDP_socket();
    if (socket == -1) {
        printf("faild to open socket\n");
        return ERROR;
    }

    //connect the socket to ip and port
    int ct = RUDP_connect(socket, ip_address, port_number);
    if (ct <= 0) {
        printf("failed to connect\n");
        return ERROR;
    }

    //create a buffer.
    char *buffer = (char*)malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Failed to allocate memory\n");
        return 1;
    }

    srand(time(NULL)); // Seed the random number generator
    generate_random_buffer(buffer, BUFFER_SIZE); //create the random buffer of size 2MB

    int choice;
    do {
       if (RUDP_send(socket, buffer, BUFFER_SIZE) < 0) {
        printf("failed to send the packet");
        free(buffer);
        RUDP_close(socket);
        return ERROR;
       }

       printf("Send again? 1 - yes | 0 - no\n");
       scanf("%d", &choice);

    }while (choice == 1);

    free(buffer);
    RUDP_close(socket);
    printf("connection closed\n");
    return 0;
}


void generate_random_buffer(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = rand() % 256; // Random value between 0 and 255
    }
}