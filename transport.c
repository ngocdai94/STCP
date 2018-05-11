/* Team: Dai Nguyen, Olsen Ong, Irwan
 * transport.c
 *
 * CPSC4510: Project 3 (STCP)
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers. You are required to fill in the STCP
 * functionality in this file.
 *
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"

// NEED TO DOCUMENT
const unsigned int MAX_SEQUENCE_NUM = 255;
const unsigned int WINDOW_SIZE = 3072;
const unsigned int MSS = 536;

enum {
  CSTATE_ESTABLISHED,
  SYN_SENT,
  SYNC_ACK_RECV
};    /* you should have more states */


/* this structure is global to a mysocket descriptor */
typedef struct
{
  bool_t done;    /* TRUE once connection is closed */
  
  int connection_state;   /* state of the connection (established, etc.) */
  tcp_seq initial_sequence_num;
  unsigned int rec_seq_num;  // next sequence number
  unsigned int rec_wind_size;

  /* any other connection-wide global variables go here */
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);

//NEED TO DOCUMENT
STCPHeader* create_SYN_packet(unsigned int seq, unsigned int ack);
STCPHeader* create_SYN_ACK_packet(unsigned int seq, unsigned int ack);
STCPHeader* create_ACK_packet(unsigned int seq, unsigned int ack);

bool send_SYN(mysocket_t sd, context_t* ctx);
void wait_for_SYN_ACK(mysocket_t sd, context_t* ctx);
bool send_ACK(mysocket_t sd, context_t* ctx);

/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
  context_t *ctx;
  
  ctx = (context_t *) calloc(1, sizeof(context_t));
  assert(ctx);
  
  generate_initial_seq_num(ctx);
  
  /* XXX: you should send a SYN packet here if is_active, or wait for one
   * to arrive if !is_active.  after the handshake completes, unblock the
   * application with stcp_unblock_application(sd).  you may also use
   * this to communicate an error condition back to the application, e.g.
   * if connection fails; to do so, just set errno appropriately (e.g. to
   * ECONNREFUSED, etc.) before calling the function.
   */
  if (is_active) {  // Client control path, initiate connection
    // Send SYN
    if (!send_SYN(sd, ctx))
      return;
    
    // Wait for SYN-ACK
    wait_for_SYN_ACK(sd, ctx);

    // Send ACK Packet
    if (!send_ACK(sd, ctx))
      return;
    
  } else {  // Server control path, wait for connection
//    wait_for_SYN(sd, ctx);
//
//    if (!send_SYN_ACK(sd, ctx)) return;
//
//    wait_for_ACK(sd, ctx);
  }
  ctx->connection_state = CSTATE_ESTABLISHED;
  stcp_unblock_application(sd);
  
  control_loop(sd, ctx);
  
  /* do any cleanup here */
  free(ctx);
}

// NEED TO DOCUMENT THIS METHOD
/* generate random initial sequence number for an STCP connection */
static void generate_initial_seq_num(context_t *ctx)
{
  assert(ctx);
  
#ifdef FIXED_INITNUM
  /* please don't change this! */
  ctx->initial_sequence_num = 1;
#else
  /* you have to fill this up */
  /*ctx->initial_sequence_num =;*/
  ctx->initial_sequence_num = rand() % MAX_SEQUENCE_NUM + 1;
#endif
}


/* control_loop() is the main STCP loop; it repeatedly waits for one of the
 * following to happen:
 *   - incoming data from the peer
 *   - new data from the application (via mywrite())
 *   - the socket to be closed (via myclose())
 *   - a timeout
 */
static void control_loop(mysocket_t sd, context_t *ctx)
{
  assert(ctx);
  assert(!ctx->done);
  
  while (!ctx->done)
  {
    unsigned int event;
    
    /* see stcp_api.h or stcp_api.c for details of this function */
    /* XXX: you will need to change some of these arguments! */
    event = stcp_wait_for_event(sd, 0, NULL);
    
    /* check whether it was the network, app, or a close request */
    if (event & APP_DATA)
    {
      /* the application has requested that data be sent */
      /* see stcp_app_recv() */
    }
    
    /* etc. */
  }
}

