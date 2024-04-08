/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file 		server.c
 *
 * @brief 		Code file to implement server example of  MCTP transport library
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jan 2024
 * @author 		Barrett Edwards <code@jrlabs.io>
 * 
 */

/* INCLUDES ==================================================================*/

/* exit()
 */
#include <stdlib.h>

/* printf()
 */
#include <stdio.h>

/* memset()
 */
#include <string.h>

/* errno
 */
#include <errno.h>

/* __u8
 * __u32
 * __u64
 */
#include <linux/types.h>
#include <ptrqueue.h>

#include <fmapi.h>

#include "main.h"

/* MACROS ====================================================================*/

#define MCTP_PORT 2508

//#define MCTP_VERBOSE
#ifdef MCTP_VERBOSE
#define VERBOSE(v, m, t)             ({ if(v) printf("%d:%s %s\n",    t, __FUNCTION__, m   ); })
#define VERBOSE_INT(v, m, t, i)      ({ if(v) printf("%d:%s %s %d\n", t, __FUNCTION__, m, i); })
#define VERBOSE_STR(v, m, t, s)      ({ if(v) printf("%d:%s %s %s\n", t, __FUNCTION__, m, s); })
#else
#define VERBOSE(v, m, t)
#define VERBOSE_INT(v, m, i)
#define VERBOSE_STR(v, m, s)
#endif

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/* GLOBAL VARIABLES ==========================================================*/

/* PROTOTYPES ================================================================*/

int fmapi_handler(struct mctp *, struct mctp_action *ma);
int fmop_identify_switch_device(struct mctp_state *state, struct mctp_msg *req, struct mctp_msg *resp);

/* FUNCTIONS =================================================================*/

int main()
{
	struct mctp *m;

	// Create the threads object
	m = mctp_init();
	if (m == NULL) {	
		goto end;
	}

	// Set Message handler functions
	mctp_set_handler(m, MCMT_CXLFMAPI, fmapi_handler);

	// Set verbosity levels
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_ERROR);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_THREADS);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_STEPS);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_PACKET);
	mctp_set_verbosity(m, mctp_get_verbosity(m) | MCTP_VERBOSE_MESSAGE);

	// Run the threads
	mctp_run(m, MCTP_PORT, 0, MCRM_SERVER, 1, 1);


	printf("Main thread sleeping ###########################3\n");
	sleep(10);
	printf("Main thread calling stop ###########################3\n");
	mctp_stop(m);
	printf("Main thread calling free ###########################3\n");

	// Free memory
	mctp_free(m);

	return 0;

end: 
	return 1;
}


/**
 * 
 * @return 	0 indicates that the response message should NOT be sent 
 *			1 indicates that the response message should be se sent
 * STEPS 
 * 1: Verify type of message is CXL FMAPI 
 * 2: Deserialize buffer into local Request FM API Header object 
 * 3: Verify FM API Message Category
 * 4: Fill Response MCTP Transport Header: dst, src, owner, tag, type 
 * 5: Handle Opcode
 * 6: Handle simple response case 
 */
int fmapi_handler(struct mctp *m, struct mctp_action *ma)
{
	struct mctp_msg *mr, *mm;
	struct fmapi_hdr req_fh, resp_fh; 
	int rv, rc, ret;
	unsigned long len;

	VERBOSE(1, "", 0);

	// Initialize variables 
	len = 0;
	ret = 0;
	rc = FMRC_UNSUPPORTED;	
	mm = ma->req;

	// : Get mctp_msg buffer for the response
	mr = pq_pop(m->msgs, 1);
	if (mr == NULL)  
		goto end;

	// STEP 1: Verify type of message is CXL FMAPI
	if ( mm->type != MCMT_CXLFMAPI )
		goto end;

	// STEP 2: Deserialize buffer into local Request FM API Header object 
	rv = fmapi_deserialize(&req_fh, mm->payload, FMOB_HDR, NULL);
	if (rv == 0)
		goto end;

	// STEP 3: Verify FM API Message Category
	if (req_fh.category != FMMT_REQ)
		goto end;

	// STEP 4: Fill Response MCTP Transport Header: dst, src, owner, tag, type 
	mctp_fill_msg_hdr(mr, mm->src, mm->dst, 0, mm->tag);
	mr->type = mm->type;
	
	// STEP 5: Handle Opcode
	switch(req_fh.opcode)
	{
		case FMOP_PSC_ID: 				     					// 0x5100
			ret = fmop_identify_switch_device(&m->state, mm, mr);
			goto end;

		default: 
			len = 0;
			ret = 1;
			rc = FMRC_UNSUPPORTED;	
			goto send;
	}

send:
	// STEP 6: Handle simple response case 

	// Fill Response FM API HDR 
	mr->len = fmapi_fill_hdr(&resp_fh, FMMT_RESP, req_fh.tag, req_fh.opcode, 0, len, rc, 0);

	// Serialize response fmapi_hdr into response message data buffer 
	rv = fmapi_serialize(mr->payload, &resp_fh, FMOB_HDR);
	if (rv == 0)
		ret = 0;

	ma->rsp = mr;

	pq_push(m->tmq, ma);

end:
	return ret ;
}


