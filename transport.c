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
  SYN_ACK_SENT,
  SYN_ACK_RECV,
  FIN_SENT,
  CSTATE_CLOSED,
  FIN_1,
  FIN_2,
  SEND_ACK_THEN_FIN,
  STATE_CLOSED,
  LAST_ACK
}; /* you should have more states */

/* this structure is global to a mysocket descriptor */
typedef struct
{
  bool_t done; /* TRUE once connection is closed */

  int connection_state; /* state of the connection (established, etc.) */
  tcp_seq initial_sequence_num;
  unsigned int snd_seq_num;

  unsigned int rec_seq_num; // next sequence number
  unsigned int rec_wind_size;

  /* any other connection-wide global variables go here */
} context_t;

static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);

// ------------------------------- Dai Part ------------------------------- //
// Create SYN Packet
// To create a syn packet, TCP header need need to be allocate and need
// infomation such as sequence number, acknowledgement number, data offset
// TH_SYN flag of TCP, and window size
//
// Pre: STCP Header is available
// Post: STCP Header return an allocated SYN packet
STCPHeader *create_SYN_Packet(unsigned int seq, unsigned int ack);

// Create SYN+ACK Packet
// Similar to create_SYN_packet, TCP header need need to be allocate and need
// infomation such as sequence number, acknowledgement number, data offset
// and window size. The difference now is TH_SYN & TH_ACK flag of TCP
//
// Pre: STCP Header is available
// Post: STCP Header return an allocated SYN_ACK packet
STCPHeader *create_SYN_ACK_Packet(unsigned int seq, unsigned int ack);

// Create ACK Packet
// Similar to create_SYN_packet, TCP header need need to be allocate and need
// infomation such as sequence number, acknowledgement number, data offset
// and window size. The difference now is TH_ACK flag of TCP
//
// Pre: STCP Header is available
// Post: STCP Header return an allocated ACK packet
STCPHeader *create_ACK_Packet(unsigned int seq, unsigned int ack);

// Send SYN packet to remote server
bool send_SYN(mysocket_t sd, context_t *ctx);

// Wait for SYN+ACK from the remote server
void wait_for_SYN_ACK(mysocket_t sd, context_t *ctx);

// Send ACK to remote server to complete TCP-HANDSHAKE
bool send_ACK(mysocket_t sd, context_t *ctx);
// ---------------------------------- End ---------------------------------- //

// ------------------------------- Irwan Part ------------------------------ //
void wait4_SYN(mysocket_t sd, context_t *ctx);
bool send_SYNACK(mysocket_t sd, context_t *ctx);
void wait4_ACK(mysocket_t sd, context_t *ctx);
// ---------------------------------- End ---------------------------------- //

// ------------------------------- Olsen Part ------------------------------ //
/* TODO
 Include your work here!!
 */
STCPHeader* create_DATA_packet(unsigned int seq, unsigned int ack, char* payload, size_t payload_length);
bool send_packet_network(mysocket_t sd, context_t* ctx, char* payload, size_t payload_length);
void send_packet_app(mysocket_t sd, context_t* ctx, char* payload, size_t length);
void parse_packet(context_t* ctx, char* payload, bool& bFin, bool& bDup);
STCPHeader* create_FIN_packet(unsigned int seq, unsigned int ack);
bool send_FIN_packet(mysocket_t sd, context_t* ctx);


