/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file 		mctp_ctrl.c
 *
 * @brief 		Code file for MCTP Control Functions 
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jan 2024
 * @author 		Barrett Edwards <code@jrlabs.io>
 * 
 */

/* INCLUDES ==================================================================*/

#define _GNU_SOURCE

/* printf()
 */
#include <stdio.h>

/* memset()
 * memcpy()
 */
#include <string.h>

/* calloc()
 */
#include <stdlib.h>

/* errno
 */
#include <errno.h>

/* Needed for gettid()
 */
#define _GNU_SOURCE

/* gettid()
 */
#include <unistd.h>

//#include <sys/types.h>

/* __u8
 * __u32
 * __u64
 */
#include <linux/types.h>

/* be32toh()
 */
#include <endian.h>

/* autl_prnt_buf()
 */
#include <arrayutils.h>
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

/* 
 * String representation of Special Endpoint ID values (ID)
 *
 * See DSP0236 v1.3.1 Table 2.
 */
const char *STR_MCID[] = {
	"Null", 		// MCID_NULL	   	= 0,
	"Broadcast"		// MCID_BROADCAST 	= 0xff
};

/*
 * String representation of MCTP Control Completion Codes (CC)
 *
 * See DSP0236 v1.3.0 Table 13.
 */
const char *STR_MCCC[] = {
	"Success",		   	  		// MCCC_SUCCESS		   			= 0x00,
	"Error",	   		  		// MCCC_ERROR		   			= 0x01,
	"Error Invalid Data",  		// MCCC_ERROR_INVALID_DATA	   	= 0x02,
	"Error Invalid Length",		// MCCC_ERROR_INVALID_LENGTH  	= 0x03,
	"Error Not Ready",	  		// MCCC_ERROR_NOT_READY	   		= 0x04,
	"Error Unsupported CMD"		// MCCC_ERROR_UNSUPPORTED_CMD 	= 0x05
};

/*
 * MCTP Control Command IDs (CM)
 *
 * See DSP0236 v1.3.0 Table 12.
 */
const char *STR_MCCM[] = {
	"Reserved",			 				// MCCM_RESERVED			 		= 0x00,
	"Set Endpoint ID",		 			// MCCM_SET_ENDPOINT_ID		 		= 0x01,
	"Get Endpoint ID", 					// MCCM_GET_ENDPOINT_ID		 		= 0x02,
	"Get Endpoint UUID",	 			// MCCM_GET_ENDPOINT_UUID		 	= 0x03,
	"Get Version Support",	 			// MCCM_GET_VERSION_SUPPORT	 		= 0x04,
	"Get Message Type Support",			// MCCM_GET_MESSAGE_TYPE_SUPPORT	= 0x05,
	"Get Vendor Message Support", 		// MCCM_GET_VENDOR_MESSAGE_SUPPORT	= 0x06,
	"Resolve Endpoint ID",	 			// MCCM_RESOLVE_ENDPOINT_ID	 		= 0x07,
	"Allocate Endpoint IDs", 			// MCCM_ALLOCATE_ENDPOINT_IDS	 	= 0x08,
	"Routing Info Update",	 			// MCCM_ROUTING_INFO_UPDATE	 		= 0x09,
	"Get Routing Table Entries", 		// MCCM_GET_ROUTING_TABLE_ENTRIES 	= 0x0A,
	"Prepare Endpoint Discovery", 		// MCCM_PREPARE_ENDPOINT_DISCOVERY 	= 0x0B,
	"Endpoint Discover",  				// MCCM_ENDPOINT_DISCOVERY			= 0x0C,
	"Discovery Notify",					// MCCM_DISCOVERY_NOTIFY			= 0x0D,
	"Get Network ID",					// MCCM_GET_NETWORK_ID				= 0x0E,
	"Query hop",		 				// MCCM_QUERY_HOP			 		= 0x0F,
	"Resolve UUID",		 				// MCCM_RESOLVE_UUID		 		= 0x10,
	"Query Rate Limit",					// MCCM_QUERY_RATE_LIMIT			= 0x11,
	"Request RX Rate Limit",			// MCCM_REQUEST_TX_RATE_LIMIT		= 0x12,
	"Update Rate Limit",				// MCCM_UPDATE_RATE_LIMIT			= 0x13,
	"Query Supported Interfaces" 		// MCCM_QUERY_SUPPORTED_INTERFACES 	= 0x14,
};

/*
 * String representation of MCTP Control Set EID Operations (SE)
 *
 * DSP0236 1.3.1 Table 14 
 */
const char *STR_MCSE[] = {
	"Set",		// MCSE_SET 		= 0,
	"Force",	// MCSE_FORCE 		= 1,
	"Reset",	// MCSE_RESET 		= 2,
	"Discover"	// MCSE_DISCOVER 	= 3, 
};

/*
 * String representation of MCTP Control - Get Endpoint EID - Endpoint Typea (EP)
 *
 * DSP0236 1.3.1 Table 15
 */
const char *STR_MCEP[] = {
	"Endpoint",		// MCEP_SIMPLE_ENDPOINT	= 0,
	"Bridge"		// MCEP_BRIDGE			= 1,
};

