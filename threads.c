/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file 		threads.c
 *
 * @brief 		Code file for Thread Functions of MCTP transport library
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jan 2024
 * @author 		Barrett Edwards <code@jrlabs.io>
 * 
 */

/* INCLUDES ==================================================================*/

#define _GNU_SOURCE

/* ISO C11 Standard: 7.26 - Thread support library
 * thrd_t
 * thrd_create()
 * thrd_current()
 */
//#include <threads.h>

/* pid_t 
 */
#include <sys/types.h>

/* pthread_t 
 * pthread_create()
 * pthread_join()
 * pthread_getthreadid_np()
 */
#include <pthread.h>

/* useconds_t
 * usleep()
 * close()
 * gettid()
 */
#include <unistd.h>

/* exit()
 */
#include <stdlib.h>

/* printf()
 */
#include <stdio.h>

/* memset()
 * memcpy()
 */
#include <string.h>

/* errno
 */
#include <errno.h>

/* AF_INET
 * SOCK_STREAM
 * socklen_t 
 * struct sockaddr_in 
 * socket()
 * bind()
 * listen()
 * accept()
 * recv()
 * send()
 */
#include <sys/socket.h>

/* INADDR_ANY
 * ntohl()
 * htonl()
 */
#include <netinet/in.h>

/* __u8
 * __u32
 * __u64
 */
#include <linux/types.h>

/* be32toh()
 */
#include <endian.h>

/* uuid_t
 * uuid_generate();
 */
#include <uuid/uuid.h>

/* autl_prnt_buf()
 */
#include <arrayutils.h>
#include <timeutils.h>
#include <ptrqueue.h>

#include "mctp.h"

/* MACROS ====================================================================*/

#define MCTP_THREAD_ERROR_USLEEP 1000

//#define MCTP_VERBOSE
#ifdef MCTP_VERBOSE
 #define INIT 			unsigned step = 0;
 #define ENTER 					if (m->verbose & MCTP_VERBOSE_THREADS) 	printf("%d:%s Enter\n", 			gettid(), __FUNCTION__);
 #define STEP 			step++; if (m->verbose & MCTP_VERBOSE_STEPS) 	printf("%d:%s STEP: %u\n", 			gettid(), __FUNCTION__, step);
 #define HEX32(m, i)			if (m->verbose & MCTP_VERBOSE_STEPS) 	printf("%d:%s STEP: %u %s: 0x%x\n",	gettid(), __FUNCTION__, step, m, i);
 #define INT32(m, i)			if (m->verbose & MCTP_VERBOSE_STEPS) 	printf("%d:%s STEP: %u %s: %d\n",	gettid(), __FUNCTION__, step, m, i);
 #define EXIT(rc) 				if (m->verbose & MCTP_VERBOSE_THREADS)	printf("%d:%s Exit: %d\n", 			gettid(), __FUNCTION__,rc);

 #define TINIT 			 self->loop=0; self->threadid = gettid(); 
 #define TENTER 		              if (self->m->verbose & MCTP_VERBOSE_THREADS) 	printf("%d:%s Enter\n", 				self->threadid, __FUNCTION__);
 #define TLOOP(i) 		self->loop=i; if (self->m->verbose & MCTP_VERBOSE_STEPS)    printf("%d:%s LOOP: %u\n", 				self->threadid, __FUNCTION__, self->loop);
 #define TINT32(k, i)                 if (self->m->verbose & MCTP_VERBOSE_STEPS)    printf("%d:%s LOOP: %u %s: %d\n",		self->threadid, __FUNCTION__, self->loop, k, i);
 #define TEXIT(rc) 			  		  if (self->m->verbose & MCTP_VERBOSE_THREADS) 	printf("%d:%s Exit: %d\n", 				self->threadid, __FUNCTION__,rc);
 #define TERR(k, i)                   if (self->m->verbose & MCTP_VERBOSE_ERROR)    printf("%d:%s LOOP: %u ERR: %s: %d\n",	self->threadid, __FUNCTION__, self->loop, k, i);
 #define TMSG(k)                      if (self->m->verbose & MCTP_VERBOSE_THREADS)  printf("%d:%s LOOP: %u MSG: %s\n",		self->threadid, __FUNCTION__, self->loop, k);

#else
 #define INIT 
 #define ENTER
 #define EXIT(rc)
 #define STEP
 #define HEX32(m, i)
 #define INT32(m, i)

 #define TINIT 			self->loop = 0;	self->threadid = gettid(); 
 #define TENTER
 #define TLOOP(i) 		 self->loop=i;
 #define TINT32(m,i)
 #define TERR(k, i)
 #define TMSG(k)
 #define TEXIT(rc)