// ---------------------------------- End ---------------------------------- //

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
    // Network -> Transport -> Application

    // wait for a SYN packet to arrive... from the peer
    wait4_SYN(sd, ctx);

    // send SYN-ACK
    if (!send_SYNACK(sd, ctx))
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
    // Olsen was here

    /* see stcp_api.h or stcp_api.c for details of this function */
    /* XXX: you will need to change some of these arguments! */
    event = stcp_wait_for_event(sd, ANY_EVENT, NULL);

    if (event & APP_DATA)
    {
      /* the application has requested that data be sent */
      /* see stcp_app_recv() */
      size_t max_payload_length = (STCP_MSS < ctx->rec_wind_size ? STCP_MSS : ctx->rec_wind_size) - sizeof(STCPHeader);
      char payload[max_payload_length];
      ssize_t app_bytes = stcp_app_recv(sd, payload, max_payload_length);

      if (app_bytes == 0)
      {
        free(ctx);
        stcp_unblock_application(sd);

        return;
      }
      send_packet_network(sd, ctx, payload, app_bytes);
      wait4_ACK(sd, ctx);
    }

    if (event & NETWORK_DATA)
    {
      bool bFin = false;
      bool bDup = false;
      char payload[MSS];
      ssize_t network_bytes = stcp_network_recv(sd, payload, STCP_MSS);

      
      // WANN edits..
      // for closing with 4-way handshake
      STCPHeader* header = (STCPHeader*) payload;
      if(header->th_flags & TH_ACK)
      {
        if((ctx->connection_state) == FIN_1)
        {
          ctx->connection_state == FIN_2;
          break;
        }
        else if((ctx->connection_state) == FIN_2)
        {
          ctx->connection_state == CSTATE_CLOSED;
          ctx->done = true;
          break;
        }
        goto end;
      }


      if (network_bytes < sizeof(STCPHeader))
      {
        free(ctx);
        stcp_unblock_application(sd);

        return;
      }


      parse_packet(ctx, payload, bFin, bDup);

      if (bDup)
      {
        send_ACK(sd, ctx);

        return;
      }

      if (bFin)
      {
        /*send_ACK(sd, ctx);
        stcp_fin_received(sd);
        ctx->connection_state = CSTATE_CLOSED;*/


        // Wann edits. For 4-way handshake-------------
        // need to send ACK if FIN is received and notify app. We just need to send FIN flag.
        //ctx->snd_nak++;
        ctx->rec_seq_num++;

        // tell the app
        stcp_fin_received(sd);

        // change states
        if(ctx->connection_state == CSTATE_ESTABLISHED)
        {
          ctx->connection_state = SEND_ACK_THEN_FIN;
          break;
        }
        /*else if(ctx->connection_state == FIN_1)
        {
          ctx->connection_state = STATE_CLOSING;
          break;
        }*/
        else if(ctx->connection_state == FIN_2)
        {
          dprintf("Connection closed.\n");
          ctx->connection_state = STATE_CLOSED;
          ctx->done = true;
          break;
        }
        else
        {
          perror("Error. Invalid state with FIN.\n");
        }

        return;
      }

      if (network_bytes - sizeof(STCPHeader))
      {
        send_packet_app(sd, ctx, payload, network_bytes);
        send_ACK(sd, ctx);

        // last_FIN state
        if(ctx->connection_state == SEND_ACK_THEN_FIN)
        {
          goto last_FIN;
        }
      }
    }

    // Wann edit.. Changed app closing to use 4-way handshake.
    if (event & APP_CLOSE_REQUESTED)
    {
      /*if (ctx->connection_state == CSTATE_ESTABLISHED)
      {
        send_FIN_packet(sd, ctx);
      }*/


      last_FIN:
      // check if there is a connection. If yes, break it...
      if (ctx->connection_state == CSTATE_ESTABLISHED)
      {
        // change to FIN_1
        ctx->connection_state = FIN_1;
        break;
      }
      else if (ctx->connection_state == SEND_ACK_THEN_FIN)
      {
        ctx->connection_state = LAST_ACK;
        break;
      }
      else
      {
        perror("Error. Invalid state with CLOSE request.\n");
      }

      // send FIN PACKET--------
      if(!send_FIN_packet(sd, ctx))
      {
        perror("FIN unsuccessfully sent.\n");
      }

      printf("connection_state: %d\n", ctx->connection_state);
    }

    end:
    if (event & ANY_EVENT)
    {
    }

    /* etc. */
  }
}

// Create SYN Packet
// To create a syn packet, TCP header need need to be allocate and need
// infomation such as sequence number, acknowledgement number, data offset
// TH_SYN flag of TCP, and window size
//
// Pre: STCP Header is available
// Post: STCP Header return an allocated SYN packet
STCPHeader *create_SYN_Packet(unsigned int seq, unsigned int ack)
{
  STCPHeader *SYN_packet = (STCPHeader *)malloc(sizeof(STCPHeader));
  SYN_packet->th_seq = htonl(seq);
  SYN_packet->th_ack = htonl(ack);
  SYN_packet->th_off = htons(5);           // header size offset for packed data
  SYN_packet->th_flags = TH_SYN;           // set packet type to SYN
  SYN_packet->th_win = htons(WINDOW_SIZE); // default value
  return SYN_packet;
}