/*
 * String representation of MCTP Control - Get Endpoint EID - Endpoint ID Type (IT)
 *
 * DSP0236 1.3.1 Table 15
 */
const char *STR_MCIT[] = {
	"Dynamic", 			// MCIT_DYNAMIC			= 0,
	"Static",			// MCIT_STATIC 			= 1,
	"Static Current",	// MCIT_STATIC_CURRENT		= 2,
	"Static Different"	// MCIT_STATIC_DIFFERENT	= 3
};

/* PROTOTYPES ================================================================*/

// Implemented 
static int set_eid 			(struct mctp *m, struct mctp_action *ma);
static int get_eid 			(struct mctp *m, struct mctp_action *ma);
static int get_ver_support 	(struct mctp *m, struct mctp_action *ma);
static int get_type_support (struct mctp *m, struct mctp_action *ma);
static int get_uuid			(struct mctp *m, struct mctp_action *ma);

// Unimplemented
int get_vendor_msg_type_support 	(struct mctp *m, struct mctp_action *ma);
int resolve_endpoint_id 			(struct mctp *m, struct mctp_action *ma);
int allocate_endpoint_ids 			(struct mctp *m, struct mctp_action *ma);
int routing_info_update 			(struct mctp *m, struct mctp_action *ma);
int get_routing_table_entries 		(struct mctp *m, struct mctp_action *ma);
int prepare_for_endpoint_discovery 	(struct mctp *m, struct mctp_action *ma);
int endpoint_discovery 				(struct mctp *m, struct mctp_action *ma);
int discovery_notify 				(struct mctp *m, struct mctp_action *ma);
int get_network_id 					(struct mctp *m, struct mctp_action *ma);
int query_hop 						(struct mctp *m, struct mctp_action *ma);
int resolve_uuid 					(struct mctp *m, struct mctp_action *ma);
int query_rate_limit 				(struct mctp *m, struct mctp_action *ma);
int request_tx_rate_limit			(struct mctp *m, struct mctp_action *ma);
int update_rate_limit 				(struct mctp *m, struct mctp_action *ma);
int query_supported_interfaces 		(struct mctp *m, struct mctp_action *ma);

/* FUNCTIONS =================================================================*/

/**
 * Handler of incoming MCTP Control messages
 *
 * @param[in/out] 	m 	Pointer to an mctp_state object
 * @param[in] 		ma 	Pointer to an mctp_action that is the request
 * @return 				0 upon success, 1 upon failure 
 *
 * STEPS
 * 1: Get Message body
 * 2: Verify request is from the tag owner, if not discard
 * 3: Verify request bit, if not a request, discard
 * 4: Verify EID 
 * 5: Handle each MCTP Control Command 
 */
int mctp_ctrl_handler(struct mctp *m, struct mctp_action *ma)
{
	INIT
	int rv;
	struct mctp_ctrl *mc;

	ENTER 

	// Initialize variables 
	rv = 1;

	STEP // 1: Get Message body
	mc = (struct mctp_ctrl*) ma->req->payload;

	STEP // 2: Verify request is from the tag owner, if not discard
	if ( ma->req->owner == 0) 
		goto end;
	 
	STEP // 3: Verify request bit, if not a request, discard
	if ( mc->req == 0 ) 
		goto end;

	STEP // 4: Verify EID 
	//If new req isn't a Broadcast and the EID has been set, and the new req EID doesn't match, discard
	if ( (ma->req->dst != MCID_NULL) && (ma->req->dst != MCID_BROADCAST) ) 
		if (ma->req->dst != m->state.eid) 
			goto end;
	
	STEP // 5: Handle each MCTP Control Command 
	switch (mc->cmd)
	{
		case MCCM_RESERVED:																break;	// 0x00
		case MCCM_SET_ENDPOINT_ID:				rv = set_eid(m, ma);					break;  // 0x01
		case MCCM_GET_ENDPOINT_ID: 				rv = get_eid(m, ma);					break;	// 0x02
		case MCCM_GET_ENDPOINT_UUID:			rv = get_uuid(m, ma); 					break;	// 0x03
		case MCCM_GET_VERSION_SUPPORT:			rv = get_ver_support(m, ma);			break;	// 0x04
		case MCCM_GET_MESSAGE_TYPE_SUPPORT:		rv = get_type_support(m, ma);			break;	// 0x05
		case MCCM_GET_VENDOR_MESSAGE_SUPPORT:											break;	// 0x06
		case MCCM_RESOLVE_ENDPOINT_ID:													break;	// 0x07
		case MCCM_ALLOCATE_ENDPOINT_IDS:												break;	// 0x08
		case MCCM_ROUTING_INFO_UPDATE:													break;	// 0x09
		case MCCM_GET_ROUTING_TABLE_ENTRIES:											break;	// 0x0A
		case MCCM_PREPARE_ENDPOINT_DISCOVERY:											break;	// 0x0B
		case MCCM_ENDPOINT_DISCOVERY:													break;	// 0x0C
		case MCCM_DISCOVERY_NOTIFY:														break;	// 0x0D
		case MCCM_GET_NETWORK_ID:														break;	// 0x0E
		case MCCM_QUERY_HOP:															break;	// 0x0F
		case MCCM_RESOLVE_UUID:															break;	// 0x10
		case MCCM_QUERY_RATE_LIMIT:														break;	// 0x11
		case MCCM_REQUEST_TX_RATE_LIMIT:												break;	// 0x12
		case MCCM_UPDATE_RATE_LIMIT:													break;	// 0x13
		case MCCM_QUERY_SUPPORTED_INTERFACES:											break;	// 0x14	
		default:								rv = 0;									break;
	}

end:

	EXIT(rv)

	return rv;
}


