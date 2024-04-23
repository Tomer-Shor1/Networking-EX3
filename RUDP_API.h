#include <netinet/in.h>
#define SUCCESS 1
#define FAILURE 0
#define ERROR -1

#define RETRY 14
#define PACKET_MAX_SIZE 60000

typedef struct RUDP_flags {
  unsigned int SYN : 1;
  unsigned int ACK : 1;
  unsigned int DATA : 1;
  unsigned int FIN : 1;
} RUDP_flags;

typedef struct _RUDP {
  RUDP_flags flags;
  int seq_num;
  int checksum;
  int length;  // the length of the data
  char data[PACKET_MAX_SIZE];
} RUDP;

/*
  cerates RUDP socket
*/
int RUDP_socket();

/*
  *creating RUDP socket
  @ return the socket number 
  * if fails return error 
*/



/*
    *connect to socket - sender side.
    @return 1 if succes, 0 if failed.
*/
int RUDP_connect(int socket, char *ip, int port);

/*
    *listening for unpcoming connection requests.
    @returns 1 if succes, 0 if fails.
*/
int RUDP_wait_for_connection(int socket, int port);


/*
    *send data to the peer on RUDP.
    @param socket - the socket to send on.
    @param data - the data to send.
    @param data_length - the length of the data
    @returns 1 if received data packet , 0 if receives syn/ack packet, 2 if its the last data packet, 
            -1 if fails to receive, 2 if receives close request.
*/
int RUDP_send(int socket, char *data, int data_length, struct sockaddr *serverAddress, socklen_t *serverAddressLen, struct sockaddr *clientAddress, socklen_t *clientAddressLen);


/*
    *receive data from a peer.
    @param socket - the socket to receive from
    @param data - the data to receive 
    @param date_length - the length of the data
    @returns 1 if success, 0 if fails
*/
int RUDP_receive(int socket, int port, char **data, int *data_length);

/*
  closes RUDP socket
*/
int RUDP_close(int socket, struct sockaddr_in *clientAddress, socklen_t clientAddressLen);

