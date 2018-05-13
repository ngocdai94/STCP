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

// IS THIS YOUR STUFF, IRWAN????

// Default values 
/* Default data offset when sending packet. A header not using the optional
   TCP field has a data offset of 5(20bytes), while a header using the max
  optional field has a data offset of 15(60 bytes).
 */
#define STCP_HEADER_LEN 5

// max packet size
#define STCP_MAX_PKT_SIZE sizeof(STCPHeader) + STCP_MSS; // MSS from .h

enum
{
  CSTATE_ESTABLISHED,
  SYN_SENT,
  SYN_RECV,
  SYNC_ACK_SENT,
  SYNC_ACK_RECV,
  FIN_SENT,
  CSTATE_CLOSED
}; /* you should have more states */

/* this structure is global to a mysocket descriptor */
typedef struct
{
  bool_t done; /* TRUE once connection is closed */

  int connection_state; /* state of the connection (established, etc.) */
  tcp_seq initial_sequence_num;
  unsigned int rec_seq_num; // next sequence number
  unsigned int rec_wind_size;

  /* any other connection-wide global variables go here */
} context_t;

static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);

//NEED TO DOCUMENT
STCPHeader *create_SYN_packet(unsigned int seq, unsigned int ack);
STCPHeader *create_SYN_ACK_packet(unsigned int seq, unsigned int ack);
STCPHeader *create_ACK_packet(unsigned int seq, unsigned int ack);

bool send_SYN(mysocket_t sd, context_t *ctx);
void wait_for_SYN_ACK(mysocket_t sd, context_t *ctx);
bool send_ACK(mysocket_t sd, context_t *ctx);

void wait4_SYN(mysocket_t sd, context_t* ctx);
bool send_SYNACK(mysocket_t sd, context_t* ctx);
void wait4_ACK(mysocket_t sd, context_t* ctx);

/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
  context_t *ctx;

  ctx = (context_t *)calloc(1, sizeof(context_t));
  assert(ctx);

  generate_initial_seq_num(ctx);

  /* XXX: you should send a SYN packet here if is_active, or wait for one
   * to arrive if !is_active.  after the handshake completes, unblock the
   * application with stcp_unblock_application(sd).  you may also use
   * this to communicate an error condition back to the application, e.g.
   * if connection fails; to do so, just set errno appropriately (e.g. to
   * ECONNREFUSED, etc.) before calling the function.
   */
  if (is_active)
  { // Client control path, initiate connection
    // Send SYN
    if (!send_SYN(sd, ctx))
      return;

    // Wait for SYN-ACK
    wait_for_SYN_ACK(sd, ctx);

    // Send ACK Packet
    if (!send_ACK(sd, ctx))
      return;
  }
  else
  {
    // Server control path, wait for connection
    // THIS IS IRWANN JOBBBB
    // PLEASE DO IT, AND UPDATE YOUR DOCUMENTATION

    //    wait_for_SYN(sd, ctx);
    //
    //    if (!send_SYN_ACK(sd, ctx)) return;
    //
    //    wait_for_ACK(sd, ctx);
    
    
    // IRWANNNNN
    
    // Network -> Transport -> Application
    
    // wait for a SYN packet to arrive... from the peer
    wait4_SYN(sd, ctx);
    
    
    // send SYN-ACK
    if(!send_SYNACK(sd, ctx))
      return;
    //print error?
    
    
    // wait for ACK
    wait4_ACK(sd, ctx);
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
} // irwan was here
//kkkkkkk

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
    // Olsen Ong
    // 올센 ooooo
    /* see stcp_api.h or stcp_api.c for details of this function */
    /* XXX: you will need to change some of these arguments! */
    event = stcp_wait_for_event(sd, ANY_EVENT, NULL);

    /* check whether it was the network, app, or a close request */
    if (event & APP_DATA)
    {
      /* the application has requested that data be sent */
      /* see stcp_app_recv() */
      size_t max_payload_length = (MSS < ctx->rec_wind_size ? MSS : ctx->rec_wind_size) - sizeof(STCPHeader);
      char payload[max_payload_length];
      ssize_t app_bytes = stcp_app_recv(sd, payload, max_payload_length);

      // printf("App Data Bytes: %d\n", app_bytes);
      // printf("App Data Payload: %s\n", payload);

      if (app_bytes == 0)
      {
        free(ctx);
        stcp_unblock_application(sd);
        //errno = ECONNREFUSED; // TODO

        return;
      }
      send_DATA_packet_network(sd, ctx, payload, app_bytes);
      wait_for_ACK(sd, ctx);
    }

    if (event & NETWORK_DATA)
    {
      bool isFIN = false;
      bool isDUP = false; // test if packet is a duplicate
      char payload[MSS];
      ssize_t network_bytes = stcp_network_recv(sd, payload, MSS);

      if (network_bytes < sizeof(STCPHeader))
      {
        free(ctx);
        stcp_unblock_application(sd);
        //errno = ECONNREFUSED; // TODO

        return;
      }

      printSTCPHeader((STCPHeader *)payload);
      // printf("Network Data Payload: %s\n", payload + sizeof(STCPHeader));
      // printf("Network Bytes: %d\n", network_bytes);
      parse_DATA_packet(ctx, payload, isFIN, isDUP);

      if (isDUP)
      {
        send_ACK(sd, ctx);

        return;
      }

      if (isFIN)
      {
        clock_gettime(CLOCK_REALTIME, &spec);
        // printf("%d isFIN\n", spec.tv_nsec);
        send_ACK(sd, ctx);
        stcp_fin_received(sd);
        ctx->connection_state = CSTATE_CLOSED;

        return;
      }

      if (network_bytes - sizeof(STCPHeader))
      {
        // printf("isDATA\n");
        send_DATA_packet_app(sd, ctx, payload, network_bytes);
        send_ACK(sd, ctx);
      }
    }

    if (event & APP_CLOSE_REQUESTED)
    {
      if (ctx->connection_state == CSTATE_ESTABLISHED)
      {
        send_FIN_packet(sd, ctx);
      }

      printf("connection_state: %d\n", ctx->connection_state);
    }

    if (event & ANY_EVENT)
    {
    }

    /* etc. */
  }
}