/** 
 * Prepare an MCTP Control Message - Get EID
 *
 * @param m		emapi_msg* to fill
 * @return 		0 upon success, non zero otherwise
 */
int mctp_ctrl_fill_get_eid(struct mctp_ctrl_msg *m)
{
	int rv;

	// Initialize variables
	rv = 1;

	// Validate Inputs 
	if (m == NULL)
		goto end;

	// Clear Header
	memset(&m->hdr, 0, sizeof(struct mctp_ctrl_msg));

	// Set header 
	m->hdr.cmd 		= MCCM_GET_ENDPOINT_ID;	

	// Set object
		
	rv = 0;

end:

	return rv;
}

/** 
 * Prepare an MCTP Control Message - Get Message Type Support
 *
 * @param m		emapi_msg* to fill
 * @return 		0 upon success, non zero otherwise
 */
int mctp_ctrl_fill_get_type(struct mctp_ctrl_msg *m)
{
	int rv;

	// Initialize variables
	rv = 1;

	// Validate Inputs 
	if (m == NULL)
		goto end;

	// Clear Header
	memset(&m->hdr, 0, sizeof(struct mctp_ctrl_msg));

	// Set header 
	m->hdr.cmd = MCCM_GET_MESSAGE_TYPE_SUPPORT;	

	// Set object
		
	rv = 0;

end:

	return rv;
}

/** 
 * Prepare an MCTP Control Message - Get Message Version Support
 *
 * @param m		emapi_msg* to fill
 * @return 		0 upon success, non zero otherwise
 */
int mctp_ctrl_fill_get_ver(struct mctp_ctrl_msg *m, int type)
{
	int rv;

	// Initialize variables
	rv = 1;

	// Validate Inputs 
	if (m == NULL)
		goto end;

	// Clear Header
	memset(&m->hdr, 0, sizeof(struct mctp_ctrl_msg));

	// Set header 
	m->hdr.cmd = MCCM_GET_VERSION_SUPPORT;	

	// Set object
	m->obj.get_ver_req.type = type;
		
	rv = 0;

end:

	return rv;
}

/** 
 * Prepare an MCTP Control Message - Get Endpoint UUID
 *
 * @param m		emapi_msg* to fill
 * @return 		0 upon success, non zero otherwise
 */
int mctp_ctrl_fill_get_uuid(struct mctp_ctrl_msg *m)
{
	int rv;

	// Initialize variables
	rv = 1;

	// Validate Inputs 
	if (m == NULL)
		goto end;

	// Clear Header
	memset(&m->hdr, 0, sizeof(struct mctp_ctrl_msg));

	// Set header 
	m->hdr.cmd = MCCM_GET_ENDPOINT_UUID;	

	// Set object
		
	rv = 0;

end:

	return rv;
}

/** 
 * Prepare an MCTP Control Message - Set Endpoint UUID
 *
 * @param m		emapi_msg* to fill
 * @return 		0 upon success, non zero otherwise
 */
int mctp_ctrl_fill_set_eid(struct mctp_ctrl_msg *m, int eid)
{
	int rv;

	// Initialize variables
	rv = 1;

	// Validate Inputs 
	if (m == NULL)
		goto end;

	// Clear Header
	memset(&m->hdr, 0, sizeof(struct mctp_ctrl_msg));

	// Set header 
	m->hdr.cmd = MCCM_SET_ENDPOINT_ID;	

	// Set object
	m->obj.set_eid_req.eid = eid;
		
	rv = 0;

end:

	return rv;
}

/**
 * Convenience function to fill MCTP Control object
 */
void mctp_fill_ctrl(
	struct mctp_msg *mm,
	__u8 req,
	__u8 datagram,
	__u8 inst,
	__u8 cmd)
{
	struct mctp_ctrl *mc;
	mc = mctp_get_ctrl(mm);
	mc->req = req;
	mc->datagram = datagram;
	mc->inst = inst;
	mc->cmd = cmd;
}

/**
 * Convenience Function to return a pointer to the MCTP Control object 
 */
struct mctp_ctrl *mctp_get_ctrl(struct mctp_msg *mm)
{
	return (struct mctp_ctrl*) mm->payload;
}

/**
 * Convenience function to get a pointer to the 4th byte of a MCTP Control Message 
 */
__u8 *mctp_get_ctrl_payload(struct mctp_msg *mm)
{
	return mm->payload + MCLN_CTRL;
}

/**
 * Perform MCTP Control - Get Endpoint ID Command
 *
 * @param m		struct mctp* 
 * @param ma 	struct mctp_action* inbound message to handle
 * @return 		0 upon success, 1 upon failure 
 *
 * STEPS
 * 1: Get response mctp_msg 
 * 2: Set payload pointers 
 * 3: Validate Inputs 
 * 4: Perform Action 
 * 5: Prepare Response Object
 * 6: Prepare Response Header
 * 7: Prepare MCTP Header 
 * 8: Submit message to Transmit Message Queue
 */
