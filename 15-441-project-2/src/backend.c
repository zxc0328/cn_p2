#include "backend.h"
#include <stdlib.h>                   //HJadded: stdlib.h for rand() in send_SYN()

#include <time.h>                     //HJdebug: added time.h

void print_state(cmu_socket_t *dst);
// initial  value
double EstimatedRTT = INIT_RTT; // set init EstimatedRTT
double current_timeout = INIT_TIMEOUT;// set inital timeout value


void send_SYN(cmu_socket_t * dst){                                             //HJadded: send_SYN() *all sending of individual flag can be rewritten to switch
  uint32_t ISN;
  char *rsp;
  ISN = rand() % SEQMAX;
  dst->ISN = ISN;
  while(pthread_mutex_lock(&(dst->window.ack_lock)) != 0);
  dst->window.last_ack_received = ISN;
  pthread_mutex_unlock(&(dst->window.ack_lock));
  rsp = create_packet_buf(dst->my_port, ntohs(dst->conn.sin_port), ISN, 0, 
    DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, SYN_FLAG_MASK, 1, 0, NULL, NULL, 0);
  sendto(dst->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
    &(dst->conn), sizeof(dst->conn));
  free(rsp);
  return;
}

void send_ACK(cmu_socket_t * dst){                                            //HJadded: send_ACK()
  char *rsp;
  rsp = create_packet_buf(dst->my_port, ntohs(dst->conn.sin_port), 
  dst->window.last_ack_received, dst->window.last_seq_received + 1, 
  DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
  sendto(dst->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
    &(dst->conn), sizeof(dst->conn));
  free(rsp);
  return;
}


//only used by listener when in SYN_RECVD state and failed to receive ack of syn/ack
void resend_SYNACK(cmu_socket_t * sock){                                         //HJcp1: added resend_SYNACK()
  char* rsp;
  uint32_t seq, ack;                                                  
  socklen_t conn_len = sizeof(sock->conn);
  seq = sock->ISN;
  ack = sock->window.last_seq_received  + 1;
  rsp = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), seq, ack, 
    DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, SYN_FLAG_MASK|ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
  sendto(sock->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
    &(sock->conn), conn_len);
  free(rsp);
  sock->state = SYN_RECVD;
}