// Create SYN+ACK Packet
// Similar to create_SYN_packet, TCP header need need to be allocate and need
// infomation such as sequence number, acknowledgement number, data offset
// and window size. The difference now is TH_SYN & TH_ACK flag of TCP
//
// Pre: STCP Header is available
// Post: STCP Header return an allocated SYN_ACK packet
STCPHeader *create_SYN_ACK_Packet(unsigned int seq, unsigned int ack)
{
  STCPHeader *SYN_ACK_packet = (STCPHeader *)malloc(sizeof(STCPHeader));
  SYN_ACK_packet->th_seq = htonl(seq);
  SYN_ACK_packet->th_ack = htonl(ack);
  SYN_ACK_packet->th_off = htons(5);            // header size offset for packed data
  SYN_ACK_packet->th_flags = (TH_SYN | TH_ACK); // set packet type to SYN_ACK
  SYN_ACK_packet->th_win = htons(WINDOW_SIZE);  // default value
  return SYN_ACK_packet;
}

// Create ACK Packet
// Similar to create_SYN_packet, TCP header need need to be allocate and need
// infomation such as sequence number, acknowledgement number, data offset
// and window size. The difference now is TH_ACK flag of TCP
//
// Pre: STCP Header is available
// Post: STCP Header return an allocated ACK packet
STCPHeader *create_ACK_Packet(unsigned int seq, unsigned int ack)
{
  STCPHeader *ACK_packet = (STCPHeader *)malloc(sizeof(STCPHeader));
  ACK_packet->th_seq = htonl(seq);
  ACK_packet->th_ack = htonl(ack);
  ACK_packet->th_off = htons(5);           // header size offset for packed data
  ACK_packet->th_flags = TH_ACK;           // set packet type to ACK
  ACK_packet->th_win = htons(WINDOW_SIZE); // default value
  return ACK_packet;
}

// Send SYN packet to remote server
bool send_SYN(mysocket_t sd, context_t *ctx)
{
  bool status;

  // Create SYN Packet
  STCPHeader *SYN_packet = create_SYN_Packet(ctx->initial_sequence_num, 0);
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

// Wait for SYN+ACK from the remote server
void wait_for_SYN_ACK(mysocket_t sd, context_t *ctx)
{
  char buffer[sizeof(STCPHeader)];

  unsigned int event = stcp_wait_for_event(sd, NETWORK_DATA, NULL);

  ssize_t receivedBytes = stcp_network_recv(sd, buffer, STCP_MSS);

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
    ctx->connection_state = SYN_ACK_RECV;
  }
}

// Send ACK to remote server to complete TCP-HANDSHAKE
bool send_ACK(mysocket_t sd, context_t *ctx)
{
  bool status;

  // Create ACK Packet
  STCPHeader *ACK_packet =
      create_ACK_Packet(ctx->initial_sequence_num, ctx->rec_seq_num + 1);

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

void wait4_SYN(mysocket_t sd, context_t *ctx)
{
  // create buffer to hold header data
  char buffer[sizeof(STCPHeader)];

  // wait for NETWORK_DATA event
  stcp_wait_for_event(sd, NETWORK_DATA, NULL);

  ssize_t bytes_recv;
  if(bytes_recv = stcp_network_recv(sd, buffer, STCP_MSS) < sizeof(STCPHeader))
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
    ctx->connection_state = SYN_RECV; // double check the enum name
     }
}

