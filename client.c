/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file 		client.c
 *
 * @brief 		Code file client example of MCTP Transport Library
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jan 2024
 * @author 		Barrett Edwards <code@jrlabs.io>
 * 
 */

/* INCLUDES ==================================================================*/

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <endian.h>

#include <arrayutils.h>
#include <ptrqueue.h>

#include <fmapi.h>

#include "mctp.h"

/* MACROS ====================================================================*/

#define MCTP_MEM_ALIGNMENT 4096
#define MCTP_RECV_BUFFER_COUNT 1024
#define MCTP_PORT 2508
#define MCTP_MAX_NUM_PACKETS 1024

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/* GLOBAL VARIABLES ==========================================================*/

/* PROTOTYPES ================================================================*/

void *client_thread(void *arg);
int test_ctrl_set_eid(struct mctp *m);
int test_ctrl_get_eid(struct mctp *m);
int test_ctrl_get_msg_type_support(struct mctp *m);
int test_ctrl_get_version_support(struct mctp *m);
int test_ctrl_get_endpoint_uuid(struct mctp *m);
int test_fmapi_identify_switch(struct mctp *m);

/* FUNCTIONS =================================================================*/

int main()
{
	struct mctp *m;

	m = mctp_init();
	if (m == NULL) {	
		goto end;
	}

	// Set Message Handler Thread function 
	mctp_set_mh(m, client_thread);

	// Set verbosity levels
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_ERROR);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_THREADS);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_STEPS);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_PACKET);

	// Run 
	mctp_run(m, MCTP_PORT, 0, MCRM_CLIENT, 1, 0);

	printf("mctp_run() completed\n");


	// Free memory
	mctp_free(m);

	return 0;

end:

	return 1;

}

/**
 * Message Handler Thread that performs client actions
 *
 * This thread function is called in place of the message_handler() function
 *
 * TESTS
 * 1: Set EID 
 * 2: Get EID 
 * 3: Get Version support
 * 4: Get Message Type Support
 * 5: Get Endpoint UUID
 */
void *client_thread(void *arg)
{
	struct message_handler *self;
	struct mctp *m;
	int rv;

	self = (struct message_handler*) arg;
	m = self->m;

	printf("%s Started \n", __FUNCTION__);

	// TEST 1: Set EID 
	printf("-----------------------------------------------------------------\n");
	printf("TEST 1: Set EID\n");
	rv = test_ctrl_set_eid(m);
	if ( rv != 0 ) {
		printf("%s test_ctrl_set_eid failed rv:%d\n", __FUNCTION__, rv);
		goto end;
	}

	// TEST 2: Get EID 
	printf("-----------------------------------------------------------------\n");
	printf("TEST 2: Get EID\n");
	rv = test_ctrl_get_eid(m);
	if ( rv != 0 ) {
		printf("%s test_ctrl_get_eid failed rv:%d\n", __FUNCTION__, rv);
		goto end;
	}

	// TEST 3: Get Version Support
	printf("-----------------------------------------------------------------\n");
	printf("TEST 4: Get Version Support\n");
	rv = test_ctrl_get_version_support(m);
	if ( rv != 0 ) {
		printf("%s test_ctrl_version_support failed rv:%d\n", __FUNCTION__, rv);
		goto end;
	}

	// TEST 4: Get Message Type Support
	printf("-----------------------------------------------------------------\n");
	printf("TEST 4: Get Message Type Support\n");
	rv = test_ctrl_get_msg_type_support(m);
	if ( rv != 0 ) {
		printf("%s test_ctrl_get_msg_type_support failed rv:%d\n", __FUNCTION__, rv);
		goto end;
	}

	// TEST 5: Get Endpoint UUID
	printf("-----------------------------------------------------------------\n");
	printf("TEST 4: Get Endpoint UUID\n");
	rv = test_ctrl_get_endpoint_uuid(m);
	if ( rv != 0 ) {
		printf("%s test_ctrl_get_endpoint_uuid failed rv:%d\n", __FUNCTION__, rv);
		goto end;
	}

	// TEST 6: FMAPI - Identify Switch Device 
	printf("-----------------------------------------------------------------\n");
	printf("TEST 6: FMAPI - Identify Switch Device\n");
	rv = test_fmapi_identify_switch(m);
	if ( rv != 0 ) {
		printf("%s test_fmapi_identify_switch failed rv:%d\n", __FUNCTION__, rv);
		goto end;
	}

	sleep(20);
end:

	// Tell Threads to stop	
	pthread_mutex_lock(&m->mtx);
	{
		m->stop_threads = 2;
		pthread_cond_signal(&m->cond);
	}
	pthread_mutex_unlock(&m->mtx);

	printf("%s Ending \n", __FUNCTION__);

	return NULL;
}