void send_FIN(cmu_socket_t * dst){                                            //HJadded: send_FIN()
  char *rsp;
  while(pthread_mutex_lock(&(dst->window.ack_lock)) != 0);
  dst->FSN = dst->window.last_ack_received;
  pthread_mutex_unlock(&(dst->window.ack_lock));
  rsp = create_packet_buf(dst->my_port, ntohs(dst->conn.sin_port), 
  dst->FSN, dst->window.last_seq_received + 1, 
  DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, FIN_FLAG_MASK|ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
  sendto(dst->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
    &(dst->conn), sizeof(dst->conn));
  free(rsp);
  return;
}


/* reset function, not needed for CP1
void reset_sock(cmu_socket_t * dst){                                          //HJadded: reset_sock()
  while(pthread_mutex_lock(&(dst->window.ack_lock)) != 0);
  dst->window.last_ack_received = 0;
  pthread_mutex_unlock(&(dst->window.ack_lock));
  dst->window.last_seq_received = 0;
  while(pthread_mutex_lock(&(dst->recv_lock)) != 0);
  free(dst->received_buf);
  dst->received_buf = NULL;
  dst->received_len = 0;
  pthread_mutex_unlock(&(dst->recv_lock));
  while(pthread_mutex_lock(&(dst->send_lock)) != 0);
  free(dst->sending_buf);
  dst->sending_buf = NULL;
  dst->sending_len = 0;
  pthread_mutex_unlock(&(dst->send_lock));
  while(pthread_mutex_lock(&(dst->death_lock)) != 0);
  dst->dying = FALSE;
  pthread_mutex_unlock(&(dst->death_lock));
  dst->ISN = 0;
  dst->FSN = 0;
  return;
}
*/

/*
 * Param: sock - The socket to check for acknowledgements. 
 * Param: seq - Sequence number to check 
 *
 * Purpose: To tell if a packet (sequence number) has been acknowledged.
 *
 */
int check_ack(cmu_socket_t * sock, uint32_t seq){
  int result;
  while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
  if(sock->window.last_ack_received > seq)
    result = TRUE;
  else
    result = FALSE;
  pthread_mutex_unlock(&(sock->window.ack_lock));
  return result;
}

/*
 * Param: sock - The socket used for handling packets received
 * Param: pkt - The packet data received by the socket
 *
 * Purpose: Updates the socket information to represent
 *  the newly received packet.
 *
 * Comment: This will need to be updated for checkpoints 1,2,3
 *
 */
void handle_message(cmu_socket_t * sock, char* pkt){
  char* rsp;
  uint8_t flags = get_flags(pkt);
  uint32_t data_len, seq, ack;                                                   //HJadded: added var: ack
  socklen_t conn_len = sizeof(sock->conn);
  switch(flags){                                                                 //HJadded: more flags: syn, syn_ack, fin, fin_ack
    case SYN_FLAG_MASK:

      if(sock->state != LISTEN && sock->state != SYN_RECVD)
        break;

      seq = rand() % SEQMAX;
      sock->ISN = seq;
      while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
      sock->window.last_ack_received = seq;
      pthread_mutex_unlock(&(sock->window.ack_lock));

      sock->window.last_seq_received = get_seq(pkt);
      ack = get_seq(pkt) + 1;
      rsp = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), seq, ack, 
        DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, SYN_FLAG_MASK|ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
      sendto(sock->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
        &(sock->conn), conn_len);
      free(rsp);
      sock->state = SYN_RECVD;
      break;

    case SYN_FLAG_MASK|ACK_FLAG_MASK:                                 //what if SYN+ACK is received but ack number is wrong?

      if((sock->state != SYN_SENT && sock->state != ESTABLISHED) || get_ack(pkt) != sock->ISN + 1)

        break;

      sock->window.last_seq_received = get_seq(pkt);
      while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
      sock->window.last_ack_received = get_ack(pkt);
      pthread_mutex_unlock(&(sock->window.ack_lock));
      send_ACK(sock);
      sock->state = ESTABLISHED;
      break;
    
    case FIN_FLAG_MASK|ACK_FLAG_MASK:
printf("backend.c: handlemsg(): pkt ack is: %u, my FSN+1 is: %u, my state is", get_ack(pkt), sock->FSN + 1);print_state(sock);
      if(get_ack(pkt) == sock->FSN + 1){//FIN/ACK
  printf("backend.c: handle msg(): I reced a FIN|ACK\n");

        switch (sock->state){
          case FIN_WAIT_1:
            sock->window.last_seq_received = get_seq(pkt);
            while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
            sock->window.last_ack_received = get_ack(pkt);
            pthread_mutex_unlock(&(sock->window.ack_lock));
            send_ACK(sock);
            sock->state = TIME_WAIT;
            break;

          case FIN_WAIT_2:
            sock->window.last_seq_received = get_seq(pkt);
            send_ACK(sock);
            sock->state = TIME_WAIT;
            break;

          case TIME_WAIT:
          case CLOSING:
            send_ACK(sock);
            break;

          default:
            break;
        }
      }else{//normal FIN
        switch(sock->state){
          case ESTABLISHED:
            sock->window.last_seq_received = get_seq(pkt);
            send_ACK(sock);
            sock->state = CLOSE_WAIT;
            while(pthread_mutex_lock(&(sock->death_lock)) !=  0);
            sock->dying = TRUE;
            pthread_mutex_unlock(&(sock->death_lock));
            break;

          case FIN_WAIT_1:
            sock->window.last_seq_received = get_seq(pkt);
            send_ACK(sock);
            sock->state = CLOSING;
            break;

          case CLOSE_WAIT:
          case CLOSING:
            send_ACK(sock);
            break;
          
          default:
            break;
        }//end of else switch
      }
      break;

    case ACK_FLAG_MASK:                                             //HJadded: edited behavior to respond to specialized ACK flag 
      if(get_ack(pkt) > sock->window.last_ack_received){
          while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
          sock->window.last_ack_received = get_ack(pkt);
          pthread_mutex_unlock(&(sock->window.ack_lock));
        }
      if(get_plen(pkt) == get_hlen(pkt)){//just an ACK, if data+ACK, go to default
        if(get_ack(pkt) == sock->FSN + 1)
        {
          switch(sock->state){
            case FIN_WAIT_1:
              sock->state = FIN_WAIT_2;
              break;

            case CLOSING:
              sock->state = TIME_WAIT;
              break;
            
            case LAST_ACK:
              sock->state = CLOSED;
              break;
            
            default:
              break;
          }
        }
        if(get_ack(pkt) == sock->ISN + 1){
          if(sock->state == SYN_RECVD){
            sock->state = ESTABLISHED;
          }
        }
        break;
      }
    default:
      if(get_ack(pkt) == sock->FSN + 1){
        switch(sock->state){
          case FIN_WAIT_1:
            sock->state = FIN_WAIT_2;
            break;

          case CLOSING:
            sock->state = TIME_WAIT;
            break;
          
          default:
            break;
        }
      }
      else if(get_ack(pkt) == sock->ISN + 1){
        if(sock->state == SYN_RECVD)
          sock->state = ESTABLISHED;
      }

      if(sock->state != ESTABLISHED && sock->state != FIN_WAIT_1 && sock->state != FIN_WAIT_2 && sock->state != CLOSING && sock->state != TIME_WAIT)
        break;

      
      rsp = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), get_ack(pkt), get_seq(pkt) + 1, 
        DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
      sendto(sock->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
        &(sock->conn), conn_len);
      free(rsp);
      seq = get_seq(pkt);
      if( seq > sock->window.last_seq_received){                  //HJadded: deleted OR condition: || (seq == 0 && sock->window.last_seq_received == 0)
        sock->window.last_seq_received = seq;
        data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;
        if(sock->received_buf == NULL){
          sock->received_buf = malloc(data_len);
        }
        else{
          sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
        }
        memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
        sock->received_len += data_len;
      }
      break;
  }
}

