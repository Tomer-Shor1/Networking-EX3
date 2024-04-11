#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "RUDP_API.h"

int main(int argc, char* argv[]){
    int port = atoi(argv[2]);  //convert port string into integer
    //printf("%d", port);

    printf("-------- RUDP Receiver --------\n");
    int socket = RUDP_socket();
    if (socket == -1) {
        printf("failed to create socket");
        return ERROR;
    }

    //connect the socket and port
    int rc = RUDP_wait_for_connection(socket, port);
    if (rc == -1) {
        printf("failed to connect");
        return ERROR;
    }

    //create file for saving the stats
    FILE* file = fopen("stats", "w+");
    if (file == NULL) {
    printf("Error opening  stats file");
    return 1;
    }


    double average_time = 0;
    double average_speed = 0;
    clock_t start, end;

    char* data = NULL;
    int data_length = 0;
    char total_data[2097152] = {0};  // 2MB

    start = clock();
    end = clock();
    int number_of_runs = 0;

    do {
        int rc = RUDP_receive(socket, &data, &data_length);
        if (rc == -1) {
            printf("error. failed to receive");
            return ERROR;
        }

        if (rc == -2) {  //if the connection was closed by the sender
            break;
        }

        if (rc == 1 && start < end) {  // if its the first data packet, start the timer
            start = clock();
        }

        if (rc == 1) {  //if its data packet, add it to the total data
            strcat(total_data, data);
        }

        if (rc == 2) { //if received a last packet of data
            strcat(total_data, data);
            printf("Received total data: %zu\n", sizeof(total_data));
            end = clock();
            double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
            average_time += time_taken;
            double speed = 2 / time_taken;
            average_speed += speed;
            fprintf(file, "Run #%d Data: Time=%f S ; Speed=%f MB/S\n", number_of_runs,
              time_taken, speed);  //write stats to the file
            memset(total_data, 0, sizeof(total_data));  
            number_of_runs++;
        }
    }while (rc >= 0);

    //add stats to the file
    fprintf(file, "\n");
    fprintf(file, "Average time: %f S\n", average_time / (number_of_runs - 1));
    fprintf(file, "Average speed: %f MB/S\n", average_speed / (number_of_runs - 1));

    //print the file
    rewind(file);
    char print_buffer[100];
    while (fgets(print_buffer, 100, file) != NULL) {
    printf("%s", print_buffer);
    }

    fclose(file);
    return 0;
}