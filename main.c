/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file 		mctp.c
 *
 * @brief 		Code file for MCTP transport library
 *
 * @details 	As per MCTP specification, all MCTP fields are Big Endian
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jan 2024
 * @author 		Barrett Edwards <code@jrlabs.io>
 * 
 */

/* INCLUDES ==================================================================*/

/* gettid()
 */
#define _GNU_SOURCE

/* exit()
 */
//#include <stdlib.h>

/* errno
 */
#include <errno.h>

/* printf()
 */
#include <stdio.h>

/* free()
 */
#include <stdlib.h>

/* memset()
 * memcpy()
 */
#include <string.h>

/* pthread_t 
 * pthread_create()
 * pthread_join()
 * pthread_getthreadid_np()
 */
#include <pthread.h>

/* __u32
 */
#include <linux/types.h>

/* sem_t
 * sem_init()
 * sem_timedwait()
 */
#include <semaphore.h>

/* autl_prnt_buf()
 */
#include <arrayutils.h>
#include <timeutils.h>
#include <ptrqueue.h>

#include "mctp.h"

/* MACROS ====================================================================*/

//#define MCTP_VERBOSE
#ifdef MCTP_VERBOSE
 #define INIT 			unsigned step = 0;
 #define ENTER 					if (m->verbose & MCTP_VERBOSE_THREADS) 	printf("%d:%s Enter\n", 				gettid(), __FUNCTION__);
 #define STEP 			step++; if (m->verbose & MCTP_VERBOSE_STEPS) 	printf("%d:%s STEP: %u\n", 				gettid(), __FUNCTION__, step);
 #define HEX32(k, i)			if (m->verbose & MCTP_VERBOSE_STEPS) 	printf("%d:%s STEP: %u %s: 0x%x\n",		gettid(), __FUNCTION__, step, k, i);
 #define INT32(k, i)			if (m->verbose & MCTP_VERBOSE_STEPS) 	printf("%d:%s STEP: %u %s: %d\n",		gettid(), __FUNCTION__, step, k, i);
 #define ERR32(k, i)			if (m->verbose & MCTP_VERBOSE_ERROR) 	printf("%d:%s STEP: %u ERR: %s: %d\n",	gettid(), __FUNCTION__, step, k, i);
 #define EXIT(rc) 				if (m->verbose & MCTP_VERBOSE_THREADS)	printf("%d:%s Exit: %d\n", 				gettid(), __FUNCTION__,rc);
#else
 #define INIT
 #define ENTER
 #define STEP
 #define HEX32(k, i)
 #define INT32(k, i)
 #define ERR32(k, i)
 #define EXIT(rc)
#endif

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/* GLOBAL VARIABLES ==========================================================*/

/**
 * String representation of MCTP Threads Run Mode (RM)
 */
const char *STR_MCRM[] = {
	"Server",	// MCRM_SERVER	   	= 0,
	"Client"	// MCRM_CLIENT 	= 1
};

/* String representation of MCTP Message Type Codes (MT)
 *
 * See DSP0239 v1.9.0 Table 1.
 */
const char *STR_MCMT[] = {
	"CONTROL", 		// MCMT_CONTROL		= 0x00,
	"PLDM",	  		// MCMT_PLDM 		= 0x01,
	"NCSI",	  		// MCMT_NCSI 		= 0x02,
	"ETHERNET",		// MCMT_ETHERNET	= 0x03,
	"NVMEMI",  		// MCMT_NVMEMI		= 0x04,
	"SPDM",	  		// MCMT_SPDM 		= 0x05,
	"SECURE",  		// MCMT_SECURE		= 0x06,
	"CXLFMAPI",		// MCMT_CXLFMAPI	= 0x07,
	"CXLCCI ", 		// MCMT_CXLCCI		= 0x08,
	"VDM_PCI", 		// MCMT_VDM_PCI		= 0x7E,
	"VDM_IANA"		// MCMT_VDM_IANA	= 0x7F
};

/* PROTOTYPES ================================================================*/