/**
 * Handle the FM API Opcode: Identify Switch Device (Opcode 5100h)
 *
 * @return 	1 to send response message, 0 to not send a response
 * 
 * STEPS:
 * 1: Deserialize buffer into local Request FM API Header object 
 * 2: Deserialize requset buffer into local object (if needed)
 * 3: Obtain lock on switch state 
 * 4: Populate response object with data
 * 5: Release lock on switch state
 * 6: Compute FM API Payload Length
 * 7: Fill Response FM API HDR 
 * 8: Serialize response fmapi_hdr into response message data buffer 
 * 9: Fill in opcode specifc response data 
 */
int fmop_identify_switch_device(struct mctp_state *state, struct mctp_msg *req, struct mctp_msg *resp)
{
	struct fmapi_hdr req_fh, resp_fh; 
	int rv, rc;
	unsigned long len;
	struct fmapi_psc_id_rsp id;

	len = 0;
	rc = FMRC_SUCCESS;	
	state->verbose = state->verbose;
	
	// STEP 1: Deserialize buffer into local Request FM API Header object 
	rv = fmapi_deserialize(&req_fh, req->payload, FMOB_HDR, NULL);
	if (rv == 0)
		goto end;

	// STEP 2: Deserialize requset buffer into local object (if needed)
	if (rv == 0)
		goto end;

	// STEP 3: Obtain lock on switch state 
	// TBD

	// STEP 4: Populate response object with data
	memset(&id, 0, sizeof(struct fmapi_psc_id_rsp));
	id.ingress_port 	= 1;					//!< Ingress Port ID 
	id.num_ports 		= 32;					//!< Total number of physical ports
	id.num_vcss 		= 16; 					//!< Max number of VCSs
	id.active_ports[0] 	= 0xFF;					//!< Active physical port bitmask: enabled (1), disabled (0)
	id.active_ports[1] 	= 0xFF;					//!< Active physical port bitmask: enabled (1), disabled (0)
	id.active_ports[2] 	= 0xFF;					//!< Active physical port bitmask: enabled (1), disabled (0)
	id.active_ports[3] 	= 0xFF;					//!< Active physical port bitmask: enabled (1), disabled (0)
	id.active_vcss[0] 	= 0xFF;					//!< Active vcs bitmask: enabled (1), disabled (0)
	id.active_vcss[1] 	= 0xFF;					//!< Active vcs bitmask: enabled (1), disabled (0)
	id.num_vppbs 		= 32;					//!< Max number of vPPBs 
	id.active_vppbs 	= 32;					//!< Number of active vPPBs 
	id.num_decoders 	= 1;					//!< Number of HDM decoders available per USP 

	// STEP 5: Release lock on switch state 

	// STEP 6: Compute FM API Payload Length
	len = FMLN_PSC_IDENTIFY_SWITCH;

	// STEP 7: Fill Response FM API HDR 
	fmapi_fill_hdr(&resp_fh, FMMT_RESP, req_fh.tag, req_fh.opcode, 0, len, rc, 0);

	// STEP 8: Serialize response fmapi_hdr into response message data buffer 
	rv = fmapi_serialize(resp->payload, &resp_fh, FMOB_HDR);
	if (rv == 0)
		goto end;

	// STEP 9: Fill in opcode specifc response data 
	rv = fmapi_serialize(resp->payload + FMLN_HDR, &id , FMOB_PSC_ID_RSP);
	if (rv == 0)
		goto end;

	resp->len = FMLN_HDR + FMLN_PSC_IDENTIFY_SWITCH;

	return 1;

end:
				// 1 implies to send this response message back to requestor 
	return 0;   // 0 implies to not send a response back to the requestor
}
 