STCPHeader *create_SYN_packet(unsigned int seq, unsigned int ack)
{
  STCPHeader *SYN_packet = (STCPHeader *)malloc(sizeof(STCPHeader));
  SYN_packet->th_seq = htonl(seq);
  SYN_packet->th_ack = htonl(ack);
  SYN_packet->th_off = htons(5);           // header size offset for packed data
  SYN_packet->th_flags = TH_SYN;           // set packet type to SYN
  SYN_packet->th_win = htons(WINDOW_SIZE); // default value
  return SYN_packet;
}

STCPHeader *create_ACK_packet(unsigned int seq, unsigned int ack)
{
  STCPHeader *ACK_packet = (STCPHeader *)malloc(sizeof(STCPHeader));
  ACK_packet->th_seq = htonl(seq);
  ACK_packet->th_ack = htonl(ack);
  ACK_packet->th_off = htons(5);           // header size offset for packed data
  ACK_packet->th_flags = TH_ACK;           // set packet type to ACK
  ACK_packet->th_win = htons(WINDOW_SIZE); // default value
  return ACK_packet;
}

bool send_SYN(mysocket_t sd, context_t *ctx)
{
  bool status;

  // Create SYN Packet
  STCPHeader *SYN_packet = create_SYN_packet(ctx->initial_sequence_num, 0);
  ctx->initial_sequence_num++;

  // Send SYN packet
  ssize_t sentBytes =
      stcp_network_send(sd, SYN_packet, sizeof(STCPHeader), NULL);

  // Verify sending of SYN packet
  if (sentBytes > 0)
  { // If SYN packet suucessfully sent
    ctx->connection_state = SYN_SENT;
    free(SYN_packet);
    status = true;
  }
  else
  {
    free(SYN_packet);
    free(ctx);
    errno = ECONNREFUSED;
    status = false;
  }

  return status;
}

void wait_for_SYN_ACK(mysocket_t sd, context_t *ctx)
{
  char buffer[sizeof(STCPHeader)];

  unsigned int event = stcp_wait_for_event(sd, NETWORK_DATA, NULL);

  ssize_t receivedBytes = stcp_network_recv(sd, buffer, MSS);

  // Verify size of received packet
  if ((unsigned int)receivedBytes < sizeof(STCPHeader))
  {
    free(ctx);
    // stcp_unblock_application(sd);
    errno = ECONNREFUSED; // TODO
    return;
  }

  // Parse received data
  STCPHeader *receivedPacket = (STCPHeader *)buffer;

  // Check for appropriate flags and set connection state
  if (receivedPacket->th_flags == (TH_ACK | TH_SYN))
  {
    ctx->rec_seq_num = ntohl(receivedPacket->th_seq);
    ctx->rec_wind_size =
        ntohs(receivedPacket->th_win) > 0 ? ntohs(receivedPacket->th_win) : 1;
    ctx->connection_state = SYNC_ACK_RECV;
  }
}