/* FUNCTIONS =================================================================*/

/**
 * Convenience function to fill MCTP Header fields
 *
 * @param mm 	struct mctp_msg* to fill 
 * @param dest 	Destination EID 
 * @param src   Source EID 
 * @param owner Bit indicating if SRC is the owner
 * @param tag   Tag ID to track multiple outstanding commands 
 */
void mctp_fill_msg_hdr(
	struct mctp_msg *mm,
	__u8 dest,
	__u8 src,
	__u8 owner,
	__u8 tag)
{
	mm->dst = dest;
	mm->src = src;
	mm->owner = owner;
	mm->tag = tag;
}

/**
 * Free memory allocated by init function
 * 
 * STEPS
 * 1: Verify input
 * 2: Close socket connection
 * 3: Destroy Mutexes 
 * 4: Free queues
 * 5: Free mctp struct memory
 */
int mctp_free(struct mctp *m)
{
	INIT 
	struct mctp_version *head, *curr, *next;
	int rv;

	ENTER

	// Initialize variables
	rv = 1;

	STEP // 1: Verify input
	if (m == NULL) {
		errno = EINVAL;
		rv = -1;
		goto end;
	}

	STEP // 2: Close socket connection
	if (m->conn != m->sock)
		close(m->conn);	
	close(m->sock);	

	STEP // 3: Destroy Mutexes 
	pthread_mutex_destroy(&m->mtx);
	pthread_cond_destroy(&m->cond);
	pthread_mutex_destroy(&m->tags_mtx);
	
	STEP // 4: Free queues
	pq_free(m->rpq);
	pq_free(m->rmq);
	pq_free(m->tpq);
	pq_free(m->tmq);
	pq_free(m->taq);
	pq_free(m->acq);
	pq_free(m->pkts);
	pq_free(m->msgs);
	pq_free(m->actions);

	STEP // 5 Free MCTP Versions array 
	head = m->mctp_versions;
	while (head != NULL)
	{
		curr = head;
		head = head->next_type;
		while (curr != NULL)
		{
			next = curr->next_entry;
			free(curr);
			curr = next;
		}
	}

	STEP // 6: Free mctp struct memory
	free(m);

	rv = 0;

end:

	EXIT(rv);

	return rv;
}

/**
 * Get the verbosity bit mask
 */
__u32 mctp_get_verbosity(struct mctp *m)
{
	return m->verbose;
}

/**
 * Initialize an mctp object
 *
 * STEPS
 * 1: Allocate memory for mctp struct
 * 2: Initialize message handlers
 * 3: Initialize message_handler thread
 * 4: Initialize UUID
 * 5: Initialize mutex variables
 */
struct mctp *mctp_init()
{
	struct mctp *m;
 
	// STEP 1: Allocate memory for mctp struct
	m = (struct mctp*) calloc (1, sizeof(struct mctp));
	if (m == NULL) 
	{
		errno = EFAULT;
		return NULL;
	}

	// STEP 2: Initialize message handlers
	m->handlers[MCMT_CONTROL] = mctp_ctrl_handler;

	// STEP 3: Initialize message_handler thread
	m->fn_sr = mctp_socket_reader;
	m->fn_pr = mctp_packet_reader;
	m->fn_mh = mctp_message_handler;
	m->fn_pw = mctp_packet_writer;
	m->fn_sw = mctp_socket_writer;
	m->fn_st = mctp_submission_thread;
	m->fn_ct = mctp_completion_thread;

	// STEP 4: Initialize UUID
	uuid_generate(m->uuid);
	memcpy(m->state.uuid, m->uuid, MCLN_UUID);

	// STEP 5: Initialize mutex variables
	pthread_mutex_init(&m->mtx, NULL);
	pthread_cond_init(&m->cond, NULL);
	pthread_mutex_init(&m->tags_mtx, NULL);

	// STEP 6: Initialize mctp_versions array
	m->mctp_versions = NULL;