/*
 * Param: sock - The socket used for receiving data on the connection.
 * Param: flags - Signify different checks for checking on received data.
 *  These checks involve no-wait, wait, and timeout.
 *
 * Purpose: To check for data received by the socket. 
 *
 */

int check_for_data(cmu_socket_t * sock, int flags, double timeout_value){               //HJadded: changed from void to int, and func behavior

  char hdr[DEFAULT_HEADER_LEN];
  char* pkt;
  socklen_t conn_len = sizeof(sock->conn);
  ssize_t len = 0;
  uint32_t plen = 0, buf_size = 0, n = 0;
  fd_set ackFD;
  struct timeval time_out;

  // transform double to tv_sec and tv_usec
  int intpart = (int)timeout_value;
  double decpart = timeout_value - intpart;
  time_out.tv_sec = (long)intpart;
  time_out.tv_usec = (long)(decpart*1000000);
  // sec to usec, multiply 1000000, ex: 0.5 sec => 500000 usec



  while(pthread_mutex_lock(&(sock->recv_lock)) != 0);
  switch(flags){
    case NO_FLAG:
      len = recvfrom(sock->socket, hdr, DEFAULT_HEADER_LEN, MSG_PEEK,
                (struct sockaddr *) &(sock->conn), &conn_len);
      break;
    case TIMEOUT:
      FD_ZERO(&ackFD);
      FD_SET(sock->socket, &ackFD);
      if(select(sock->socket+1, &ackFD, NULL, NULL, &time_out) <= 0){
        pthread_mutex_unlock(&(sock->recv_lock));
        return EXIT_TIMEOUT;
      }
    case NO_WAIT:
      len = recvfrom(sock->socket, hdr, DEFAULT_HEADER_LEN, MSG_DONTWAIT | MSG_PEEK,
               (struct sockaddr *) &(sock->conn), &conn_len);
      break;
    default:
      perror("ERROR unknown flag");
      return EXIT_ERROR;
  }
  if(len >= DEFAULT_HEADER_LEN){
    plen = get_plen(hdr);
    pkt = malloc(plen);
    while(buf_size < plen ){
        n = recvfrom(sock->socket, pkt + buf_size, plen - buf_size, 
          NO_FLAG, (struct sockaddr *) &(sock->conn), &conn_len);
      buf_size = buf_size + n;
    }
    handle_message(sock, pkt);
    free(pkt);
  }
  pthread_mutex_unlock(&(sock->recv_lock));
  return EXIT_SUCCESS;
}


