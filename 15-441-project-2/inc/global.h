#include "grading.h"
#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define EXIT_SUCCESS 0
#define EXIT_ERROR -1
#define EXIT_FAILURE 1
#define EXIT_TIMEOUT 2				//HJadded: EXIT_TIMEOUT = 2

#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2

#define TRUE 1
#define FALSE 0

#define SEQMAX 0xffffffff			//HJadded: max sequense number

typedef struct out_of_order_pkt out_of_order_pkt;

// transmssion states
typedef enum reno_states{
	SLOW_START,
	CONGESTION_AVOIDANCE,
	FAST_RECOVERY
}reno_states;

// data struct for out of order packet
struct out_of_order_pkt {
	uint32_t seq;
	uint16_t data_len;
	struct out_of_order_pkt *next;
};


typedef struct {
	uint32_t last_seq_received;
	uint32_t last_ack_received;
	pthread_mutex_t ack_lock;
	char * last_byte_acked;
	char * last_byte_sent; // this actually points to next byte to be sent
	char * last_byte_written;
	char * last_byte_read;
	char * next_byte_expected;
	char * last_byte_received;
	uint16_t advertised_window;
	uint16_t my_window_to_advertise;
	enum reno_states transmission_state; // should be inited to SLOW_START
	int dup_ACK_count;
	size_t recving_buf_begining_seq; //this is constantly updated. shows what seq number the first byte in recving buffer is
	out_of_order_pkt *out_of_order_queue;
} window_t;

typedef enum states{				//HJadded: enum states
	CLOSED,//0
    LISTEN,//1
    SYN_SENT,//2
    SYN_RECVD,//3
    ESTABLISHED,//4
	FIN_WAIT_1,//5
	FIN_WAIT_2,//6
	CLOSING,//7
	TIME_WAIT,//8
	CLOSE_WAIT,//9
	LAST_ACK,//10
}states;





typedef struct {
	int socket;   
	pthread_t thread_id;
	uint16_t my_port;
	uint16_t their_port;
	struct sockaddr_in conn;
	char* received_buf;
	int received_len;
	pthread_mutex_t recv_lock;
	pthread_cond_t wait_cond;
	char* sending_buf;
	int sending_len;
	int type;
	pthread_mutex_t send_lock;
	int dying;
	pthread_mutex_t death_lock;
	window_t window;
	enum states state;					//HJadded: attribute state, ISN, FSN
	uint32_t ISN;
	uint32_t FSN;
} cmu_socket_t;

#endif