	mctp_set_version(m, MCMT_BASE,    0xF1,0xF3,0xF1,0x00);
	mctp_set_version(m, MCMT_CONTROL, 0xF1,0xF3,0xF1,0x00);

	return m;
}

/**
 * Determine the number of packets needed for this MCTP Message
 * 
 * @return the number of packets, 0 if error
 */
int mctp_pkt_count(struct mctp_msg *mm)
{
	int rv;

	// Initialize variables
	rv = 0;

	switch (mm->type)
	{
		case MCMT_CONTROL: 		rv = 1;		break; 	// All MCTP control messages are 1 packet long
		case MCMT_PLDM:								
		case MCMT_NCSI: 							
		case MCMT_ETHERNET:							
		case MCMT_NVMEMI:							
		case MCMT_SPDM: 					
		case MCMT_SECURE:						
		case MCMT_CXLFMAPI:								
		case MCMT_CSE:
		case MCMT_CXLCCI:					
		case MCMT_VDM_PCI:						
		case MCMT_VDM_IANA:					
		{
			// Compute the number of MCLN_BTU sized packets needed  
			rv = mm->len / MCLN_BTU; 
			if ((mm->len % MCLN_BTU) > 0 )
				rv++;
		}	
			break;
		default:							break;
	}
	return rv;
}

/**
 * Print an MCTP Transport Header
 */
void mctp_prnt_hdr(struct mctp_hdr *mh)
{
	if (mh == NULL)
		return;
	
	printf("MCTP Header:\n");
	printf("Header version:         0x%x\n", mh->ver);
	printf("Destination EID:        0x%02x\n", mh->dest);
	printf("Source EID:             0x%02x\n", mh->src);
	printf("Start of Message:       %d\n", mh->som);
	printf("End of Message:         %d\n", mh->eom);
	printf("Packet Sequence #:      0x%x\n", mh->seq);
	printf("Tag Owner:              %d\n", mh->owner);
	printf("Tag:                    0x%x\n", mh->tag);
}

/**
 * Print MCTP Pkt
 */
void mctp_prnt_pkt(struct mctp_pkt *mp)
{
	if (mp == NULL) 
		return;
	
	// Print the MCTP Transport Header
	mctp_prnt_hdr(&mp->hdr);

	// Print the payload 
	autl_prnt_buf(mp, sizeof(struct mctp_pkt), 4, 1);
}

/**
 * Print MCTP Packet Wrapper
 */
void mctp_prnt_pkt_wrapper(struct mctp_pkt_wrapper *pw)
{
	if (pw == NULL) 
		return;

	printf("MCTP Packet Wrapper:\n");
	timespec_print(&pw->ts);
	printf("Next:     %p\n", pw->next);
	mctp_prnt_pkt(&pw->pkt);
}

/*
 * Print an MCTP Message Type
 */
void mctp_prnt_type(struct mctp_type *mt) 
{
	printf("MCTP Type:\n");
	printf("Integrity Check:        %d\n", mt->IC);
	printf("Message Type:           0x%02x  %s\n", mt->type, mcmt(mt->type));
}

/**
 * Print the current MCTP Endpoint Configuration
 */
void mctp_prnt_state(struct mctp_state *ms)
{
	char buf[37];
	// Convert UUID into String for printing
	uuid_unparse(ms->uuid, buf);

	printf("MCTP State:\n");
	printf("Endpoint ID:        %02x\n", ms->eid);
	printf("Bus Owner EID:      %02x\n", ms->bus_owner_eid);
	printf("Verbose Flags:      %08x\n", ms->verbose);
	printf("UUID:               %s\n", buf);
}

/**
 * Print MCTP Message
 */
void mctp_prnt_msg(struct mctp_msg *mm)
{
	if (mm == NULL) 
		return;

	printf("MCTP Message:\n");
	printf("Destination EID:        0x%02x\n", mm->dst);
	printf("Source EID:             0x%02x\n", mm->src);
	printf("Type:                   0x%02x - %s\n", mm->type, mcmt(mm->type));
	printf("Tag Owner:              %d\n", mm->owner);
	printf("Tag:                    %d\n", mm->tag);
	printf("Payload Len:            %d\n", mm->len);
	printf("Payload:\n");

	// Print the payload in bytes
	autl_prnt_buf(mm->payload, mm->len, 4, 1);
}

