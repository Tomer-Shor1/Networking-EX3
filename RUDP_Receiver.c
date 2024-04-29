#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "RUDP_API.h"



int main(int argc, char* argv[]){
    int port = atoi(argv[2]);  // convert port string into integer
    printf("Server's port number is %d\n", port);

    printf("-------- RUDP Receiver --------\n");
    int socket = RUDP_socket();
    if (socket == -1) {
        printf("Failed to create socket\n");
        return ERROR;
    }

    // Listen and save client address
    int rc = RUDP_listen(socket, port);
    if (rc == -1) {
        printf("Failed to connect\n");
        return ERROR;
    }
    printf("Connected\n");

    RUDP * packet = (RUDP*)malloc(sizeof(RUDP));
    if (packet == NULL) {
        return ERROR;
    }  

    // create file for saving the stats
    FILE* file = fopen("stats", "w+");
    if (file == NULL) {
        printf("Error opening  stats file\n");
        return ERROR;
    }

    fprintf(file, "\n\n~~~~~~~~ Stats ~~~~~~~~\n");
    double avg_time = 0;
    double avg_speed = 0;
    clock_t start_time, end_time;

    char* data = NULL;
    char total_data[2097152] = {0};  //2MB
    int data_length = 0;

    start_time = clock();
    end_time = clock();
    int number_of_runs = 0;

    do {
        int rc = RUDP_receive(socket, &data, &data_length);
        if (rc == -1) {
            printf("Error. failed to receive\n");
            return ERROR;
        }
        if (rc == -2) {  // if the connection was closed by the sender - break and print stats.
            break;
        }
        if (rc == 1 && start_time < end_time) {  // if its the first data packet, start the timer.
            start_time = clock();
        }
        if (rc == 1) {  // if its data packet, add it to the total data
            strcat(total_data, data);
        }
        if (rc == 2) { // if received a last packet of data
            strcat(total_data, data);
            printf("Received all data\n");
            end_time = clock();
            double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
            double speed = 2 / time_taken;
            avg_time += time_taken;
            avg_speed += speed;
            fprintf(file, "Run #%d Time=%f S ; Speed=%f MB/S\n", number_of_runs, time_taken, speed);  // write stats to the file
            memset(total_data, 0, sizeof(total_data));  // initilize data
            number_of_runs++;
        }
    }while (rc >= 0);

    // add stats to the file
    fprintf(file, "\n");
    fprintf(file, "Average time: %f S\n", avg_time / (number_of_runs - 1));
    fprintf(file, "Average speed: %f MB/S\n", avg_speed / (number_of_runs - 1));

    //print the file
    rewind(file);
    char print_buffer[100];
    while (fgets(print_buffer, 100, file) != NULL){
    printf("%s", print_buffer);
    }
    fclose(file);
    return 0;
}
