#include <netinet/in.h>
#define SUCCESS 1
#define FAILURE 0
#define ERROR -1

#define RETRY 30
#define PACKET_MAX_SIZE 1024 //changeable


/*
    *the struct represent RUDP flags.
    @flag SYN - 1 if SYN is on , otherwise 0.
    @flag ACK - 1 if ACK is on , otherwise 0.
    @flag DATA - 1 if a packet contains data, otherwise 0.
    @flag FIN - 1 if its the last packet to send, otherwise 0.
*/
typedef struct RUDP_flags {
  unsigned int SYN : 1;
  unsigned int ACK : 1;
  unsigned int DATA : 1;
  unsigned int FIN : 1;
} RUDP_flags;


/*
    *The struct represents RUDP packet.
    @flags - RUDP packet's flags as mentioned in RUDP_flags struct
    @seq_num - packets sequence number.
    @lenght - the lenght of the data
    @data - the buffer storing the packet's data. it's maximum size is PACKET_MAX_SIZE
*/
typedef struct _RUDP {
  RUDP_flags flags;
  int seq_num;
  int checksum;
  char data[PACKET_MAX_SIZE];
  int length;  // the length of the data
} RUDP;




/*
  *creating RUDP socket
  @ return the socket number 
  * if fails return error 
*/
int RUDP_socket();


/*
    *connect to server - sender side.
    *NOTE that after connection the protocol is no longer connectionless like UDP.
    @param socket - the socket to connect on.
    @param ip - user's IP address.
    @param port - user's port number - has to be identical to server's port number.
    *return 1 if succes, 0 if failed, -1 if error accured in running time.
*/
int RUDP_connect(int socket, char *ip, int port);


/*
    *listening for unpcoming connection requests.
    @param socket - the socket to listen on.
    @param port - the port of upcoming requests.
    *returns 1 if succes, 0 if fails.
*/
int RUDP_listen(int socket, int port);


/*
    *send data to the peer on RUDP.
    @param socket - the socket to send on.
    @param data - the data to send.
    @param data_length - the length of the data
    @returns 1 if success, 0 if fails

*/
int RUDP_send(int socket, char *data, int data_length);


/*
    *receive data from a peer.
    @param socket - the socket to receive from
    @param data - the data to receive 
    @param date_length - the length of the data
    @returns -1 if fails to receive, -2 if receives close request,
             , 0 if receives syn/ack packet, 1 if received data packet, 2 if its the last data packet, 
            
*/
int RUDP_receive(int socket, char **data, int *data_length);

/*
  closes RUDP socket
*/
int RUDP_close(int socket);


/*
  print buffer function - mostly used for debugging 
  @param buffer - the buffer to print
  @param size - size of the buffer
*/
void print_buffer(const char *buffer, size_t size);