/**
 * Retire an MCTP action 
 *
 * Checks in the mctp_msg and mctp_action to the central free pools 
 * @param m	struct mctp* 
 * @param a struct mctp_action*
 */
void mctp_retire(struct mctp* m, struct mctp_action *a)
{
	struct mctp_pkt_wrapper *pw, *next;

	// Check in msg
	if (a->req != NULL)
		pq_push(m->msgs, a->req);
	if (a->rsp != NULL)
		pq_push(m->msgs, a->rsp);

	if (a->pw != NULL)
	{
		pw = a->pw;
		do	
		{
			next = pw->next;
			pw->next = NULL;
			pq_push(m->pkts, pw);
			pw = next;
		} while (pw != NULL);
	}

	// Clear action 
	memset(a, 0, sizeof(struct mctp_action));

	// Check in action 
	pq_push(m->actions, a);
}

/** 
 * Start the threads 
 *
 * @return 	 0 on success
 *			-1 on socket create failure (both)
 *			-2 on socket bind failure (server)
 *			-3 on socket connect failure (client)
 *			 1 on pthread_create() failure (both)
 *			 2 on connection_thread start failure (both)
 *
 * STEPS
 * 1: Store parameters in mctp object 
 * 2: Create socket
 * 3: Set parameters for server socket
 * 4: Configure socket 
 * 5: Start connection thread
 */
int mctp_run(struct mctp *m, int port, __u32 address, int mode, int use_threads, int dontblock)
{
	INIT 
	int rv;
	sem_t sem;
	struct timespec ts, delta;

	ENTER 

	// Initialize variables 
	rv = -1;
	delta.tv_sec = 1;
	delta.tv_nsec = 0;

	STEP // 1: Store parameters in mctp object 
	m->port = port;
	m->mode = mode;
	m->use_threads = use_threads;
	m->wait = use_threads;

	STEP // 2: Create socket
	m->sock = socket(AF_INET, SOCK_STREAM, 0);
	if ( m->sock < 0 ) 
	{
		ERR32("Could not create socket. rv:", m->sock);
		rv = -1;
		goto close;
	}

	STEP // 3: Set parameters for server socket
	memset( &m->sa_server, 0, sizeof(struct sockaddr_in));
	m->sa_server.sin_family = AF_INET;			
	m->sa_server.sin_port = htons(port);
	m->sa_server.sin_addr.s_addr = address;

	STEP // 4: Configure Socket
	if ( mode == MCRM_SERVER ) 
	{
		// Bind to socket
		rv = bind(m->sock, (struct sockaddr *) &m->sa_server, sizeof(struct sockaddr_in));
		if ( rv < 0 ) 
		{
			ERR32("Could not bind socket. rv",  rv);
			rv = -2;
			goto close;
		}

		// Listen on socket
		listen(m->sock,5);
	}
	else {
		// Connect to the server as a client
		rv = connect(m->sock, (struct sockaddr *) &m->sa_server, sizeof(struct sockaddr_in));
		if ( rv < 0 ) 
		{
			ERR32("Socket connect failed. rv:", rv);
			rv = -3;
			goto close;
		}
		m->conn = m->sock;
	}

	// Set struct mctp pointer in Connection Handler object
	m->ch.m = m;
	m->ch.dontblock = dontblock;
	m->ch.sem = NULL;

	STEP // 5: Start Connection Handler Thread
	// If the user specified dontblock, then start the connection handler thread function as a independent thread and return
	if (dontblock) 
	{
		// Initialize sempahore 
		sem_init(&sem, 0, 0);
		m->ch.sem = &sem;

		// Start thread 
		rv = pthread_create( &m->pt_ch, NULL, mctp_connection_handler, (void*) &m->ch);
		if ( rv != 0 ) 
		{
			ERR32("Could not create Connection Handler Thread", rv);
			rv = 1;
			goto close;
		}

		// Compute timeout to wait for semaphore
		timespec_get(&ts, CLOCK_MONOTONIC);
		timespec_add(&ts, &delta, &ts);
		
		// Pend on a semaphore until all the threads are running 
		rv = sem_timedwait(&sem, &ts);

		sem_destroy(&sem);
		if (rv != 0) 
		{
			ERR32("Threads failed to start", rv);
			rv = 2;
			goto close;
		}
	}
	else 
	{
		mctp_connection_handler(&m->ch);
	}

	rv = 0;

	goto end;

close:

	close(m->sock);

end:

	EXIT(rv)

	return rv;
}