#endif // MCTP_VERBOSE

//#define IFV(u) 							if (opts[CLOP_VERBOSITY].u64 & u) 

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/* GLOBAL VARIABLES ==========================================================*/

/* PROTOTYPES ================================================================*/

static int mctp_configure(struct mctp *m);

/* FUNCTIONS =================================================================*/

/**
 * Configure an mctp object prior to calling run()
 *
 * STEPS
 * 1: Reset mctp state 
 * 2: Zero out variables	
 * 3: Clear existing queues
 * 4: Create queues
 * 5: Prepare data structures for threads
 */
static int mctp_configure(struct mctp *m)
{
	INIT

	ENTER 

	STEP // 1: Reset mctp state 
	m->all_threads_started = 0;
	m->stop_threads = 0;
	memset( &m->sa_client, 0, sizeof(struct sockaddr_in));
	m->client_len = sizeof(struct sockaddr_in);
	m->state.bus_owner_eid = 0;

	STEP // 2: Zero out variables	
	memset(&m->sr, 0, sizeof(struct socket_reader));
	memset(&m->pr, 0, sizeof(struct packet_reader));
	memset(&m->mh, 0, sizeof(struct message_handler));
	memset(&m->pw, 0, sizeof(struct packet_writer));
	memset(&m->sw, 0, sizeof(struct socket_writer));
	memset(&m->st, 0, sizeof(struct submission_thread));
	memset(&m->ct, 0, sizeof(struct completion_thread));

	STEP // 3: Clear existing queues
	pq_free(m->rpq);
	pq_free(m->rmq);
	pq_free(m->tpq);
	pq_free(m->tmq);
	pq_free(m->taq);
	pq_free(m->acq);
	pq_free(m->pkts);
	pq_free(m->msgs);
	pq_free(m->actions);
	m->rpq = NULL;
	m->rmq = NULL;
	m->tmq = NULL;
	m->tpq = NULL;
	m->taq = NULL;
	m->acq = NULL;
	m->pkts = NULL;
	m->msgs = NULL;
	m->actions = NULL;

	STEP // 4: Create queues 
	m->rpq = pq_init(MCTP_RPQ_SIZE, 0); 
	m->tpq = pq_init(MCTP_TPQ_SIZE, 0);
	m->rmq = pq_init(MCTP_RMQ_SIZE, 0);
	m->tmq = pq_init(MCTP_TMQ_SIZE, 0);
	m->taq = pq_init(MCTP_TAQ_SIZE, 0);
	m->acq = pq_init(MCTP_ACQ_SIZE, 0);

	// Create Central Object Pools 
	m->pkts    = pq_init(MCTP_PKT_POOL_SIZE,    sizeof(struct mctp_pkt_wrapper)); 
	m->msgs    = pq_init(MCTP_MSG_POOL_SIZE,    sizeof(struct mctp_msg)); 
	m->actions = pq_init(MCTP_ACTION_POOL_SIZE, sizeof(struct mctp_action)); 

	// Fail if any of the queues / pools failed to be created 
	if ( !m->rpq || !m->tpq || !m->rmq || !m->tmq || !m->taq || !m->pkts || !m->msgs || !m->actions ) 
	{
		errno = EFAULT;
		goto end_queue;
	}

	STEP // 5: Prepare data structures for threads
	// Set values for socket reader
	m->sr.m = m;

	// Set values for packet reader 
	m->pr.m = m;

	// Set values for message handler
	m->mh.m = m;

	// Set values for packet writer
	m->pw.m = m;

	// Set values for socket writer
	m->sw.m = m;

	// Set values for submission thread 
	m->st.m = m;
	m->st.thread_delta.tv_sec = 0;
	m->st.thread_delta.tv_nsec = MCTP_THREAD_SUBMIT_NSLEEP;
	m->st.action_delta.tv_sec = MCTP_ACTION_DELTA_SEC; 
	m->st.action_delta.tv_nsec = MCTP_ACTION_DELTA_NSEC;

	// Set values for completion thread
	m->ct.m = m;

	// Initialize per thread mutexes
	pthread_mutex_init(&m->st.mtx, NULL);
	pthread_cond_init(&m->st.cond, NULL);

	EXIT(0)

	return 0;

end_queue:

	pq_free(m->rpq); 
	pq_free(m->rmq);
	pq_free(m->tpq);
	pq_free(m->tmq);
	pq_free(m->taq);
	pq_free(m->acq);
	pq_free(m->pkts);
	pq_free(m->msgs);
	pq_free(m->actions);

	EXIT(1)

	return 1;
}