bool send_ACK(mysocket_t sd, context_t *ctx)
{
  // printf("Sending ACK\n");
  bool status;

  // Create ACK Packet
  STCPHeader *ACK_packet =
      create_ACK_packet(ctx->initial_sequence_num, ctx->rec_seq_num + 1);

  // Send ACK packet
  ssize_t sentBytes =
      stcp_network_send(sd, ACK_packet, sizeof(STCPHeader), NULL);

  // Verify sending of ACK packet
  if (sentBytes > 0)
  { // If ACK packet suucessfully sent
    free(ACK_packet);
    status = true;
  }
  else
  {
    free(ACK_packet);
    free(ctx);
    // stcp_unblock_application(sd);
    errno = ECONNREFUSED; // TODO
    status = false;
  }

  return status;
}

void wait4_SYN(mysocket_t sd, context_t* ctx)
{
  // create buffer to hold header data
  char buffer[sizeof(STCPHeader)];
  
  // wait for NETWORK_DATA event
  stcp_wait_for_event(sd, NETWORK_DATA, NULL);
  
  ssize_t bytes_recv;
  if((bytes_recv = stcp_network_recv(sd, buffer, MSS) < sizeof(STCPHeader))
     {
       // data not complete. Drop it
       free(ctx);
       // error code?
       // errno = ECONNREFUSED;
       return;
     }
     
     // Inspect the header in the buffer.
     STCPHeader* pkt = (STCPHeader*)buffer;
     
     if(pkt->th_flags == TH_SYN)
     {
       ctx->rec_wind_size = ntohs(pkt->th_win);
       ctx->rec_seq_num = ntohl(pkt->th_seq);
       ctx->connection_state = SYN_RECVD;    // double check the enum name
     }
}
     
bool send_SYNACK(mysocket_t sd, context_t* ctx)
{
  bool status = false;
  
  // create pkt to send
  STCPHeader* SYNACK_pkt = (STCPHeader*) malloc(sizeof(STCPHeader));
  SYNACK_pkt->th_seq = htonl(seq);
  SYNACK_pkt->th_win = htons(WINDOW_SIZE);
  SYNACK_pkt->th_off = htons(STCP_HEADER_LEN);
  SYNACK_pkt->th_ack = htonl(rec_seq_num++);
  SYNACK_pkt->th_flags = (TH_SYN | TH_ACK);

  // after complete, update seq_num
  ctx->seq_num++;

  // send the SYN-ACK
  ssize_t bytes_sent;
  if((bytes_sent = stcp_network_send(sd, SYNACK_pkt,
                                    sizeof(STCPHeader), NULL)) <= 0)
  {
   perror("not sent properly");
   
   //refuse connection
   free(ctx);      // free incorrect memory
   free(SYNACK_pkt);  // --
   
   // unblock
   stcp_unblock_application(sd);
   errno = ECONNREFUSED;
   return false;
  }
  else // if sent properly
  {
   // update the connection state
   ctx->connection_state = SYN_ACK_SENT;
   
   // release no longer used data
   free(SYNACK_pkt);
   
   // DO WE NEED TO FREE(CTX)?
   // ARE WE STILL USING IT, THO??
   
   return true;
  }
}
     

void wait4_ACK(mysocket_t sd, context_t* ctx)
{
  char buffer[sizeof(STCPHeader)];

  // blocking event
  stcp_wait_for_event(sd, NETWORK_DATA, NULL);

  // recv the header
  ssize_t bytes_recv;
  if((bytes_recv = stcp_network_recv(sd, buffer, MSS)) < sizeof(STCPHeader)
    {
      // data recvd not complete. Drop it
      free(ctx);
      errno = ECONNREFUSED;
      return;
    }
    
    // Verify header
    STCPHeader* pkt = (STCPHeader*) buffer;
    
    if(pkt->th_flags == TH_ACK)
    {
      // update seq#, windowsize, & connection_state
      ctx->rec_seq_num = ntohl(pkt->th_seq);
      ctx->rec_wind_size = ntohs(pkt->th_win);
      
      // check if state = FIN
      if(ctx->connection_state == FIN_SENT)
        ctx->connection_state = CSTATE_CLOSED;
    }
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
void our_dprintf(const char *format, ...)
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