/**
 * Specify the function to call for a MCTP Message type 
 */
void mctp_set_handler (
	struct mctp *m, 
	int type, 
	int (*func)(struct mctp *m, struct mctp_action *ma))
{
	if (type < MCMT_MAX)
		m->handlers[type] = func;
}

/**
 * Set the function to be called as the message handler thread 
 */
void mctp_set_mh(struct mctp *m, void *(*fn)(void*arg))
{
	m->fn_mh = fn;
}

/**
 * Set the verbosity bit mask
 */
void mctp_set_verbosity(struct mctp *m, __u32 level)
{
	m->verbose = level;
	m->state.verbose = level;
}

/**
 * Method to request mctp threads to stop
 *
 * This is called by the thread functions to say they exited abnormally
 * This is called when a thread has experienced an error and needs to tell the main thread to stop all the other threads
 * Pend upon the mutex which will unlock when the main thread calls pthread_cond_wait()
 * When the lock is obtained, tell the main thread to stop all the threads by setting a bit, 
 * then issue signal, then unlock, then exit
 */
void mctp_request_stop(struct mctp *m)
{
	pthread_mutex_lock(&m->mtx);
	{
		m->stop_threads = 2;
		pthread_cond_signal(&m->cond);
	}
	pthread_mutex_unlock(&m->mtx);
}

/**
 * Instruct all threads to stop, wait, join
 * 
 * This can only be called by an external thread, not by any of the child mctp threads
 *
 * @detail Pend upon the mutex which will unlock when the main thread calls pthread_cond_wait()
 *         When the lock is obtained, tell the main thread to stop all the threads by setting a bit, 
 *         then issue signal, then unlock, then exit
 */
int mctp_stop(struct mctp *m)
{
	pthread_mutex_lock(&m->mtx);
	{
		// If we get the mutex and the threads haven't started, 
		// then the connection thread has pended on the accept() and won't 
		// return, so cancel the thread 
		if ( ( m->use_threads != 0 ) && ( m->all_threads_started == 0 ) ) {
			pthread_cancel(m->pt_ch);
			close(m->sock);
		}
		else {
			m->stop_threads = 1;
			pthread_cond_signal(&m->cond);
		}
	}
	pthread_mutex_unlock(&m->mtx);
	
	// Join with the connection handler thread before exiting function
	pthread_join(m->pt_ch, NULL);

	return 0;
}


/**
 * Submit an object for transmission 
 *
 * @param m 			struct mctp* 
 * @param type  		mctp message type
 * @param obj   		Pointer to serialized data buffer to send 
 * @param len   		Length of object in bytes 
 * @param retry 		Number of attempts to send the object. -1=forever, -2=default
 * @param user_data 	void* to a user data object to keep with the action until completion 
 * @param fn_submitted 	Function to call when action is submitted to tmq
 * @param fn_completed 	Function to call when response to action is received 
 * @param fn_failed 	Function to call when retry attempts have elapsed 
 * @return              struct mctp_action* of the action submitted. NULL on error and sets errno
 *
 * STEPS
 * 1: Validate inputs 
 * 2. Prepare message 
 * 3. Prepare action 
 * 4. Submit action	
 */ 