/**
 * Connection Handler Loop that listens for a TCP connection to be established
 *
 * This will continue to loop when each connection is dropped
 * STEPS 
 * 1: Configure threads for the new connection 
 * 2: Accept a connection
 * 3: Start threads 
 * 4: Pend until signaled to stop the threads
 * 5: Close connection if still connected
 * 6: Stop threads
 * 7: Unlock the mutex now that the threads have been stopped
 */
void *mctp_connection_handler(void *arg)
{
	struct connection_handler *self;
	int rv;

	// Initialize variables 
	self = (struct connection_handler *) arg;	
	TINIT

	TENTER

	// Thread Loop
	do 	
	{
		TLOOP(1) // LOOP 1: Configure threads for the new connection 
		mctp_configure(self->m);

		// Send signal to caller that queues & threads are ready 
		if (self->sem != NULL)
		{
			sem_post(self->sem);
			self->sem = NULL;
		}

		TLOOP(2) // LOOP 2: Accept a connection
		if (self->m->mode == MCRM_SERVER) 
		{
			self->m->conn = accept(self->m->sock, (struct sockaddr *) &self->m->sa_client, &self->m->client_len);
			if (self->m->conn < 0) 
			{
				TERR("accept() returned with error rv:",  self->m->conn);
				goto end_sock;
			}
		}

		TLOOP(3) // STEP 3: Start threads 
		if (self->m->use_threads) 
		{
			// Lock mutex before starting any threads 
			pthread_mutex_lock(&self->m->mtx);

			rv = pthread_create( &self->m->pt_sw, NULL, self->m->fn_sw, (void*) &self->m->sw);
			if ( rv != 0 ) 
			{
				TERR("Could not create socket writer thread", rv);
				goto end_thread;
			}

			rv = pthread_create( &self->m->pt_pw, NULL, self->m->fn_pw, (void*) &self->m->pw);
			if ( rv != 0 ) 
			{
				TERR("Could not create packet writer thread", rv);
				goto end_thread;
			}

			rv = pthread_create( &self->m->pt_mh, NULL, self->m->fn_mh, (void*) &self->m->mh);
			if ( rv != 0 ) 
			{
				TERR("Could not create message handler thread", rv);
				goto end_thread;
			}

			rv = pthread_create( &self->m->pt_pr, NULL, self->m->fn_pr, (void*) &self->m->pr);
			if ( rv != 0 ) {
				TERR("Could not create packet reader thread", rv);
				goto end_thread;
			}

			rv = pthread_create( &self->m->pt_sr, NULL, self->m->fn_sr, (void*) &self->m->sr);
			if ( rv != 0 ) 
			{
				TERR("Could not create socket reader thread", rv);
				goto end_thread;
			}

			rv = pthread_create( &self->m->pt_st, NULL, self->m->fn_st, (void*) &self->m->st);
			if ( rv != 0 ) 
			{
				TERR("Could not create submission thread", rv);
				goto end_thread;
			}

			rv = pthread_create( &self->m->pt_ct, NULL, self->m->fn_ct, (void*) &self->m->ct);
			if ( rv != 0 ) 
			{
				TERR("Could not create completion thread", rv);
				goto end_thread;
			}

			// Set bit indicating main thread has completed the starting of threads
			self->m->all_threads_started = 1;

			TLOOP(4) // LOOP 4: Pend until signaled to stop the threads
			while ( self->m->stop_threads == 0 ) 
				pthread_cond_wait(&self->m->cond, &self->m->mtx);

			TLOOP(5) // LOOP 5: Close connection if still connected
			close(self->m->conn);	

			TLOOP(6) // LOOP 6: Stop threads
			pthread_cancel(self->m->pt_sr);
			pthread_cancel(self->m->pt_pr);
			pthread_cancel(self->m->pt_mh);
			pthread_cancel(self->m->pt_pw);
			pthread_cancel(self->m->pt_sw);
			pthread_cancel(self->m->pt_st);
			pthread_cancel(self->m->pt_ct);
			pthread_join(self->m->pt_sr, NULL);
			pthread_join(self->m->pt_pr, NULL);
			pthread_join(self->m->pt_mh, NULL);
			pthread_join(self->m->pt_pw, NULL);
			pthread_join(self->m->pt_sw, NULL);
			pthread_join(self->m->pt_st, NULL);
			pthread_join(self->m->pt_ct, NULL);
			self->m->pt_sr = 0;
			self->m->pt_pr = 0;
			self->m->pt_mh = 0;
			self->m->pt_pw = 0;
			self->m->pt_sw = 0;
			self->m->pt_st = 0;
			self->m->pt_ct = 0;

			TLOOP(7) // LOOP 7: Unlock the mutex now that the threads have been stopped
			pthread_mutex_unlock(&self->m->mtx);
		}
		else {
			// If we are not using threads, loop through and call each thread function
			// TODO 
		}

	} while (self->m->stop_threads != 1 && self->m->mode == MCRM_SERVER);

end_thread:

	if (self->m->pt_sr != 0)
		pthread_cancel(self->m->pt_sr);
	if (self->m->pt_pr != 0)
		pthread_cancel(self->m->pt_pr);
	if (self->m->pt_mh != 0)
		pthread_cancel(self->m->pt_mh);
	if (self->m->pt_pw != 0)
		pthread_cancel(self->m->pt_pw);
	if (self->m->pt_sw != 0)
		pthread_cancel(self->m->pt_sw);
	if (self->m->pt_st != 0)
		pthread_cancel(self->m->pt_st);
	if (self->m->pt_ct != 0)
		pthread_cancel(self->m->pt_ct);

end_sock:

	// Destroy per thread mutexes
	pthread_mutex_destroy(&self->m->st.mtx);
	pthread_cond_destroy(&self->m->st.cond);

	close(self->m->sock);

	TEXIT(self->m->stop_threads == 0 && self->m->mode == MCRM_SERVER);

	return NULL;
}