bool send_SYNACK(mysocket_t sd, context_t *ctx)
{
  bool status = false;

  // create pkt to send
  STCPHeader *SYNACK_pkt = (STCPHeader *)malloc(sizeof(STCPHeader));
  SYNACK_pkt->th_seq = htonl(ctx->snd_seq_num);
  SYNACK_pkt->th_win = htons(WINDOW_SIZE);
  SYNACK_pkt->th_off = htons(STCP_HEADER_LEN);
  SYNACK_pkt->th_ack = htonl(ctx->rec_seq_num++);
  SYNACK_pkt->th_flags = (TH_SYN | TH_ACK);

  // after complete, update seq_num
  ctx->snd_seq_num++;

  // send the SYN-ACK
  ssize_t bytes_sent;
  if ((bytes_sent = stcp_network_send(sd, SYNACK_pkt,
                                      sizeof(STCPHeader), NULL)) <= 0)
  {
    perror("not sent properly");

    //refuse connection
    free(ctx);        // free incorrect memory
    free(SYNACK_pkt); // --

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

void wait4_ACK(mysocket_t sd, context_t *ctx)
{
  char buffer[sizeof(STCPHeader)];

  // blocking event
  stcp_wait_for_event(sd, NETWORK_DATA, NULL);

  // recv the header
  ssize_t bytes_recv;
  if((bytes_recv = stcp_network_recv(sd, buffer, STCP_MSS)) < sizeof(STCPHeader))
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
    if (ctx->connection_state == FIN_SENT)
      ctx->connection_state = CSTATE_CLOSED;
    }
}

void send_packet_app(mysocket_t sd, context_t *ctx, char *payload, size_t length)
{

  // Send DATA packet

  stcp_app_send(sd, payload + sizeof(STCPHeader), length - sizeof(STCPHeader));
}

void parse_packet(context_t *ctx, char *payload, bool &bFin, bool &bDup)
{

  STCPHeader *payloadHeader = (STCPHeader *)payload;

  ctx->rec_seq_num = ntohl(payloadHeader->th_seq);

  ctx->rec_wind_size = ntohs(payloadHeader->th_win);

  bFin = payloadHeader->th_flags == TH_FIN;
}

STCPHeader *create_DATA_packet(unsigned int seq, unsigned int ack, char *payload, size_t payload_length)
{
  unsigned int DATA_packet_size = sizeof(STCPHeader) + payload_length;

  STCPHeader *DATA_packet = (STCPHeader *)malloc(DATA_packet_size);

  DATA_packet->th_seq = htonl(seq);
  DATA_packet->th_ack = htonl(ack);
  DATA_packet->th_flags = NETWORK_DATA;
  DATA_packet->th_win = htons(WINDOW_SIZE);
  DATA_packet->th_off = htons(5);

  memcpy((char *)DATA_packet + sizeof(STCPHeader), payload, payload_length);

  return DATA_packet;
}

bool send_packet_network(mysocket_t sd, context_t *ctx, char *payload, size_t payload_length)
{

  STCPHeader *DATA_packet = create_DATA_packet(ctx->snd_seq_num, ctx->rec_seq_num + 1, payload, payload_length);

  ctx->snd_seq_num += payload_length;

  ssize_t sentBytes = stcp_network_send(sd, DATA_packet, sizeof(STCPHeader) + payload_length, NULL);

  if (sentBytes > 0)
  { 
    free(DATA_packet);
    return true;
  }
  else
  {

    free(DATA_packet);
    free(ctx);
    stcp_unblock_application(sd);
    errno = ECONNREFUSED;

    return false;
  }
}

bool send_FIN_packet(mysocket_t sd, context_t* ctx) {

  STCPHeader* FIN_packet = create_FIN_packet(ctx->snd_seq_num, ctx->rec_seq_num + 1);

  ctx->snd_seq_num++;

  ssize_t sentBytes = stcp_network_send(sd, FIN_packet, sizeof(STCPHeader), NULL);


  if (sentBytes > 0) {  
    ctx->connection_state = FIN_SENT;
    wait4_ACK(sd, ctx);

    free(FIN_packet);

    return true;

  } else {
    free(FIN_packet);

    free(ctx);

    stcp_unblock_application(sd);

    errno = ECONNREFUSED;

    return false;

  }

}

STCPHeader* create_FIN_packet(unsigned int seq, unsigned int ack) {

  STCPHeader* FIN_packet = (STCPHeader*)malloc(sizeof(STCPHeader));

  FIN_packet->th_seq = htonl(seq);
  FIN_packet->th_ack = htonl(ack);
  FIN_packet->th_flags = TH_FIN;
  FIN_packet->th_win = htons(WINDOW_SIZE);
  FIN_packet->th_off = htons(5);

  return FIN_packet;

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