// measure time diff between start and end
double diff(struct timeval start,struct timeval end)
{
    struct timeval temp;
    if ((end.tv_usec-start.tv_usec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_usec = 1000000+end.tv_usec-start.tv_usec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_usec = end.tv_usec-start.tv_usec;
    }
    long cur_time = 1000000 * temp.tv_sec + temp.tv_usec;
    double sec = cur_time / 1000000.0;
    return sec;
}


// add timeout as input value to do adaptive retransmission 
void check_for_data_m(cmu_socket_t * sock, int flags, double timeout_value){
  char hdr[DEFAULT_HEADER_LEN];
  char* pkt;
  socklen_t conn_len = sizeof(sock->conn);
  ssize_t len = 0;
  uint32_t plen = 0, buf_size = 0, n = 0;
  fd_set ackFD;
  // only useful if falgs=TIMOUT
  struct timeval time_out;
  // transform double to tv_sec and tv_usec
  int intpart = (int)timeout_value;
  double decpart = timeout_value - intpart;
  time_out.tv_sec = (long)intpart;
  time_out.tv_usec = (long)(decpart*1000000);
  // sec to usec, multiply 1000000, ex: 0.5 sec => 500000 usec

  while(pthread_mutex_lock(&(sock->recv_lock)) != 0);
  switch(flags){
    case NO_FLAG:
      len = recvfrom(sock->socket, hdr, DEFAULT_HEADER_LEN, MSG_PEEK,
                (struct sockaddr *) &(sock->conn), &conn_len);
      break;
    case TIMEOUT:
      printf("timeout: %lu sec : %lu usec \n", time_out.tv_sec,time_out.tv_usec);
      FD_ZERO(&ackFD);
      FD_SET(sock->socket, &ackFD);
      if(select(sock->socket+1, &ackFD, NULL, NULL, &time_out) <= 0){
        break;
      }
    case NO_WAIT:
      len = recvfrom(sock->socket, hdr, DEFAULT_HEADER_LEN, MSG_DONTWAIT | MSG_PEEK,
               (struct sockaddr *) &(sock->conn), &conn_len);
      break;
    default:
      perror("ERROR unknown flag");
  }
  if(len >= DEFAULT_HEADER_LEN){
    plen = get_plen(hdr);
    pkt = malloc(plen);
    while(buf_size < plen ){
        n = recvfrom(sock->socket, pkt + buf_size, plen - buf_size, 
          NO_FLAG, (struct sockaddr *) &(sock->conn), &conn_len);
      buf_size = buf_size + n;
    }
    handle_message(sock, pkt);
    free(pkt);
  }
  pthread_mutex_unlock(&(sock->recv_lock));
}

/*
 * Param: sock - The socket to use for sending data
 * Param: data - The data to be sent
 * Param: buf_len - the length of the data being sent
 *
 * Purpose: Breaks up the data into packets and sends a single 
 *  packet at a time.
 *
 * Comment: This will need to be updated for checkpoints 1,2,3
 *
 */
// void single_send_old(cmu_socket_t * sock, char* data, int buf_len){
//     char* msg;
//     char* data_offset = data;
//     int sockfd, plen;
//     size_t conn_len = sizeof(sock->conn);
//     uint32_t seq;

//     sockfd = sock->socket; 
//     if(buf_len > 0){
//       while(buf_len != 0){
//         seq = sock->window.last_ack_received;
//         if(buf_len <= MAX_DLEN){
//             plen = DEFAULT_HEADER_LEN + buf_len;
//             msg = create_packet_buf(sock->my_port, sock->their_port, seq, seq, 
//               DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, NULL, data_offset, buf_len);
//             buf_len = 0;
//           }
//           else{
//             plen = DEFAULT_HEADER_LEN + MAX_DLEN;
//             msg = create_packet_buf(sock->my_port, sock->their_port, seq, seq, 
//               DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, NULL, data_offset, MAX_DLEN);
//             buf_len -= MAX_DLEN;
//           }
//         while(TRUE){
//           sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
//           check_for_data(sock, TIMEOUT);
//           if(check_ack(sock, seq))
//             break;
//         }
//         data_offset = data_offset + plen - DEFAULT_HEADER_LEN;
//       }
//     }
// }



/*
 * Param: sock - The socket to use for sending data
 * Param: data - The data to be sent
 * Param: buf_len - the length of the data being sent
 *
 * Purpose: Breaks up the data into packets and sends a single 
 *  packet at a time.
 *
 * Comment: This will need to be updated for checkpoints 1,2,3
 *
 */
void single_send(cmu_socket_t * sock, char* data, int buf_len){
    char* msg;
    char* data_offset = data;
    int sockfd, plen;
    size_t conn_len = sizeof(sock->conn);
    uint32_t seq;

    // set var to calculate RTT
    double SampleRTT;
    struct timeval start, end, cur;
    //set var for timeout operation
    double temp_timeout;
    double used_time;


    sockfd = sock->socket; 
    if(buf_len > 0){
      while(buf_len != 0){
        seq = sock->window.last_ack_received;
  printf("backend.c: singlesend(): window.last.seq is: %u, window.last.ack is: %u\n", sock->window.last_seq_received, sock->window.last_ack_received);
        if(buf_len <= MAX_DLEN){
            plen = DEFAULT_HEADER_LEN + buf_len;
            msg = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), seq, sock->window.last_seq_received + 1, 
              DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, 1, 0, NULL, data_offset, buf_len);
            buf_len = 0;
          }
          else{
            plen = DEFAULT_HEADER_LEN + MAX_DLEN;
            msg = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), seq, sock->window.last_seq_received + 1, 
              DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, 1, 0, NULL, data_offset, MAX_DLEN);
            buf_len -= MAX_DLEN;
          }

        // count retransmit time for Karn/Partridge Algorithm
        int retransmit_cnt = 0;
        // set current packet start timer to measure RTT and to check if timeout happen 
        gettimeofday(&start, NULL);
        // set temp_timeout for first transmition (temp_timeout is timeout for current transmition)
        temp_timeout = current_timeout;
        used_time = 0.0;
        while(TRUE){
          
          // send packet
          sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);

          //waiting reply for (temp_timeout - used_time) seconds 
          //(note: any packet, even is not ack, can make check_for_data return,
          // return does not mean we get ack)
          check_for_data(sock, TIMEOUT, temp_timeout - used_time);
        
          // if last_ack_received > seq, it means data is acked
          if(check_ack(sock, seq)){
            // if there is no retrainsmition, update EstimatedRTT and timeout
            if (retransmit_cnt==0){
              //set current packet end timer
              gettimeofday(&end, NULL);
              SampleRTT = diff(start, end);
              printf("RTT is: %f for current packet.\n", SampleRTT);

              // for the first EstimatedRTT update, set EstimatedRTT = SampleRTT 
              if (EstimatedRTT==0.0){ 
                EstimatedRTT = SampleRTT;
              } else {
                EstimatedRTT = 0.9*EstimatedRTT+ 0.1*SampleRTT;
              }

              current_timeout = EstimatedRTT*2.0;  
            }
            //data is acked by receiver, break while loop and move to next packet
            break;
          
          // if data is not received (last_ack_received <= seq)
          } else {
            //measure cur time related to start of transmition
            gettimeofday(&cur, NULL);
            used_time = diff(start, cur);
            // if timeouted
            if (temp_timeout - used_time <= 0.0) {
              //because of retransmition, update var
              retransmit_cnt+=1;
              gettimeofday(&start, NULL);

              // Karn  Algorithm
              //current_timeout = current_timeout*2;

              temp_timeout = current_timeout;
              used_time = 0.0;
              printf("retransmit_cnt is: %i, temp_timeout is %f\n", retransmit_cnt,temp_timeout);
            }

          }
        
        }// end of while(TRUE) loop


        data_offset = data_offset + plen - DEFAULT_HEADER_LEN;
      }
    }
}