STCPHeader* create_SYN_packet(unsigned int seq, unsigned int ack)
{
  STCPHeader* SYN_packet = (STCPHeader*)malloc(sizeof(STCPHeader));
  SYN_packet->th_seq = htonl(seq);
  SYN_packet->th_ack = htonl(ack);
  SYN_packet->th_off = htons(5);  // header size offset for packed data
  SYN_packet->th_flags = TH_SYN;  // set packet type to SYN
  SYN_packet->th_win = htons(WINDOW_SIZE);  // default value
  return SYN_packet;
}

STCPHeader* create_ACK_packet(unsigned int seq, unsigned int ack) {
  STCPHeader* ACK_packet = (STCPHeader*)malloc(sizeof(STCPHeader));
  ACK_packet->th_seq = htonl(seq);
  ACK_packet->th_ack = htonl(ack);
  ACK_packet->th_off = htons(5);  // header size offset for packed data
  ACK_packet->th_flags = TH_ACK;  // set packet type to ACK
  ACK_packet->th_win = htons(WINDOW_SIZE);  // default value
  return ACK_packet;
}

bool send_SYN(mysocket_t sd, context_t* ctx)
{
  bool status;
  
  // Create SYN Packet
  STCPHeader* SYN_packet = create_SYN_packet(ctx->initial_sequence_num, 0);
  ctx->initial_sequence_num++;
  
  // Send SYN packet
  ssize_t sentBytes =
    stcp_network_send(sd, SYN_packet, sizeof(STCPHeader), NULL);
  
  // Verify sending of SYN packet
  if (sentBytes > 0) {  // If SYN packet suucessfully sent
    ctx->connection_state = SYN_SENT;
    free(SYN_packet);
    status = true;
  } else {
    free(SYN_packet);
    free(ctx);
    errno = ECONNREFUSED;
    status = false;
  }
  
  return status;
}

void wait_for_SYN_ACK(mysocket_t sd, context_t* ctx) {
  char buffer[sizeof(STCPHeader)];
  
  unsigned int event = stcp_wait_for_event(sd, NETWORK_DATA, NULL);
  
  ssize_t receivedBytes = stcp_network_recv(sd, buffer, MSS);
  
  // Verify size of received packet
  if ((unsigned int)receivedBytes < sizeof(STCPHeader)) {
    free(ctx);
    // stcp_unblock_application(sd);
    errno = ECONNREFUSED;  // TODO
    return;
  }
  
  // Parse received data
  STCPHeader* receivedPacket = (STCPHeader*)buffer;
  
  // Check for appropriate flags and set connection state
  if (receivedPacket->th_flags == (TH_ACK | TH_SYN)) {
    ctx->rec_seq_num = ntohl(receivedPacket->th_seq);
    ctx->rec_wind_size =
      ntohs(receivedPacket->th_win) > 0 ? ntohs(receivedPacket->th_win) : 1;
    ctx->connection_state = SYNC_ACK_RECV;
  }
}

bool send_ACK(mysocket_t sd, context_t* ctx)
{
  // printf("Sending ACK\n");
  bool status;
  
  // Create ACK Packet
  STCPHeader* ACK_packet =
    create_ACK_packet(ctx->initial_sequence_num, ctx->rec_seq_num + 1);
  
  // Send ACK packet
  ssize_t sentBytes =
    stcp_network_send(sd, ACK_packet, sizeof(STCPHeader), NULL);
  
  // Verify sending of ACK packet
  if (sentBytes > 0) {  // If ACK packet suucessfully sent
    free(ACK_packet);
    status = true;
  } else {
    free(ACK_packet);
    free(ctx);
    // stcp_unblock_application(sd);
    errno = ECONNREFUSED;  // TODO
    status = false;
  }
  
  return status;
}

/**********************************************************************/
/* our_dprintf
 *
 * Send a formatted message to stdout.
 *
 * format               A printf-style format string.
 *
 * This function is equivalent to a printf, but may be
 * changed to log errors to a file if desired.
 *
 * Calls to this function are generated by the dprintf amd
 * dperror macros in transport.h
 */
void our_dprintf(const char *format,...)
{
  va_list argptr;
  char buffer[1024];
  
  assert(format);
  va_start(argptr, format);
  vsnprintf(buffer, sizeof(buffer), format, argptr);
  va_end(argptr);
  fputs(buffer, stdout);
  fflush(stdout);
}