static int get_eid(struct mctp *m, struct mctp_action *ma)
{
	INIT
	struct mctp_ctrl_msg *req, *rsp;
	int rv;

	ENTER 

	// Initialize Variables 
	rv = 1;

	STEP // 1: Get response mctp_msg 
	ma->rsp = pq_pop(m->msgs, 1);
	if (ma->rsp == NULL)  
		goto end;

	STEP // 2: Set payload pointers 
	req = (struct mctp_ctrl_msg*) &ma->req->payload;
	rsp = (struct mctp_ctrl_msg*) &ma->rsp->payload;

	STEP // 3: Validate Inputs 

	STEP // 4: Perform Action 

	STEP // 5: Prepare Response Object
	rsp->obj.get_eid_rsp.comp_code 		= MCCC_SUCCESS;
	rsp->obj.get_eid_rsp.eid 			= m->state.eid;
	rsp->obj.get_eid_rsp.endpoint_type 	= MCEP_SIMPLE_ENDPOINT;
	rsp->obj.get_eid_rsp.id_type 		= MCIT_DYNAMIC;

	STEP // 6: Prepare Response Header
	memcpy(&rsp->hdr, &req->hdr, sizeof(struct mctp_ctrl));
	rsp->hdr.req = 0;

	STEP // 7 : Prepare MCTP Header 
	ma->rsp->dst 	= ma->req->src;
	ma->rsp->src 	= ma->req->dst;
	ma->rsp->type 	= ma->req->type;
	ma->rsp->len 	= MCLN_CTRL + MCLN_CTRL_GET_EID_RESP;

	STEP // 8: Submit message to Transmit Message Queue
	pq_push(m->tmq, ma);

	rv = 0;

end:

	EXIT(rv)

	return rv;
}

/**
 * Perform MCTP Control - Get Endpoint UUID Command
 *
 * @param m		struct mctp* 
 * @param ma 	struct mctp_action* inbound message to handle
 * @return 		0 upon success, 1 upon failure 
 *
 * STEPS
 * 1: Get response mctp_msg 
 * 2: Set payload pointers 
 * 3: Validate Inputs 
 * 4: Perform Action 
 * 5: Prepare Response Object
 * 6: Prepare Response Header
 * 7: Prepare MCTP Header 
 * 8: Submit message to Transmit Message Queue
 */
static int get_uuid(struct mctp *m, struct mctp_action *ma)
{
	INIT
	struct mctp_ctrl_msg *req, *rsp;
	int rv;

	ENTER 

	// Initialize variables
	rv = 1;

	STEP // 1: Get response mctp_msg
	ma->rsp = pq_pop(m->msgs, 1);
	if (ma->rsp == NULL)  
		goto end;

	STEP // 2: Set payload pointers 
	req = (struct mctp_ctrl_msg*) &ma->req->payload;
	rsp = (struct mctp_ctrl_msg*) &ma->rsp->payload;

	STEP // 3: Validate Inputs 

	STEP // 4: Perform Action 

	STEP // 5: Prepare Response Object
	rsp->obj.get_uuid_rsp.comp_code = MCCC_SUCCESS;
	memcpy(rsp->obj.get_uuid_rsp.uuid, m->state.uuid, MCLN_UUID);

	STEP // 6: Prepare Response Header
	memcpy(&rsp->hdr, &req->hdr, sizeof(struct mctp_ctrl));
	rsp->hdr.req = 0;

	STEP // 7 : Prepare MCTP Header 
	ma->rsp->dst 	= ma->req->src;
	ma->rsp->src 	= ma->req->dst;
	ma->rsp->type 	= ma->req->type;
	ma->rsp->len 	= MCLN_CTRL + MCLN_CTRL_GET_UUID_RESP;

	STEP // 7: Submit message to Transmit Message Queue
	pq_push(m->tmq, ma);

	rv = 0;

end:

	EXIT(rv)

	return rv;
}

/**
 * Perform MCTP Control - Get MCTP Message Type Support Command
 *
 * @param m		struct mctp* 
 * @param ma 	struct mctp_action* inbound message to handle
 * @return 		0 upon success, 1 upon failure 
 *
 * STEPS
 * 1: Get response mctp_msg 
 * 2: Set payload pointers 
 * 3: Validate Inputs 
 * 4: Perform Action 
 * 5: Prepare Response Object
 * 6: Prepare Response Header
 * 7: Prepare MCTP Header 
 * 8: Submit message to Transmit Message Queue
 */