/**
 * Test the ability to get the Enpoint UUDI
 *
 * Return 0 upon success, non-zero error condition otherwise
 */
int test_ctrl_get_endpoint_uuid(struct mctp *m)
{
	struct mctp_msg *mm;

	/* STEPS
	 * 1: Get an mctp_msg from the queue
	 * 2: Set MCTP Message Header 
	 * 3: Configure message type
	 * 4: Configure MCTP Control header
	 * 5: Configure command specific fields
	 * 6: Put message into send queue
	 * 7: Get response from the server
	 * 8: Print the received message 
	 * 9: Put the response message back into the recv queue 
	 */

	// STEP 1: Get an mctp_msg from the queue
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL)  {
		goto end;
	}

	// STEP 2: Set MCTP message header (DST, SRC, TO, TAG)
	mctp_fill_msg_hdr(mm, 0x02, 0x01, 1, 0);

	// STEP 3: Configure message type
	mm->type = MCMT_CONTROL;

	// STEP 4: Configure MCTP Control header (REQ, DATAGRAM, INST, CMD)
	mctp_fill_ctrl(mm, 1, 0, 0, MCCM_GET_ENDPOINT_UUID);

	mm->len = MCLN_TYPE + mctp_len_ctrl((__u8*)mctp_get_ctrl(mm));

	// STEP 5: Configure command specific fields

	// STEP 6: Put message into send queue
    pq_push(m->tmq, mm);

	printf("========== Waiting for response ==========\n");

	// STEP 7: Get response from the server
	mm = pq_pop(m->rmq, 1);
    if (mm == 0) {
         printf("%s pq_pop() returned an error\n", __FUNCTION__);
		 goto end;
	}

	// STEP 8: Print the received message 
	mctp_prnt_msg(mm);	

	// STEP 9: Put the response message back into the recv queue 
	pq_push(m->msgs, mm);

	return 0;

end:

	return 1;
}

/* 
 * Test the ability to set the EID of the remote endpoint
 *
 * Return 0 upon success, non-zero error condition otherwise
 */
int test_ctrl_set_eid(struct mctp *m)
{
	struct mctp_msg *mm;

	/* STEPS
	 * 1: Get an mctp_msg from the queue
	 * 2: Set MCTP Message Header 
	 * 3: Configure message type
	 * 4: Configure MCTP Control header
	 * 5: Configure command specific fields
	 * 6: Put message into send queue
	 * 7: Get response from the server
	 * 8: Print the received message 
	 * 9: Put the response message back into the recv queue 
	 */

	// STEP 1: Get an mctp_msg from the queue
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL)  {
		goto end;
	}

	// STEP 2: Set MCTP message header (DST, SRC, TO, TAG)
	mctp_fill_msg_hdr(mm, 0x02, 0x01, 1, 0);

	// STEP 3: Configure message type
	mm->type = MCMT_CONTROL;

	// STEP 4: Configure MCTP Control header (REQ, DATAGRAM, INST, CMD)
	mctp_fill_ctrl(mm, 1, 0, 0, MCCM_SET_ENDPOINT_ID);

	struct mctp_ctrl_msg *mc;

	// STEP 5: Configure command specific fields
	mc = (struct mctp_ctrl_msg*) &mm->payload[1];

	mctp_ctrl_fill_set_eid(mc, 0x02);

	mm->len = MCLN_TYPE + mctp_len_ctrl((__u8*)mctp_get_ctrl(mm));

	// STEP 6: Put message into send queue
    pq_push(m->tmq, mm);

	printf("========== Waiting for response ==========\n");

	// STEP 7: Get response from the server
	mm = pq_pop(m->rmq, 1);
    if (mm == 0) {
         printf("%s pq_pop() returned an error\n", __FUNCTION__);
		 goto end;
	}

	// STEP 8: Print the received message 
	mctp_prnt_msg(mm);	

	// STEP 9: Put the response message back into the recv queue 
	pq_push(m->msgs, mm);

	return 0;

