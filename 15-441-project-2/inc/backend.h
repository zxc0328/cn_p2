#ifndef _CMU_BACK_H_
#define _CMU_BACK_H_
#include "cmu_tcp.h"
#include "global.h"
#include "cmu_packet.h"

#include <time.h>
int check_ack(cmu_socket_t * dst, uint32_t seq);
int check_for_data(cmu_socket_t * sock, int flags, double timeout_usec);
void send_and_receive_data(cmu_socket_t * dst, char* data, int buf_len);
void * begin_backend(void * in);
void check_for_data_m(cmu_socket_t * sock, int flags, double timeout_usec);
// measure time diff
double diff(struct timeval start, struct timeval end);
// congestion control helper
void cc_helper(cmu_socket_t * sock);
#define INIT_RTT 0.0
#define INIT_TIMEOUT 0.1
#define MAX_SEG_LIFE 2
#define NO_DATA_SENT 0U
#define ZERO_ADV_WIND_DETECTED 1
#endif