struct mctp_action *mctp_submit(
	struct mctp *m, 
	int type, 
	void *obj, 
	size_t len,
	int retry,
	struct timespec *delta,
	void *user_data,
	void (*fn_submitted)(struct mctp *m, struct mctp_action *a),
	void (*fn_completed)(struct mctp *m, struct mctp_action *a),
	void (*fn_failed)(struct mctp *m, struct mctp_action *a)
)
{
	INIT 
	struct mctp_action *ma;
	struct mctp_msg *mm;
	sem_t sem;
	int rv;

	ENTER

	// Initialize varialbes 
	rv = 1;
	ma = NULL;

 	STEP // 1: Validate inputs 
	if (m == NULL) 
		goto end;
	
	if (obj == NULL) 
		goto end;

	if (len == 0) 
		goto end;

	STEP // 2. Prepare Message 

	// Check out msg 
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL) 
		goto end;

	// Fill out msg 
	mm->owner = 1;
	mm->type = type;
	mm->len = len;
	memcpy(&mm->payload, obj, len);

	STEP // 3. Prepare Action 

	// Check out action 
	ma = pq_pop(m->actions, 1);
	if (ma == NULL) 
		goto end;

	// Fill out action 
	memset(ma, 0, sizeof(struct mctp_action));
	ma->valid = 1;
	ma->req = mm;

	if (retry < -1)
		ma->max = MCTP_ACTION_DEFAULT_RETRY_NUM;
	else 
		ma->max = retry;
	
	timespec_get(&ma->created, CLOCK_MONOTONIC); 

	ma->user_data = user_data;
	ma->fn_submitted = fn_submitted;
	ma->fn_completed = fn_completed;
	ma->fn_failed = fn_failed;

	// Set mctp_action.sem to NULL if we are not going to pend on the sempahore
	if (delta == NULL)
		ma->sem = NULL; 
	else 
	{
		// Initialize Semaphore 
		sem_init(&sem, 0, 0);

		ma->sem = &sem;
	}

	STEP // 4. Submit action	
	
	rv = pq_push(m->taq, ma);
	if (rv != 0)
	{
		ma = NULL;
		errno = EBUSY;
		goto end;
	}

	STEP // 5: Pend on semaphore 
	if (delta != NULL)
	{
		// Compute absolute timeout 
		timespec_get(&ma->timeout, CLOCK_MONOTONIC);
		timespec_add(&ma->timeout, delta, &ma->timeout);

		// Pend on semaphore
		rv = sem_timedwait(&sem, &ma->timeout);

		// Clean up the one time use semaphore
		sem_destroy(&sem);

		// Check response. If the semaphore timedpout, then return NULL to caller
		if (rv != 0)
			ma = NULL;	
	}

end:

	EXIT(rv)

	return ma;
}

/* Functions to return a string representation of an object*/
const char *mcmt(unsigned u)              
{
	int rv;
	switch (u)
	{
		case MCMT_CONTROL: 		rv =  0; break; // 0x00
		case MCMT_PLDM: 		rv =  1; break; // 0x01
		case MCMT_NCSI: 		rv =  2; break; // 0x02
		case MCMT_ETHERNET: 	rv =  3; break; // 0x03
		case MCMT_NVMEMI: 		rv =  4; break; // 0x04
		case MCMT_SPDM: 		rv =  5; break; // 0x05
		case MCMT_SECURE: 		rv =  6; break; // 0x06
		case MCMT_CXLFMAPI: 	rv =  7; break; // 0x07
		case MCMT_CXLCCI: 		rv =  8; break; // 0x08
		case MCMT_VDM_PCI: 		rv =  9; break; // 0x7E
		case MCMT_VDM_IANA: 	rv = 10; break; // 0x7F 
		default: 				return NULL;
	}
	return STR_MCMT[rv];
}

const char *mcrm(unsigned u)
{
	if (u >= MCRM_MAX)
		return NULL;
	return STR_MCRM[u];
}