static int get_type_support(struct mctp *m, struct mctp_action *ma)
{
	INIT
	struct mctp_ctrl_msg *req, *rsp;
	int rv;

	ENTER

	// Initialize Variables
	rv = 1;

	STEP // 1: Get response mctp_msg 
	ma->rsp = pq_pop(m->msgs, 1);
	if (ma->rsp == NULL)  
		goto end;

	STEP // 2: Set payload pointers 
	req = (struct mctp_ctrl_msg*) &ma->req->payload;
	rsp = (struct mctp_ctrl_msg*) &ma->rsp->payload;

	STEP // 3: Validate Inputs 

	STEP // 4: Perform Action 

	STEP // 5: Prepare Response Object
	rsp->obj.get_msg_type_rsp.comp_code = MCCC_SUCCESS;
	rsp->obj.get_msg_type_rsp.count 	= 2;
	rsp->obj.get_msg_type_rsp.list[0] 	= MCMT_CXLFMAPI;
	rsp->obj.get_msg_type_rsp.list[1] 	= MCMT_CXLCCI;

	STEP // 6: Prepare Response Header
	memcpy(&rsp->hdr, &req->hdr, sizeof(struct mctp_ctrl));
	rsp->hdr.req = 0;

	STEP // 7 : Prepare MCTP Header 
	ma->rsp->dst 	= ma->req->src;
	ma->rsp->src 	= ma->req->dst;
	ma->rsp->type 	= ma->req->type;
	ma->rsp->len 	= MCLN_CTRL + MCLN_CTRL_GET_MSG_TYPE_SUPPORT_RESP + rsp->obj.get_msg_type_rsp.count;

	STEP // 8: Submit message to Transmit Message Queue
	pq_push(m->tmq, ma);

	rv = 0;

end:

	EXIT(rv)

	return rv;
}

/** 
 * Perform MCTP Control - Get MCTP Version Support Command 
 *
 * @param m		struct mctp* 
 * @param ma 	struct mctp_action* inbound message to handle
 * @return 		0 upon success, 1 upon failure 
 *
 * STEPS
 * 1: Get response mctp_msg 
 * 2: Set payload pointers 
 * 3: Validate Inputs 
 * 4: Perform Action 
 * 5: Prepare Response Object
 * 6: Prepare Response Header
 * 7: Prepare MCTP Header 
 * 8: Submit message to Transmit Message Queue
 */
static int get_ver_support(struct mctp *m, struct mctp_action *ma)
{
	INIT 
	struct mctp_ctrl_msg *req, *rsp;
	struct mctp_version *head, *mv;
	int rv;

	int count;

	ENTER 

	// Initialize Variables
	rv = 1;
	count = 0;

	STEP // 1: Get response mctp_msg
	ma->rsp = pq_pop(m->msgs, 1);
	if (ma->rsp == NULL)  
		goto end;

	STEP // 2: Set payload pointers 
	req = (struct mctp_ctrl_msg*) &ma->req->payload;
	rsp = (struct mctp_ctrl_msg*) &ma->rsp->payload;

	STEP // 3: Validate Inputs 

	STEP // 4: Perform Action 

	STEP // 5: Prepare Response Object

	// Search linked list for entries of requested type 
	head = m->mctp_versions;
	while (head != NULL)
	{
		if (head->type < req->obj.get_ver_req.type)
		{
			head = head->next_type;
			continue;
		}
		else if (head->type == req->obj.get_ver_req.type)
		{
			mv = head;
			while (mv != NULL)
			{
				rsp->obj.get_ver_rsp.versions[count].major  = mv->major;
				rsp->obj.get_ver_rsp.versions[count].minor  = mv->minor;
				rsp->obj.get_ver_rsp.versions[count].update = mv->update;
				rsp->obj.get_ver_rsp.versions[count].alpha  = mv->alpha;
				count++;

				// Break if we have reached the maximum number of entries returnable in a 64B MTU
				if (count >= 14)
					break;

				mv = mv->next_entry;
			}
			break;
		}
		break;
	}

	// Populate response buffer from DSP0236 1.3.1 Table 18
	if (count > 0)
		rsp->obj.get_ver_rsp.comp_code = MCCC_SUCCESS;
	else 
		rsp->obj.get_ver_rsp.comp_code = 0x80;
	rsp->obj.get_ver_rsp.count = count;

	STEP // 6: Prepare Response Header
	memcpy(&rsp->hdr, &req->hdr, sizeof(struct mctp_ctrl));
	rsp->hdr.req = 0;

	STEP // 7 : Prepare MCTP Header 
	ma->rsp->dst 	= ma->req->src;
	ma->rsp->src 	= ma->req->dst;
	ma->rsp->type 	= ma->req->type;
	ma->rsp->len 	= MCLN_CTRL + MCLN_CTRL_GET_VER_SUPPORT_RESP + (count * 4);

	STEP // 8: Submit message to Transmit Message Queue
	pq_push(m->tmq, ma);

	rv = 0;

end:

	EXIT(rv)

	return rv;
}

/**
 * Perform MCTP Control - Set Endpoint ID Command
 *
 * @param m		struct mctp* 
 * @param ma 	struct mctp_action* inbound message to handle
 * @return 		0 upon success, 1 upon failure 
 * 
 * STEPS
 * 1: Get response mctp_msg 
 * 2: Set payload pointers 
 * 3: Validate Inputs 
 * 4: Perform Action 
 * 5: Prepare Response Object
 * 6: Prepare Response Header
 * 7: Prepare MCTP Header 
 * 8: Submit message to Transmit Message Queue
 */