end:

	return 1;
}

int test_ctrl_get_eid(struct mctp *m)
{
	struct mctp_msg *mm;

	/* STEPS
	 * 1: Get an mctp_msg from the queue
	 * 2: Set MCTP Message Header 
	 * 3: Configure message type
	 * 4: Configure MCTP Control header
	 * 5: Configure command specific fields
	 * 6: Put message into send queue
	 * 7: Get response from the server
	 * 8: Print the received message 
	 * 9: Put the response message back into the recv queue 
	 */

	// STEP 1: Get an mctp_msg from the queue
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL)  {
		goto end;
	}

	// STEP 2: Set MCTP message header (DST, SRC, TO, TAG)
	mctp_fill_msg_hdr(mm, 0x02, 0x01, 1, 0);

	// STEP 3: Configure message type
	mm->type = MCMT_CONTROL;

	// STEP 4: Configure MCTP Control header (REQ, DATAGRAM, INST, CMD)
	mctp_fill_ctrl(mm, 1, 0, 0, MCCM_GET_ENDPOINT_ID);

	// STEP 5: Configure command specific fields

	mm->len = MCLN_TYPE + mctp_len_ctrl((__u8*)mctp_get_ctrl(mm));

	// STEP 6: Put message into send queue
    pq_push(m->tmq, mm);

	printf("========== Waiting for response ==========\n");

	// STEP 7: Get response from the server
	mm = pq_pop(m->rmq, 1);
    if (mm == 0) {
         printf("%s pq_pop() returned an error\n", __FUNCTION__);
		 goto end;
	}

	// STEP 8: Print the received message 
	mctp_prnt_msg(mm);	

	// STEP 9: Put the response message back into the recv queue 
	pq_push(m->msgs, mm);

	return 0;
	
end:
	return 1;
}

int test_ctrl_get_msg_type_support(struct mctp *m)
{
	struct mctp_msg *mm;

	/* STEPS
	 * 1: Get an mctp_msg from the queue
	 * 2: Set MCTP Message Header 
	 * 3: Configure message type
	 * 4: Configure MCTP Control header
	 * 5: Configure command specific fields
	 * 6: Put message into send queue
	 * 7: Get response from the server
	 * 8: Print the received message 
	 * 9: Put the response message back into the recv queue 
	 */

	// STEP 1: Get an mctp_msg from the queue
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL)  {
		goto end;
	}

	// STEP 2: Set MCTP message header (DST, SRC, TO, TAG)
	mctp_fill_msg_hdr(mm, 0x02, 0x01, 1, 0);

	// STEP 3: Configure message type
	mm->type = MCMT_CONTROL;

	// STEP 4: Configure MCTP Control header (REQ, DATAGRAM, INST, CMD)
	mctp_fill_ctrl(mm, 1, 0, 0, MCCM_GET_MESSAGE_TYPE_SUPPORT);

	// STEP 5: Configure command specific fields

	mm->len = MCLN_TYPE + mctp_len_ctrl((__u8*)mctp_get_ctrl(mm));

	// STEP 6: Put message into send queue
    pq_push(m->tmq, mm);

	printf("========== Waiting for response ==========\n");

	// STEP 7: Get response from the server
	mm = pq_pop(m->rmq, 1);
    if (mm == 0) {
         printf("%s pq_pop() returned an error\n", __FUNCTION__);
		 goto end;
	}

	// STEP 8: Print the received message 
	mctp_prnt_msg(mm);	

	// STEP 9: Put the response message back into the recv queue 
	pq_push(m->msgs, mm);

	return 0;

end:

	return 1;
}