/**
 * Socket Reader Thread
 *
 * @param arg This is a void * but will only ever be a struct socket_reader*
 *
 * STEPS
 * 1: Get pkt free pool 
 * 2: Read MCTP packet from socket connection
 * 3: Post received packet to Receive Packet Queue (RPQ)
 */
void *mctp_socket_reader(void *arg)
{
	struct socket_reader *self;
	struct mctp_pkt_wrapper *pw;
	int rv;

	// Initialize variables
	self = (struct socket_reader*) arg;
	TINIT

	TENTER

	// Thread Loop
	do
	{
	 	TLOOP(1) // STEP 1: Get pkt from free pool
		pw = pq_pop(self->m->pkts, self->m->wait);			
		if (pw == NULL) 
			goto end_thread;

		TLOOP(2) // STEP 2: Read MCTP packet from socket connection
		rv = recv(self->m->conn, &pw->pkt, sizeof(struct mctp_pkt), 0);
		if (rv <= 0) 
		{
			TINT32("recv() returned rv", rv);

			// Put mctp_pkt back to the free pool
			pq_push(self->m->pkts, pw);			

			goto end_thread;
		}
		TINT32("recv() returned rv", rv);

		// Increment packet counter 
		self->packet_count++;

		// Set the time when this packet was received 
		timespec_get(&pw->ts, CLOCK_MONOTONIC);

		TLOOP(3) // STEP 3: Post mctp_packet to the Receive Packet Queue (RPQ)
		rv = pq_push(self->m->rpq, pw);
		if (rv != 0) 
		{
			self->dropped_count++;
	
			// Put the mctp_packet back into the pool
			pq_push(self->m->pkts, pw);			

			continue;
		}

	 } while (self->m->stop_threads == 0);

end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

/**
 * Packet Reader Thread
 *
 * @param arg This is a void * but will only ever be a struct packet_reader*
 *
 * STEPS
 *  1: Get an mctp_packet from the Receive Packet Queue 
 *  2: Verify the MCTP header version. Drop packet if unsupported
 *  3: Verify Destination ID 
 *  4: Verify sequence number
 *  5: If SOM, verify completion of prior message 
 *  6: If not SOM, verify the SOM has been received for this tag 
 *  7: Verify Tag Owner field matches
 *  8: If SOM, check out a new message buffer from the pool
 *  9: Copy data from the packet into the message
 * 10: Determine if the entire packet has been received
 * 11: Entire msg has been received. Posting to Receive Message Queue (RMQ)
 * 12: Increment the expected packet sequence number 
 * 13: Return the packet buffer back to the pool
 */  
void *mctp_packet_reader(void *arg)
{
	struct packet_reader *self;
	struct mctp_pkt_wrapper *pw;
	//struct mctp_pkt *mp;
	struct mctp_msg *mm;
	__u8 tag;
	int rv;

	// Initialize variables
	self = (struct packet_reader*) arg;
	TINIT

	TENTER
	
	// Thread Loop
	do 
	{
	 	TLOOP(1) // LOOP 1: Get an mctp_packet from the Receive Packet Queue (RPQ)
		pw = pq_pop(self->m->rpq, self->m->wait);
		if (pw == NULL) 
			goto end_thread;

		// Increment the packet counter 
		self->packet_count++;

		// Print the packet
		if (self->m->verbose & MCTP_VERBOSE_PACKET)
			mctp_prnt_pkt_wrapper(pw);

		TLOOP(2) // LOOP 2: Verify the MCTP header version. Drop packet if unsupported
		if (pw->pkt.hdr.ver != 1) 
		{
			self->dropped_version++;
			goto drop;
		}

		// Extract values for convenience 
		tag = pw->pkt.hdr.tag;

		TLOOP(3) // LOOP 3: Verify Destination ID 
		// TBD

		TLOOP(4) // LOOP 4: Verify sequence number

		// If new pkt seq num doesn't match the expected value then a pkt has been lost
		if (self->pkt_seq != pw->pkt.hdr.seq) 
		{
			// Cancel in process message for this message tag if there is one 
			if (self->tags[tag] != NULL) 
			{
				// Return in process message buffer to the pool
				pq_push(self->m->msgs, self->tags[tag]);

				// Set the in process message to NULL
				self->tags[tag] = NULL;
			}

			self->dropped_seqnum++;

			// If this isn't a SOM packet then we drop it until we get a SOM packet
			if (pw->pkt.hdr.som == 0) 
				goto drop;
			// If this is a SOM packet we can keep it and reset the expected seq number and the expected msg tag 
			else 
				self->pkt_seq = pw->pkt.hdr.seq;
		}

		TLOOP(5) // LOOP 5: If SOM, verify completion of prior message 

		// If new packet is SOM, then the in process message for this tag should be NULL,
		// If the in process msg for this tag isn't NULL, then we lost the EOM packet for the prior message 
		// then we need to cancel the prior in process message
		if ( (pw->pkt.hdr.som == 1) && (self->tags[tag] != NULL) ) 
		{
				// Return in process message buffer to the pool
				pq_push(self->m->msgs, self->tags[tag]);

				// Set the in process message to NULL
				self->tags[tag] = NULL;

				// increment dropped packets counter, but we really don't know how many packets have been lost
				self->dropped_noeom++; 
		}

		TLOOP(6) // LOOP 6: If not SOM, verify the SOM has been received for this tag 
		if ( (pw->pkt.hdr.som == 0)	&& (self->tags[tag] == NULL) ) 
		{
				// increment dropped packets counter, but we really don't know how many packets have been lost
				self->dropped_nosom++;
		mm->len += MCLN_BTU;

				// drop this packet
				goto drop;
		}

		TLOOP(7) // LOOP 7: Verify Tag Owner field matches
		// If they don't match, drop the in process message 
		if ( (self->tags[tag] != NULL) && (pw->pkt.hdr.owner != self->tags[tag]->owner) ) 
		{
				// Return in process message buffer to the pool
				pq_push(self->m->msgs, self->tags[tag]);

				// Set the in process message to NULL
				self->tags[tag] = NULL;

				// increment dropped packets counter, but we really don't know how many packets have been lost
				self->dropped_wrongto++;
		}

		TLOOP(8) // LOOP 8: If SOM, check out a new message buffer from the pool
		if ( pw->pkt.hdr.som == 1 ) 
		{
			TLOOP(9) // Get new message buffer from the pool
			mm = pq_pop(self->m->msgs, self->m->wait);
			if (mm == NULL) 
				goto end_thread;

			// Set mctp_msg header fields
			mm->dst   = pw->pkt.hdr.dest;
			mm->src   = pw->pkt.hdr.src;
			mm->owner = pw->pkt.hdr.owner;
			mm->tag   = pw->pkt.hdr.tag;
			mm->type  = pw->pkt.payload[0];
			mm->len   = 0;
			timespec_copy(&mm->ts, &pw->ts);

			memcpy(&mm->payload[mm->len], &pw->pkt.payload[1], MCLN_BTU-1);
			mm->len += (MCLN_BTU-1);

			// Insert new message buffer into in process array 
			self->tags[tag] = mm;
		}
		else
		{
			TLOOP(10) // LOOP 9: Copy data from the packet into the message
			mm = self->tags[tag];
			memcpy(&mm->payload[mm->len], pw->pkt.payload, MCLN_BTU);
			mm->len += MCLN_BTU;
		}
		
		TLOOP(11) // LOOP 10: Determine if the entire packet has been received
		// If it has, post message buffer to message thread queue and clear the in process message
		if ( pw->pkt.hdr.eom == 1 ) 
		{
			if (self->m->verbose & MCTP_VERBOSE_MESSAGE)
				mctp_prnt_msg(mm);

			TLOOP(12) // LOOP 11: Entire msg has been received. Posting to Receive Message Queue (RMQ)
			rv = pq_push(self->m->rmq, mm);
			if ( rv != 0 )
				goto end_thread;

			// Set the in process message to NULL
			self->tags[tag] = NULL;

			self->message_count++;
		}
	
drop:

		TLOOP(13) // LOOP 12: Increment the expected packet sequence number 
		self->pkt_seq = (self->pkt_seq + 1) % 4;

		TLOOP(14) // LOOP 13: Return the packet back to the pool
		pq_push(self->m->pkts, pw);	

	} while (self->m->stop_threads == 0);

end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

/**
 * Message Handler Thread
 *
 * @param arg This is a void * but will only ever be a struct message_handler*
 *
 * STEPS
 * 1: Get an mctp_msg from the Receive Message Queue (RMQ)
 * 2: Get the message handler function and call it
 */
void *mctp_message_handler(void *arg)
{
	struct message_handler *self;
	struct mctp_msg *mm;
	struct mctp_action *ma;

	// Initialize variables
	self = (struct message_handler*) arg;
	TINIT

	TENTER

	// Thread Loop
	do 
	{
	 	TLOOP(1) // LOOP 1: Get an mctp_msg from the Receive Message Queue (RMQ)
		mm = pq_pop(self->m->rmq, self->m->wait);
		if (mm == NULL)  
			goto end_thread;

		if (mm->owner == 1)
		{
			TLOOP(2) // LOOP 2: New MSG request. Get the message handler function and call it
			
			// Check out a new mctp_action 
			ma = pq_pop(self->m->actions, 1);
			if (ma == NULL)
				goto end_thread;

			// Put new message into the action with other data
			ma->req = mm;
			timespec_copy(&ma->created, &mm->ts);

			// Call action handler for this message type 
			self->m->handlers[mm->type](self->m, ma);	
		}
		else 
		{
			TLOOP(3) // LOOP 3: A MSG response. Find action in tags and call completion function / handler

			// Lock mutex for tags array 
			pthread_mutex_lock(&self->m->tags_mtx);
			{ 
				// Get action for this tag from tags array
				ma = self->m->tags[mm->tag];

				// Clear entry in the tags array
				self->m->tags[mm->tag] = NULL;
			}
			pthread_mutex_unlock(&self->m->tags_mtx);
			
			// There was no outstanding mctp_action that corresponded to this tag, silently drop the message 
			if (ma == NULL)
			{
				pq_push(self->m->msgs, mm);
				continue;
			}

			// Put response message into the action with other data
			ma->rsp = mm;
			timespec_get(&ma->completed, CLOCK_MONOTONIC);

			// If the action has a unique completion handler, call it, otherwise call regular handler
			if (ma->fn_completed != NULL)
				ma->fn_completed(self->m, ma);
			else 
				self->m->handlers[mm->type](self->m, ma);	
		}
		
	} while (self->m->stop_threads == 0);

end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

/**
 * Packet Writer Thread
 *
 * @param arg This is a void * but will only ever be a struct packet_writer*
 *
 * STEPS
 * 1: Get an mctp_msg from the Transmit Message Queue
 * 2: Determine length of message
 * 3: Breakup mctp_msg into mctp_packets
 * 4: Submit mctp_packets to Transmit Packet Queue (TPQ)
 * 5: Check mctp_msg back in to free pool
 */
void *mctp_packet_writer(void *arg)
{
	struct packet_writer *self;
	int rv, i, num_pkts;
	struct mctp_action *ma;
	struct mctp_msg *mm;
	struct mctp_pkt_wrapper *pw, *prev;

	// Initialize variables
	self = (struct packet_writer*) arg;
	TINIT

	TENTER

	// Thread Loop
	do
	{
	 	TLOOP(1) // LOOP 1: Get an mctp_action from the Transmit Message Queue
		ma = pq_pop(self->m->tmq, self->m->wait);
		if (ma == NULL) 
			goto end_thread;

		// Determine which message we are sending, the request or the response
		if (ma->rsp != NULL)
			mm = ma->rsp;
		else 
			mm = ma->req;

		if (self->m->verbose & MCTP_VERBOSE_MESSAGE)
			mctp_prnt_msg(mm);

		// Increment the message counter 
		self->message_count++;

		TLOOP(2) // LOOP 2: Determine length of message
		num_pkts = mctp_pkt_count(mm);

		TLOOP(3) // LOOP 3: Breakup mctp_msg into mctp_packets
		for ( i = 0 ; i < num_pkts ; i++ ) 
		{
			TLOOP(4) // 4: Check out mctp_pkt_wrapper 
			pw = pq_pop(self->m->pkts, self->m->wait);
			if (pw == NULL)
				goto end_thread;

			// Build linked list of mctp_pkt_wrappers in the mctp_action 
			if (i == 0)
			{
				pw->next = NULL;
				ma->pw = pw;
				prev = ma->pw;
			}
			else 
			{
				pw->next = NULL;
				prev->next = pw;
				prev = pw;
			}

			// Increment the packet counter 
			self->packet_count++;

			// Copy header info to packet 
			pw->pkt.hdr.ver   = 1;
			pw->pkt.hdr.dest  = mm->dst;
			pw->pkt.hdr.src   = mm->src;
			pw->pkt.hdr.owner = mm->owner;
			pw->pkt.hdr.tag   = mm->tag;

			// Determine if this is the End of Message Packet
			if (i == (num_pkts - 1))
				pw->pkt.hdr.eom = 1;

			// Set packet sequence
			pw->pkt.hdr.seq = self->pkt_seq;

			// Increment Packet Sequence for next packet
			self->pkt_seq = (self->pkt_seq + 1) % 4;

			// Determine if this is the Start of Message Packet
			if (i == 0)
			{
				pw->pkt.hdr.som = 1;
				pw->pkt.payload[0] = mm->type;
				memcpy(&pw->pkt.payload[1], &mm->payload, MCLN_BTU-1);
			}
			else
			{
				// Copy data from mctp_msg data buffer to this mctp_packet data buffer
				memcpy(pw->pkt.payload, &mm->payload[(i*MCLN_BTU)-1], MCLN_BTU);
			}
		}

		TLOOP(5) // LOOP 5: Submit mctp_action to Transmit Packet Queue (TPQ)
		rv = pq_push(self->m->tpq, ma);
		if ( rv != 0 ) 
			goto end_thread;

	} while (self->m->stop_threads == 0);

end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

/**
 * Socket Writer Thread
 *
 * @param arg This is a void * but will only ever be a struct socket_writer*
 *
 * STEPS
 * 1: Get an mctp_packet from the Transmit Packet Queue (TPQ)
 * 2: Send mctp_packet using socket connection
 * 3: Check mctp_packet back into the pool
 */
void *mctp_socket_writer(void *arg)
{
	struct socket_writer *self;
	struct mctp_action *ma;
	struct mctp_pkt_wrapper *pw;
	int rv;

	// Initialize variables
	self = (struct socket_writer*) arg;
	TINIT

	TENTER 

	// Thread Loop 
	do 
	{
	 	TLOOP(1) // LOOP 1: Get an mctp_action from the Transmit Packet Queue (TPQ)
		ma = pq_pop(self->m->tpq, self->m->wait);
		if (ma == NULL) 
			goto end_thread;

		pw = ma->pw;
		// loop through the packet linked list and send each packet
		while (pw != NULL)
		{
			// Increment the packet counter 
			self->packet_count++;

			TLOOP(2) // LOOP 2: Send mctp_packet using socket connection
			rv = send(self->m->conn, &pw->pkt, sizeof(struct mctp_pkt), 0);
			if (rv <= 0) 
			{
				// If there was an error, put the mctp_action into the action completion queue and end 
				ma->completion_code = 1;
				pq_push(self->m->acq, ma);			
				goto end_thread;
			}

			// Get next mctp_pkt_wrapper in the linked list
			pw = pw->next;
		}
		
		// Set time of mctp_action completion 
		timespec_get(&ma->completed, CLOCK_MONOTONIC);

		// If the response is not null, then we need to complete the action here 
		if (ma->rsp != NULL) 
		{
			TLOOP(3) // LOOP 3: Push mctp_action onto the Action Completion Queue
			rv = pq_push(self->m->acq, ma);
			if (rv != 0) 
				goto end_thread;
		}

	} while (self->m->stop_threads == 0);

end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

/**
 * Submission Thread 
 *
 * @param arg This is a void * but will only ever be a struct submission_thread*
 *
 * STEPS
 * 1: Loop through tag array and check if any out standing messages need to be resubmitted or retired 
 * 2: Loop through tag array and check for empty slots 
 * 3: Put thread to sleep 
 */
void *mctp_submission_thread(void *arg)
{
	struct submission_thread *self;
	struct mctp_action *ma;
	struct timespec ts;
	int i, rv;

	// Initialize variables
	self = (struct submission_thread*) arg;
	TINIT

	TENTER 

	// Thread Loop 
	do 
	{
		pthread_mutex_lock(&self->m->tags_mtx);
		{
	 		//TLOOP(1) // LOOP 1: Loop through tag array and check if any out standing messages need to be resubmitted or retired 
			for ( i = 0 ; i < MCTP_NUM_TAGS ; i++ )
			{
				ma = self->m->tags[i];

				// If this slot is NULL skip it 
				if (ma == NULL) 
					continue; 

				// Test if timeout has elapsed, if not skip
				timespec_add(&ma->submitted, &self->action_delta, &ts);
				rv = timespec_elapsed(&ts, CLOCK_MONOTONIC);
				if (rv == 0) 
					continue;

				// if we have exceeded the retry count, retire action 
				if (ma->num >= ma->max) 
				{
					// If action has a retire function call it
					if (ma->fn_failed != NULL) 
						ma->fn_failed(self->m, ma);
					else 
						mctp_retire(self->m, ma);

					// Set the current tag to NULL to clear it 
					self->m->tags[i] = NULL;
				}
				else 
				{
					// Increment the message submission count 
					ma->num++;

					// Set the submission time to now 
					timespec_get(&ma->submitted, CLOCK_MONOTONIC);

					// Resubmit the mctp_action
					pq_push(self->m->tmq, ma);
				}
			}
			
	 		//TLOOP(2) // LOOP 2: Loop through tag array and check for empty slots 
			for ( i = 0 ; i < MCTP_NUM_TAGS ; i++ )
			{
				ma = self->m->tags[i];

				// If this slot is not NULL skip it 
				if (ma != NULL) 
					continue; 

				// If there is no action in this tag slot check if there is a new command to issue 
				ma = pq_pop(self->m->taq, 0);	

				// If ma is NULL then there are no actions in the submission queue 
				if (ma == NULL) 
					continue;
				
				// Fill out action 
				ma->num = 1;
				timespec_get(&ma->submitted, CLOCK_MONOTONIC);

				// Set tag in msg 
				ma->req->tag = i;
				self->m->tags[i] = ma;

				// submit mctp_action to tmq
				rv = pq_push(self->m->tmq, ma);
			}
		}
		pthread_mutex_unlock(&self->m->tags_mtx);

		//TLOOP(3) // LOOP 3: Put thread to sleep 
		pthread_mutex_lock(&self->mtx);
		{ 
			// Get current time and then add the thread delta to it		
			timespec_get(&self->thread_timeout, CLOCK_MONOTONIC); 
			timespec_add(&self->thread_timeout, &self->thread_delta, &self->thread_timeout);
	
			// Wait for signal to wake up
			rv = 0;
			self->wake = 0;
			while (rv != ETIMEDOUT && self->wake == 0)
				rv = pthread_cond_timedwait(&self->cond, &self->mtx, &self->thread_timeout);
			self->wake = 0;
		}
		pthread_mutex_unlock(&self->mtx);

	} while (self->m->stop_threads == 0);

//end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

/**
 * Action Completion Thread 
 *
 * @param arg This is a void * but will only ever be a struct completion_thread*
 *
 * STEPS
 */
void *mctp_completion_thread(void *arg)
{
	struct completion_thread *self;
	struct mctp_action *ma;

	// Initialize variables
	self = (struct completion_thread*) arg;
	TINIT

	TENTER 

	// Thread Loop 
	do 
	{
		TLOOP(1) // LOOP 1: Pop an action off of the Action Completion Queue (ACQ)
		ma = pq_pop(self->m->acq, 1);
		if (ma == NULL) 
			goto end_thread;

		// Set completion time 
		timespec_get(&ma->completed, CLOCK_MONOTONIC);

		// Increment completed action counter 
		self->completed_actions++;

		if (ma->completion_code != 0)
		{
			TLOOP(2) // LOOP 2: Increment failed action counter 
			self->failed_actions++;

			// If the action has a fail handler call it
			if (ma->fn_failed != NULL)
				ma->fn_failed(self->m, ma);
			else 
				mctp_retire(self->m, ma);	
		}
		else 
		{
			TLOOP(3) // LOOP 3: Increment successful action counter 
			self->successful_actions++;

			// If the action has a completed handler call it
			if (ma->fn_completed != NULL)
				ma->fn_completed(self->m, ma);
			else 
				mctp_retire(self->m, ma);	
		}

	} while (self->m->stop_threads == 0);

end_thread:

	TEXIT( (self->m->stop_threads == 0) && (self->m->use_threads == 1) );

	// If thread exited abnormally, request other threads to stop
	if ( (self->m->stop_threads == 0) && (self->m->use_threads == 1) ) 
		mctp_request_stop(self->m);

	return NULL;
}