static int set_eid(struct mctp *m, struct mctp_action *ma)
{
	INIT
	struct mctp_ctrl_msg *req, *rsp;
	int rv;

	ENTER

	// Initialize Variables
	rv = 1;

	STEP // 1: Get response mctp_msg
	ma->rsp = pq_pop(m->msgs, 1);
	if (ma->rsp == NULL)  
		goto end;

	STEP // 2: Set payload pointers 
	req = (struct mctp_ctrl_msg*) &ma->req->payload;
	rsp = (struct mctp_ctrl_msg*) &ma->rsp->payload;

	STEP // 3: Validate Inputs 

	// Reject unsupported Set EID opereations
	// This endpoint doesn't support static EIDs, so fail if they try and do a reset
	if (req->obj.set_eid_req.operation == MCSE_RESET) 
	{
		rsp->obj.set_eid_rsp.comp_code  = MCCC_ERROR_INVALID_DATA;	
		rsp->obj.set_eid_rsp.assignment = SET_EID_REJECTED; 
		rsp->obj.set_eid_rsp.eid 		= m->state.eid;
		goto fail; 
	}

	// This endpoint doesn't support discovery, so fail if requestor performs a MCSE_DISCOVER 
	if (req->obj.set_eid_req.operation == MCSE_DISCOVER) 
	{
		rsp->obj.set_eid_rsp.comp_code 	= MCCC_ERROR_INVALID_DATA;	
		rsp->obj.set_eid_rsp.assignment = SET_EID_REJECTED; 
		rsp->obj.set_eid_rsp.eid 		= m->state.eid;
		goto fail; 
	}

	// Reject invalid EIDs 
	if ( (req->obj.set_eid_req.eid == MCID_NULL) || (req->obj.set_eid_req.eid == MCID_BROADCAST) ) 
	{
		rsp->obj.set_eid_rsp.comp_code 	= MCCC_ERROR_INVALID_DATA;	
		rsp->obj.set_eid_rsp.assignment = SET_EID_REJECTED; 
		rsp->obj.set_eid_rsp.eid 		= m->state.eid;
		goto fail; 
	}

	STEP // 4: Perform Action 
	m->state.eid = req->obj.set_eid_req.eid; 	
	m->state.bus_owner_eid = ma->req->src;

	// Print the MCTP endpoint state
	if (m->verbose & MCTP_VERBOSE_STEPS)
		mctp_prnt_state(&m->state);

	STEP // 5: Prepare Response Object

	rsp->obj.set_eid_rsp.comp_code 	= MCCC_SUCCESS;
	rsp->obj.set_eid_rsp.assignment = SET_EID_ACCEPTED;
	rsp->obj.set_eid_rsp.allocation = 0;
	rsp->obj.set_eid_rsp.eid 		= m->state.eid;
	rsp->obj.set_eid_rsp.pool_size = 0;

	// Set the SRC EID in the MCTP Hdr to the new value
	ma->rsp->src = m->state.eid;
	
	STEP // 6: Prepare Response Header
	memcpy(&rsp->hdr, &req->hdr, sizeof(struct mctp_ctrl));
	rsp->hdr.req = 0;

	STEP // 7 : Prepare MCTP Header 
	ma->rsp->dst 	= ma->req->src;
	ma->rsp->src 	= ma->req->dst;
	ma->rsp->type 	= ma->req->type;
	ma->rsp->len 	= MCLN_CTRL + MCLN_CTRL_SET_EID_RESP;

	STEP // 8: Submit message to Transmit Message Queue
	pq_push(m->tmq, ma);

	rv = 0;

end:

	EXIT(rv)

	return rv;

fail:

	ma->completion_code = 1;
	mctp_retire(m, ma);

	EXIT(rv)

	return rv;
}

/**
 * Determine length in bytes of MCTP Control Message
 *
 * @param ptr unsigned char pointer to a buffer. The first item in the buffer 
 *            is expected to be a mctp_ctrl struct followed by the command
 *            payload
 * @return 	  The length of the mctp_control + data in bytes
 */