void print_state(cmu_socket_t *dst){
  switch(dst->state){
    case CLOSED:
      printf("state: CLOSED\n");
      break;
    case LISTEN:
      printf("state: LISTEN\n");
      break;
    case SYN_SENT:
      printf("state: SYN_SENT\n");
      break;
    case SYN_RECVD:
      printf("state: SYN_RECVD\n");
      break;
    case ESTABLISHED:
      printf("state: ESTABLISHED\n");
      break;
    case FIN_WAIT_1:
      printf("state: FIN_WAIT_1\n");
      break;
    case FIN_WAIT_2:
      printf("state: FIN_WAIT_2\n");
      break;
    case CLOSING:
      printf("state: CLOSING\n");
      break;
    case TIME_WAIT:
      printf("state: TIME_WAIT\n");
      break;
    case CLOSE_WAIT:
      printf("state: CLOSE_WAIT\n");
      break;
    case LAST_ACK:
      printf("state: LAST_ACK\n");
      break;
  }
}


void handshake(cmu_socket_t * dst){                       //HJadd: handshake() to be used at the beginning of begin_backend()
  while(dst->state != ESTABLISHED){
printf("starting ");print_state(dst);

    switch(dst->state){
      case CLOSED:
        send_SYN(dst);
        dst->state = SYN_SENT;
        break;
      
      case SYN_SENT:

        if(check_for_data(dst, TIMEOUT, current_timeout) != 0 ){
          send_SYN(dst);
        }else{
          if(dst->state == SYN_SENT){
            send_SYN(dst);
          }
        }
        break;
      
      case LISTEN:
        check_for_data(dst, NO_FLAG, current_timeout);
        break;

      case SYN_RECVD:
        if(check_for_data(dst, TIMEOUT, current_timeout) != 0){
          resend_SYNACK(dst);
        }else{
          if(dst->state == SYN_RECVD)
          resend_SYNACK(dst);
        }

        break;
      
      default:
        break;
    }//end of switch statement

printf("ending ");print_state(dst);

  }//end of while loop
  return;
}

