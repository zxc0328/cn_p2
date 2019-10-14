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

typedef struct {
	uint32_t last_seq_received;
	uint32_t last_ack_received;
	pthread_mutex_t ack_lock;
} window_t;

typedef enum states{				//HJadded: enum states
	CLOSED,
    LISTEN,
    SYN_SENT,
    SYN_RECVD,
    ESTABLISHED,
	FIN_WAIT_1,
	FIN_WAIT_2,
	CLOSING,
	TIME_WAIT,
	CLOSE_WAIT,
	LAST_ACK,
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