unsigned int mctp_len_ctrl(__u8 *ptr)
{
	unsigned len;
	struct mctp_ctrl *mc;
	__u8 *data;

	len = 0;
	mc = (struct mctp_ctrl*) ptr;
	data = ptr + sizeof(struct mctp_ctrl);

	switch(mc->cmd)
	{
		case MCCM_RESERVED:			goto end; 			// 0x00
		case MCCM_SET_ENDPOINT_ID:		 				// 0x01
		{
			if (mc->req) 	len = MCLN_CTRL_SET_EID_REQ;
			else 			len = MCLN_CTRL_SET_EID_RESP;
		}
			break;

		case MCCM_GET_ENDPOINT_ID:		 				// 0x02
		{
			if (mc->req) 	len = MCLN_CTRL_GET_EID_REQ;
			else 			len = MCLN_CTRL_GET_EID_RESP;
		}
			break;

		case MCCM_GET_ENDPOINT_UUID:		 			// 0x03
		{
			if (mc->req) 	len = MCLN_CTRL_GET_UUID_REQ;
			else 			len = MCLN_CTRL_GET_UUID_RESP;
		}
			break;

		case MCCM_GET_VERSION_SUPPORT:	 				// 0x04
		{
			if (mc->req) 	
				len = MCLN_CTRL_GET_VER_SUPPORT_REQ;
			else {
				// Get the Version Number Entry Count from byte 2 of the data
				len = MCLN_CTRL_GET_VER_SUPPORT_RESP + data[1] * 4;
			}
		}
			break;

		case MCCM_GET_MESSAGE_TYPE_SUPPORT:				// 0x05
		{
			if (mc->req) 	
				len = MCLN_CTRL_GET_MSG_TYPE_SUPPORT_REQ;
			else { 
				// Get the Message Type Count from byte 2 of the data
				len = MCLN_CTRL_GET_MSG_TYPE_SUPPORT_RESP + data[1] * 1;
			}
		}
			break;

		case MCCM_GET_VENDOR_MESSAGE_SUPPORT:	goto end; 	// 0x06
		case MCCM_RESOLVE_ENDPOINT_ID:	 		goto end; 	// 0x07
		case MCCM_ALLOCATE_ENDPOINT_IDS:		goto end; 	// 0x08
		case MCCM_ROUTING_INFO_UPDATE:	 		goto end; 	// 0x09
		case MCCM_GET_ROUTING_TABLE_ENTRIES: 	goto end; 	// 0x0A
		case MCCM_PREPARE_ENDPOINT_DISCOVERY: 	goto end; 	// 0x0B
		case MCCM_ENDPOINT_DISCOVERY:			goto end; 	// 0x0C
		case MCCM_DISCOVERY_NOTIFY:				goto end; 	// 0x0D
		case MCCM_GET_NETWORK_ID:				goto end; 	// 0x0E
		case MCCM_QUERY_HOP:			 		goto end; 	// 0x0F
		case MCCM_RESOLVE_UUID:		 			goto end; 	// 0x10
		case MCCM_QUERY_RATE_LIMIT:				goto end; 	// 0x11
		case MCCM_REQUEST_TX_RATE_LIMIT:		goto end; 	// 0x12
		case MCCM_UPDATE_RATE_LIMIT:			goto end; 	// 0x13
		case MCCM_QUERY_SUPPORTED_INTERFACES: 	goto end; 	// 0x14
		default:                				goto end; 
	}

	// Add the length of the MCTP Control Message Header 
	len += MCLN_CTRL;

	return len;

end:
	return 0;
}

/**
 * BCD Digit Compare 
 * @return 	-1 lhs comes before rhs
 *  		 0 lhs == rhs 
 * 			+1 lhs comes after rhs 
 */
int dgtcmp(__u8 lhs, __u8 rhs)
{
	if (lhs == rhs)
		return 0;
	if (lhs == 0x0F && rhs != 0x0F)
		return -1;
	if (rhs == 0x0F && lhs != 0x0F)
		return 1;
	if (lhs < rhs)
		return -1;
	return 1;
}

/**
 * MCTP Version Compare 
 * @return 	-1 lhs comes before rhs
 *  		 0 lhs == rhs 
 * 			+1 lhs comes after rhs 
 */
int vercmp(struct mctp_version *lhs, struct mctp_version *rhs)
{
	// Compare upper digit of Major
	switch( dgtcmp(lhs->major >> 4, rhs->major >> 4) )
	{
		case -1: return -1;
		case  1: return  1;
	}
	// Compare lower digit of Major
	switch( dgtcmp(lhs->major & 0x0F, rhs->major & 0x0F) )
	{
		case -1: return -1;
		case  1: return  1;
	}

	// Compare upper digit of Minor 
	switch( dgtcmp(lhs->minor >> 4, rhs->minor >> 4) )
	{
		case -1: return -1;
		case  1: return  1;
	}
	// Compare lower digit of Minor
	switch( dgtcmp(lhs->minor & 0x0F, rhs->minor & 0x0F) )
	{
		case -1: return -1;
		case  1: return  1;
	}

	// Compare upper digit of Update
	switch( dgtcmp(lhs->update >> 4, rhs->update >> 4) )
	{
		case -1: return -1;
		case  1: return  1;
	}
	// Compare lower digit of Update
	switch( dgtcmp(lhs->update & 0x0F, rhs->update & 0x0F) )
	{
		case -1: return -1;
		case  1: return  1;
	}

	return dgtcmp(lhs->alpha, rhs->alpha);
}

/**
 * SPrint the MCTP version into a character buffer
 * @param buf 	char* to a buffer of at least 11 length (includes null terminator)
 * @poaram mv	struct mctp_version*
 * @return 		The number of characters printed - not including the null terminator
 */
int mctp_sprnt_ver(char *buf, struct mctp_version *mv)
{
	int i;
	
	i = 0;

	// Major Upper Digit 
	if ((mv->major & 0xF0) != 0xF0)
		i += sprintf(&buf[i], "%d", (mv->major >> 4) & 0x0F);

	// Major Lower Digit
	i += sprintf(&buf[i], "%d.", (mv->major & 0x0F));

	// Minor Upper Digit
	if ((mv->minor & 0xF0) != 0xF0)
		i += sprintf(&buf[i], "%d", (mv->minor >> 4) & 0x0F);
		
	// Minor Lower
	i += sprintf(&buf[i], "%d", (mv->minor & 0x0F));

	// Don't print anything for the update if it is 0xFF
	if (mv->update != 0xFF)
	{
		i += sprintf(&buf[i], ".");

		// Update Upper Digit
		if ((mv->update & 0xF0) != 0xF0)
			i += sprintf(&buf[i], "%d", (mv->update >> 4) & 0x0F);

		// Update Lower Digit
		i += sprintf(&buf[i], "%d", (mv->update & 0x0F));
	}

	// Don't print anything for the alpha if it is 0x00
	if (mv->alpha != 0)
		i += sprintf(&buf[i], "%c", mv->alpha);

	buf[i] = 0;

	return i;
}