int test_ctrl_get_version_support(struct mctp *m)
{
	struct mctp_msg *mm;
	__u8 *data;

	/* STEPS
	 * 1: Get an mctp_msg from the queue
	 * 2: Set MCTP Message Header 
	 * 3: Configure message type
	 * 4: Configure MCTP Control header
	 * 5: Configure command specific fields
	 * 6: Put message into send queue
	 * 7: Get response from the server
	 * 8: Print the received message 
	 * 9: Put the response message back into the recv queue 
	 */

	// STEP 1: Get an mctp_msg from the queue
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL)  {
		goto end;
	}

	// STEP 2: Set MCTP message header (DST, SRC, TO, TAG)
	mctp_fill_msg_hdr(mm, 0x02, 0x01, 1, 0);

	// STEP 3: Configure message type
	mm->type = MCMT_CONTROL;

	// STEP 4: Configure MCTP Control header (REQ, DATAGRAM, INST, CMD)
	mctp_fill_ctrl(mm, 1, 0, 0, MCCM_GET_VERSION_SUPPORT);

	// STEP 5: Configure command specific fields
	data = mm->payload + sizeof(struct mctp_ctrl);
	data[0] = MCMT_BASE;

	mm->len = MCLN_TYPE + mctp_len_ctrl((__u8*)mctp_get_ctrl(mm));

	// STEP 6: Put message into send queue
    pq_push(m->tmq, mm);

	printf("========== Waiting for response ==========\n");

	// STEP 7: Get response from the server
	mm = pq_pop(m->rmq, 1);
    if (mm == 0) {
         printf("%s pq_pop() returned an error\n", __FUNCTION__);
		 goto end;
	}

	// STEP 8: Print the received message 
	printf("print message -------------------------------------\n");
	mctp_prnt_msg(mm);	

	// STEP 9: Put the response message back into the recv queue 
	pq_push(m->msgs, mm);

	return 0;
end:
	return 1; 
}

/**
 * Test: FMAPI - Identify Switch Device 
 *
 * STEPS
 * 1: Get an mctp_msg from the queue
 * 2: Set MCTP Message Header 
 * 3: Configure message type
 * 4: Configure MCTP Control header
 * 5: Configure command specific fields
 * 6: Serialize Request Cammand specific fields into message buffer
 * 7: Put message into send queue
 * 8: Get response from the server
 * 9: Print the received message 
 * 10: Put the response message back into the recv queue 
 */
int test_fmapi_identify_switch(struct mctp *m)
{
	struct fmapi_hdr fh;
	struct mctp_msg *mm;

	// STEP 1: Get an mctp_msg from the queue
	mm = pq_pop(m->msgs, 1);
	if (mm == NULL) 
		goto end;

	// STEP 2: Set MCTP message header (DST, SRC, TO, TAG)
	mctp_fill_msg_hdr(mm, 0x02, 0x01, 1, 0);

	// STEP 3: Configure message type
	mm->type = MCMT_CXLFMAPI;

	// STEP 4: Configure MCTP Control header (REQ, DATAGRAM, INST, CMD)
	fmapi_fill_hdr(&fh, FMMT_REQ, 0, FMOP_PSC_ID, 0, 0, 0, 0);

	// STEP : Serialize Request Cammand specific fields into message buffer
	fmapi_serialize(mm->payload, &fh, FMOB_HDR);
	
	// STEP 5: Configure command specific fields
	{

	}

	// STEP 6: Serialize Request Cammand specific fields into message buffer

	mm->len = FMLN_HDR;

	// STEP 7: Put message into send queue
    pq_push(m->tmq, mm);

	printf("========== Waiting for response ==========\n");

	// STEP 8: Get response from the server
	mm = pq_pop(m->rmq, 1);
    if (mm == 0) {
         printf("%s pq_pop() returned an error\n", __FUNCTION__);
		 goto end;
	}

	// STEP 9: Print the received message 
	printf("print message -------------------------------------\n");
	mctp_prnt_msg(mm);	

	// STEP : Print the FM API Object 
	{
		struct fmapi_psc_id_rsp id;
		fmapi_deserialize(&id, mm->payload + FMLN_HDR, FMOB_PSC_ID_RSP, NULL);
		fmapi_prnt(&id, FMOB_PSC_ID_RSP);        
	}

	// STEP 10: Put the response message back into the recv queue 
	pq_push(m->msgs, mm);

	return 0;
end:
	return 1; 

}