void teardown(cmu_socket_t * dst){                          //HJadded: teardown process to be used in back_end()

  while(dst->state != CLOSED){
    switch (dst->state)
    {
    case ESTABLISHED:
      send_FIN(dst);
      dst->state = FIN_WAIT_1;
      break;

    case FIN_WAIT_1:

      if(check_for_data(dst, TIMEOUT, current_timeout) != EXIT_SUCCESS)//what to do if timeout here? current answer: resend FIN

        send_FIN(dst);
      break;

    case FIN_WAIT_2:

      check_for_data(dst, NO_WAIT, current_timeout);
      break;

    case CLOSING:
      if(check_for_data(dst, TIMEOUT, current_timeout) != EXIT_SUCCESS)//what to do if timeout here? current answer: resend FIN
        send_FIN(dst);
      break;

    case TIME_WAIT:                                       //what is the value of 2xMSL?

      if(check_for_data(dst, TIMEOUT, 2 * MAX_SEG_LIFE) == EXIT_TIMEOUT)
        dst->state = CLOSED;
      break;
    
    case CLOSE_WAIT:
      send_FIN(dst);
      dst->state = LAST_ACK;
      break;

    case LAST_ACK:

      if(check_for_data(dst, TIMEOUT, current_timeout) != EXIT_SUCCESS)//what to do if timeout here? current answer: resend FIN
      {
        send_FIN(dst);
printf("backend.c: teardown(): resent FIN\n");
      }else
      {
        if(dst->state == LAST_ACK)
          send_FIN(dst);
      }
      
      break;
    
    case SYN_RECVD:
      send_FIN(dst);
      dst->state = FIN_WAIT_1;
      break;
    
    default:
      break;
    }//end of switch statement

print_state(dst);

  }//end of while loop
  return;
}


/*
 * Param: in - the socket that is used for backend processing
 *
 * Purpose: To poll in the background for sending and receiving data to
 *  the other side. 
 *
 */
void* begin_backend(void * in){
  cmu_socket_t * dst = (cmu_socket_t *) in;
  int death, buf_len, send_signal;
  char* data;


  srand(time(NULL));
print_state(dst);

  handshake(dst);                                 //HJadded: execute handshake process, which loops until ESTABLISHED state is reached

  while(TRUE){
    while(pthread_mutex_lock(&(dst->death_lock)) !=  0);
    death = dst->dying;
    pthread_mutex_unlock(&(dst->death_lock));
    
    
    while(pthread_mutex_lock(&(dst->send_lock)) != 0);
    buf_len = dst->sending_len;

    if(death && buf_len == 0){                  //HJadded: when this condition is reached, execute teardown() until CLOSED state is reached
      teardown(dst);
      break;
    }
      
    if(buf_len > 0){
      data = malloc(buf_len);
      memcpy(data, dst->sending_buf, buf_len);
      dst->sending_len = 0;
      free(dst->sending_buf);
      dst->sending_buf = NULL;
      pthread_mutex_unlock(&(dst->send_lock));
      single_send(dst, data, buf_len);
      free(data);
    }
    else
      pthread_mutex_unlock(&(dst->send_lock));
    check_for_data(dst, NO_WAIT, current_timeout);

    
    while(pthread_mutex_lock(&(dst->recv_lock)) != 0);
    
    if(dst->received_len > 0)
      send_signal = TRUE;
    else
      send_signal = FALSE;
    pthread_mutex_unlock(&(dst->recv_lock));
    
    if(send_signal){
      pthread_cond_signal(&(dst->wait_cond));  
    }
  }


  pthread_exit(NULL); 
  return NULL; 
}