/**
 * Print a struct mctp_version 
 *
 *Format: "type: major.minor.update.alpha
 */
void mctp_prnt_ver(struct mctp_version *mv, int indent)
{
	char space[] = "                                                        ";
	char buf[11];
	space[indent] = 0;

	mctp_sprnt_ver(buf, mv);

	printf("%s0x%02x: %s\n", space, mv->type, buf);
}

/**
 * Print the linked list array of mctp_verion objects 
 */
void mctp_prnt_vers(struct mctp_version *mv)
{
	struct mctp_version *head, *curr;

	head = mv;

	while (head != NULL)
	{
		mctp_prnt_ver(head,0);

		curr = head->next_entry;
		while (curr != NULL)
		{
			mctp_prnt_ver(curr,4);
			curr = curr->next_entry;
		}
		head = head->next_type;
	}
}

/**
 * Add an entry to the list of supported MCTP Message versions
 */
int mctp_set_version(struct mctp *m, __u8 type, __u8 major, __u8 minor, __u8 update, __u8 alpha)
{
	struct mctp_version *new, *curr, *prev;
	int rv; 

	// Initialize variables
	rv = 1;

	// Allocate and clear memory for new struct 
	new = calloc (1, sizeof(struct mctp_version));
	if (new == NULL) 
		goto end;

	// Store new values in new struct
	new->type = type;
	new->major = major;
	new->minor = minor;
	new->update = update;
	new->alpha = alpha;

	// Determine if this is the first version to be created
	// If there is an existing linked list, insert at the head of the list 
	if (m->mctp_versions == NULL)
	{
		m->mctp_versions = new;
		goto end;
	}

	// If the new version is less than the first types entry, set it to the first 
	prev = NULL;
	curr = m->mctp_versions;
	if (new->type < curr->type) 
	{
		new->next_type = curr;
		m->mctp_versions = new;			
		goto end;
	}

	if (new->type == curr->type)
	{
		if (vercmp(new, curr) < 0)
		{
			new->next_type = curr->next_type;
			new->next_entry = curr;
			m->mctp_versions = new;
			goto end;
		}
		if (vercmp(new, curr) == 0)
		{
			goto end;
		}
	}

	// Find head of type list 
	while ( curr != NULL )
	{
		if (new->type < curr->type)
		{
			new->next_type = curr;
			prev->next_type = new;			
			goto end;
		}
		else if (new->type == curr->type)
		{
			// if new version less than curr version, insert ahead of it in the sub array
			if (vercmp(new, curr) < 0)
			{
				prev->next_type = new;
				new->next_type = curr->next_type;
				new->next_entry = curr;
				curr->next_type = NULL;
				goto end;
			}

			// Drop new if version equal to head of sub list
			if (vercmp(new, curr) == 0)
			{
				goto end;
			}

			prev = curr;
			curr = curr->next_entry;

			while (1)
			{		
				// If we have reached the end of the list, append 
				if (curr == NULL)
				{
					prev->next_entry = new;
					goto end;
				}

				if (vercmp(new, curr) < 0)
				{
					prev->next_entry = new;
					new->next_entry = curr;
					goto end;
				}

				// Drop new if version equal to curr of sub list
				if (vercmp(new, curr) == 0)
				{
					goto end;
				}
				
				prev = curr;
				curr = curr->next_entry;
			}
		}
		else if (curr->next_type == NULL)
		{
			curr->next_type = new;
			goto end;
		}

		prev = curr;
		curr = curr->next_type;
	}

	rv = 0;

end:

	return rv;
}

/* Functions to return a string representation of an object */
const char *mccc(unsigned u)
{
	if (u >= MCCC_MAX)
		return NULL;
	return STR_MCCC[u];
}

const char *mccm(unsigned u)
{
	if (u >= MCCM_MAX)
		return NULL;
	return STR_MCCM[u];
}                                               

const char *mcep(unsigned u)
{
	if (u >= MCEP_MAX)
		return NULL;
	return STR_MCEP[u];
}

const char *mcid(unsigned u)
{
	int rv;
	switch(u)
	{
		case MCID_NULL:	   		rv = 0;	break; // 0x00
		case MCID_BROADCAST: 	rv = 1; break; // 0xff
		default: 				return NULL;
	}
	return STR_MCID[rv];
}

const char *mcit(unsigned u)
{
	if (u >= MCIT_MAX)
		return NULL;
	return STR_MCIT[u];
}

const char *mcse(unsigned u)
{
	if (u >= MCSE_MAX)
		return NULL;
	return STR_MCSE[u];
}

