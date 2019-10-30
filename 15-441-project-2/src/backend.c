#include "backend.h"
#include <stdlib.h>                   //HJadded: stdlib.h for rand() in send_SYN()
#include <time.h>                     //HJdebug: added time.h


// initial  value
double EstimatedRTT = INIT_RTT; // set init EstimatedRTT
double current_timeout = INIT_TIMEOUT;// set inital timeout value

// debug: debugging function to print current state of a socket(body in backend.c)
void print_state(cmu_socket_t *dst);

// send a SYN packet: rand() an ISN and writes it to socket
void send_SYN(cmu_socket_t * dst){                                             
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

// resend a SYN packet: reads the ISN on socket
void resend_SYN(cmu_socket_t * dst){                                             
  char *rsp;
  rsp = create_packet_buf(dst->my_port, ntohs(dst->conn.sin_port), dst->ISN, 0, 
    DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, SYN_FLAG_MASK, 1, 0, NULL, NULL, 0);
  sendto(dst->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
    &(dst->conn), sizeof(dst->conn));
  free(rsp);
  return;
}

// send an ACK packet: reads from the socket's window
void send_ACK(cmu_socket_t * dst){                                           
  char *rsp;
  rsp = create_packet_buf(dst->my_port, ntohs(dst->conn.sin_port), 
  dst->window.last_ack_received, dst->window.last_seq_received + 1, 
  DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
  sendto(dst->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
    &(dst->conn), sizeof(dst->conn));
  free(rsp);
  return;
}


// only used by listener when in SYN_RECVD state and failed to receive ack of SYN|ACK
// it reads from socket's window
void resend_SYNACK(cmu_socket_t * sock){                                         
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

// send a FIN|ACK packet, writes the FSN to socket's window
void send_FIN(cmu_socket_t * dst){                                           
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



// insert an entry to out-of-order-queue based on its seq
void insert_out_of_order_queue(cmu_socket_t *sock, char *pkt){
  out_of_order_pkt *tmp, *prev, *current = sock->window.out_of_order_queue;
  tmp = (out_of_order_pkt *)malloc(sizeof(out_of_order_pkt));
  //construct the out-of-order-pkt
  tmp->seq = get_seq(pkt);
  tmp->data_len = get_plen(pkt) - get_hlen(pkt) - get_extension_length(pkt);
  tmp->next = NULL;


  if(current == NULL){
    sock->window.out_of_order_queue = tmp;
    return;
  }
  prev = NULL;
  //traverse the list
  while(current!=NULL){
    if(tmp->seq < current->seq){
      if(sock->window.out_of_order_queue == current){
        sock->window.out_of_order_queue = tmp;
        tmp->next = current;
        return;
      }else
      {
        prev->next = tmp;
        tmp->next = current;
        return;
      }
    }
    prev = current;
    current = current->next;
  }
  //done traversing list, then just add it to tail
  prev->next = tmp;
  return;
}

//
void update_LBRECVD_ptr(cmu_socket_t *sock, char *pkt){
  uint32_t seq = get_seq(pkt);
  uint16_t data_len = get_plen(pkt) - get_hlen(pkt) - get_extension_length(pkt);
  char *this_LBRCVD = sock->window.last_byte_read + (size_t)(seq - sock->window.recving_buf_begining_seq) + (size_t)data_len;
  if( this_LBRCVD > sock->window.last_byte_received){
    sock->window.last_byte_received = this_LBRCVD;
  }
  return;
}

//remove the first entry fro out-of-order-queue
void dequeue_out_of_order_queue(cmu_socket_t *sock){
  if(sock->window.out_of_order_queue == NULL){
    return;
  }else
  {
    out_of_order_pkt *tmp;
    tmp = sock->window.out_of_order_queue;
    sock->window.out_of_order_queue = sock->window.out_of_order_queue->next;
    free(tmp);
  }
  
}

// merge the intervel if there is any, updating LBRCVD
void merge_out_of_order_queue(cmu_socket_t *sock, char *pkt){
  uint16_t pkt_data_len;
  uint32_t newest_seq;
  out_of_order_pkt *current = sock->window.out_of_order_queue;
  //if no entry to merge, return
  if(current == NULL){
    return;
  }

  pkt_data_len = get_plen(pkt) - get_hlen(pkt) - get_extension_length(pkt);
  sock->window.last_byte_received += (size_t)pkt_data_len;
  newest_seq = get_seq(pkt) + (uint32_t)pkt_data_len;
  while(current != NULL){
    if(newest_seq == current->seq){
      //merge the interval by updating LBRCVD and dequeuing the out-of-order queue
      sock->window.last_byte_received += (size_t)current->data_len;
      newest_seq += (uint32_t)current->data_len;
      current = current->next;
      dequeue_out_of_order_queue(sock);
    }
    else
      break;
  }
  return;
}

//update adv_window before sending a response ack
void update_adv_window(cmu_socket_t *sock){
  sock->window.my_window_to_advertise = (uint16_t) my_min(MAX_NETWORK_BUFFER -(size_t)(sock->window.last_byte_received - sock->window.last_byte_read), MAX_NETWORK_BUFFER - 1);
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

      //define seq: first time ISN = rand(), otherwise seq = sock->ISN
      if(sock->state == LISTEN){
        seq = rand() % SEQMAX;
        sock->ISN = seq;
      }else{
        seq = sock->ISN;
      }

      while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
      sock->window.last_ack_received = seq;
      pthread_mutex_unlock(&(sock->window.ack_lock));

      //update the seq the beginning of recving buffer will represent
      sock->window.recving_buf_begining_seq = get_seq(pkt) + 1;

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
      //update the seq the beginning of recving buffer will represent
      sock->window.recving_buf_begining_seq = get_seq(pkt) + 1;
      
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
      
      // check for valid ack number
      if(get_ack(pkt) >= sock->window.last_ack_received){
          // if in data trasmission, received dup ACK, update dup count and break
          if(get_ack(pkt) == sock->window.last_ack_received && sock->state == ESTABLISHED && get_plen(pkt) == get_hlen(pkt)){
                sock->window.dup_ACK_count++;
                break;
            }
          
          while(pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
          sock->window.last_ack_received = get_ack(pkt);
          pthread_mutex_unlock(&(sock->window.ack_lock));
        }
      // update adv_window
      sock->window.advertised_window = get_advertised_window(pkt);

      //case: this is just an ACK
      if(get_plen(pkt) == get_hlen(pkt)){
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
      //else this is data+ACK, go to default
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

      //does not process data while in any state other than following
      if(sock->state != ESTABLISHED && sock->state != FIN_WAIT_1 && sock->state != FIN_WAIT_2 && sock->state != CLOSING && sock->state != TIME_WAIT)
        break;

      seq = get_seq(pkt);
      //check for valid seq and copy it to recving buf 
      if( seq > sock->window.last_seq_received){                
        sock->window.last_seq_received = seq;
        data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;
        if(sock->received_buf == NULL){
          sock->received_buf = (char *) malloc(MAX_NETWORK_BUFFER);
          sock->window.last_byte_read = sock->received_buf;
          sock->window.next_byte_expected = sock->received_buf;
          sock->window.last_byte_received = sock->received_buf;
        }
        //TODO: check for enough space in recving buffer(do we even need it tho?)

        //check the order of received pkt
        if(seq = (size_t)(sock->window.next_byte_expected - sock->window.last_byte_read) + sock->window.recving_buf_begining_seq){
          memcpy(sock->window.next_byte_expected + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
          sock->window.next_byte_expected += data_len;
          merge_out_of_order_queue(sock, pkt);
        }else//this pkt is out of order
        {
          //still copy the data to recving buffer, but need to be at the right location
          memcpy(sock->window.last_byte_read + (seq - sock->window.recving_buf_begining_seq), pkt + DEFAULT_HEADER_LEN, data_len);
          //insert entry to out_of_order_list
          insert_out_of_order_queue(sock, pkt);
          update_LBRECVD_ptr(sock, pkt);
        }
      }
      update_adv_window(sock);
      rsp = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), get_ack(pkt), get_seq(pkt) + 1, 
        DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, sock->window.my_window_to_advertise, 0, NULL, NULL, 0);
      sendto(sock->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) 
        &(sock->conn), conn_len);
      free(rsp);
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
              current_timeout = current_timeout*2;

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

// handshake()
void handshake(cmu_socket_t * dst){                      
  while(dst->state != ESTABLISHED){
printf("starting ");print_state(dst);

    switch(dst->state){
      case CLOSED:
        send_SYN(dst);
        dst->state = SYN_SENT;
        break;
      
      case SYN_SENT:

        if(check_for_data(dst, TIMEOUT, current_timeout) != 0 ){
          resend_SYN(dst);
        }else{
          if(dst->state == SYN_SENT){
            resend_SYN(dst);
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

//teardown process for backend()
void teardown(cmu_socket_t * dst){                         

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

// return the lower of two numbers, used in update_sending_buffer()
size_t my_min(size_t a, size_t b){
  if(a > b)
    return b;
  else 
    return a;
}

// given a socket and the amount of data moved from socket's buffer to tcp's
// sending buf, update the socket information accordingly
void update_socket_buffer(cmu_socket_t *sock, size_t data_moved){
  size_t original_sending_len = sock->sending_len;
  
  if(data_moved == original_sending_len){//if: moved all data in socket buffer
    sock->sending_len = 0;
    free(sock->sending_buf);
    sock->sending_buf = NULL;
  }else//else: adjust the socket buffer by coping what's left to new buffer
  {
    char *new_socket_buf;
    size_t new_sending_len = original_sending_len - data_moved;
    new_socket_buf = (char *) malloc(sizeof(char) * new_sending_len);
    memcpy(new_socket_buf, sock->sending_buf + data_moved, new_sending_len);
    free(sock->sending_buf);
    sock->sending_buf = new_socket_buf;
    sock->sending_len = new_sending_len;
  }
  return;
}

// copy data from application buffer to sending buffer
// pls make sure socket.window is initialized
void update_sending_buffer(char **sending_buffer_addr, cmu_socket_t *sock){
  size_t buf_len, data_copied;
  char * buf_to_send;
  buf_len = sock->sending_len;
  buf_to_send = *sending_buffer_addr;
  // if there is nothing written, just return
  if(buf_len == 0)
    return;

  if(buf_to_send == NULL){
    // sending buffer is NULL, create new sending buffer and move data from socket
    data_copied = my_min(MAX_NETWORK_BUFFER, buf_len);
    buf_to_send = (char *) malloc(sizeof(char) * MAX_NETWORK_BUFFER);
    memcpy(buf_to_send, sock->sending_buf, data_copied);
    sock->window.last_byte_acked = buf_to_send;
    sock->window.last_byte_sent = buf_to_send;
    sock->window.last_byte_written = buf_to_send + data_copied;
    update_socket_buffer(sock, data_copied);

  }else
  {
    size_t data_in_buffer;
    data_in_buffer = sock->window.last_byte_written - buf_to_send;
    // if moving all data would overflow the sending buffer
    // allocate new buffer, and move old data after LBA, and update pointers
    if(data_in_buffer + buf_len > MAX_NETWORK_BUFFER)
    {
      size_t len_in_flight, len_to_keep;
      char *new_buf_to_send = (char *) malloc(sizeof(char) * MAX_NETWORK_BUFFER);
      len_to_keep = sock->window.last_byte_written - sock->window.last_byte_acked;
      len_in_flight = sock->window.last_byte_sent - sock->window.last_byte_acked;
      data_copied = MAX_NETWORK_BUFFER - len_to_keep;
      // copy LBA ~ LBW to new buffer
      memcpy(new_buf_to_send, sock->window.last_byte_acked, len_to_keep);
      // copy data from socket buffer to fill the rest of sending buffer
      memcpy(new_buf_to_send + len_to_keep, sock->sending_buf, data_copied);
      // update the three pointers
      sock->window.last_byte_acked = new_buf_to_send;
      sock->window.last_byte_sent = new_buf_to_send + len_in_flight;
      sock->window.last_byte_written = new_buf_to_send + MAX_NETWORK_BUFFER;
      // update sock info
      update_socket_buffer(sock, data_copied);
      // update sending buffer pointer
      buf_to_send = new_buf_to_send;
    }else// just move all data and update LBW pointer, sock info
    {
      data_copied = buf_len;
      memcpy(sock->window.last_byte_written, sock->sending_buf, data_copied);
      sock->window.last_byte_written += data_copied;
      update_socket_buffer(sock, data_copied);
    }
  }

  // update the sending buffer pointer, length to be passed to my_send()
  *sending_buffer_addr = buf_to_send;
  return;
}

// send data in segments until there is a full window of bytes in flight
void my_send(cmu_socket_t *sock, char *sending_buffer){
  // while(sock->window.last_byte_acked != sock->window.last_byte_written){
    // switch(sock->window.transmission_state){
    //   case SLOW_START:
    //     if(sock->window.cwnd >= WINDOW_INITIAL_SSTHRESH)
    //     {
    //       sock->window.transmission_state = CONGESTION_AVOIDANCE;
    //       continue;
    //     }
    //     send_full_real_window(sock, sending_buffer);

    //     break;

    //   case CONGESTION_AVOIDANCE:
    //     break;

    //   case FAST_RECOVERY:
    //     break;
    // }
    
  //}
  //if nothing to send, just return
  if(sending_buffer == NULL)
    return;

  //if triple-ack exist, resend first packet in window
  if(sock->window.dup_ACK_count == 3){
    resend_LBA_packet(sock);
    sock->window.dup_ACK_count = 0;
    return;
  }
  //if sent everything already, just return
  if(sock->window.last_byte_sent == sock->window.last_byte_written)
    return;

  if(sock->window.advertised_window == 0){
    uint32_t seq = ((uint32_t)(sock->window.last_byte_sent - sock->window.last_byte_acked)) + sock->window.last_ack_received;
    char *offset_1B = sock->window.last_byte_sent;
    sock->window.last_byte_sent++;
    //keep sending 1 byte until received adv_win != 0
    while(sock->window.advertised_window == 0){
    send_1B_data(sock, offset_1B, seq);
    check_for_data(sock, TIMEOUT, current_timeout);
    }
  }
  send_full_real_window(sock);
  return;
}

//send the same one byte of data for adv_window probing
void send_1B_data(cmu_socket_t *sock, char* data, uint32_t seq){
  char* msg;
  int sockfd, plen;
  size_t conn_len = sizeof(sock->conn);
  sockfd = sock->socket;
  plen = DEFAULT_HEADER_LEN + 1;
  msg = create_packet_buf(sock->my_port, sock->their_port, seq, sock->window.last_seq_received + 1, 
        DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, sock->window.my_window_to_advertise, 0, NULL, data, 1);
  sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
  // free msg
  free(msg);
  msg = NULL;
  return;
}

// send data until effective window size is zero
void send_full_real_window(cmu_socket_t *sock){
  char* msg;
  char *LBA = sock->window.last_byte_acked, *LBW = sock->window.last_byte_written;
  int sockfd, plen;
  size_t effective_window, data_len_to_send, conn_len = sizeof(sock->conn);
  uint32_t seq;

  sockfd = sock->socket;
  if((size_t)sock->window.advertised_window < (size_t)(sock->window.last_byte_sent - LBA))
  {
    effective_window = 0;
  }else
  {
    effective_window = sock->window.advertised_window - (size_t)(sock->window.last_byte_sent - LBA);// need to bring max_cwnd to the equation
  }
  data_len_to_send = my_min((size_t)(LBW - sock->window.last_byte_sent), effective_window);
  while( data_len_to_send != 0){
    size_t data_sent_this_turn = 0;
    seq = (sock->window.last_byte_sent - LBA) + sock->window.last_ack_received;

    if(data_len_to_send <= MAX_DLEN){
      plen = DEFAULT_HEADER_LEN + data_len_to_send;
      msg = create_packet_buf(sock->my_port, sock->their_port, seq, sock->window.last_seq_received + 1, 
        DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, sock->window.my_window_to_advertise, 0, NULL, sock->window.last_byte_sent, data_len_to_send);// need to calculate my_adv_window
      data_sent_this_turn = data_len_to_send;
    }
    else{
      plen = DEFAULT_HEADER_LEN + MAX_DLEN;
      msg = create_packet_buf(sock->my_port, sock->their_port, seq, sock->window.last_seq_received + 1, 
        DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, sock->window.my_window_to_advertise, 0, NULL, sock->window.last_byte_sent, MAX_DLEN);
      data_sent_this_turn = MAX_DLEN;
    }
    // update LBS in window_t, and remaining data to be sent for this func call
    sock->window.last_byte_sent += data_sent_this_turn;
    data_len_to_send -= data_sent_this_turn;
    
    sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
    // free msg
    free(msg);
    msg = NULL;
  }//end of while loop
  return;
}

//resend the first packet in window (data from LBA)
void resend_LBA_packet(cmu_socket_t * sock){
  char *LBA, *LBS, *msg;
  size_t len_to_send, conn_len = sizeof(sock->conn);
  uint32_t seq;
  int sockfd, plen;
  LBA = sock->window.last_byte_acked;
  LBS = sock->window.last_byte_sent;
  seq = sock->window.last_ack_received;
  len_to_send = my_min(MAX_DLEN, (size_t)(LBS-LBA));
  plen = DEFAULT_HEADER_LEN + len_to_send;
  msg = create_packet_buf(sock->my_port, sock->their_port, seq, sock->window.last_seq_received + 1, 
  DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, sock->window.my_window_to_advertise, 0, NULL, LBA, len_to_send);
  sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
  free(msg);
  msg = NULL;
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
      
    do{
      //prepare the sending buffer
      while(pthread_mutex_lock(&(dst->send_lock)) != 0);
      update_sending_buffer(&data, dst);
      pthread_mutex_unlock(&(dst->send_lock));
      //send till the effective window is zero
      my_send(dst, data);
      if(check_for_data(dst, TIMEOUT, current_timeout) == TIMEOUT){
        resend_LBA_packet(dst);
      }
    }while(dst->window.last_byte_acked != dst->window.last_byte_written);//this condition needs double check   
    //clean up after after sending
    free(data);
    data = NULL;

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
