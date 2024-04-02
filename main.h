/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file 		mctp.h 
 *
 * @brief 		Header file for MCTP transport library
 *
 * @details 	As per MCTP specification, all MCTP fields are Big Endian
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jan 2024
 * @author 		Barrett Edwards <code@jrlabs.io>
 *
 * Macro / Enumeration Prefixes 
 * MCCC - MCTP Control Completion Codes (CC)
 * MCCM - MCTP Control Command IDs (CM) 
 * MCEP - MCTP Control - Get Endpoint EID - Endpoint Typea (EP)
 * MCID - Special Endpoint ID values (ID)
 * MCIT - MCTP Control - Get Endpoint EID - Endpoint ID Type (IT)
 * MCMT - MCTP Message Type Codes (MT)
 * MCRM - Run Mode for the MCTP Threads (RM)
 * MCSE - MCTP Control Set EID Operations (SE)
 * MCLN - Message Data Lengths for MCTP Control Messages (LN)
 * 
 */
#ifndef _MCTP_H
#define _MCTP_H

/* INCLUDES ==================================================================*/

/* __u8
 * __u32
 */
#include <linux/types.h>

/* useconds_t
 */
#include <unistd.h>

/* struct sockaddr_in 
 */
#include <netinet/in.h>

/* uuid_t
 */
#include <uuid/uuid.h>

/* timepsec
 */
#include <time.h>

/* sem_t
 */
#include <semaphore.h>

/* MACROS ====================================================================*/

// Serialized length of MCTP Header
#define MCLN_HDR 						4
// Serialized length of MCTP BTU
#define MCLN_BTU 						64
// Serialized length of MCTP Packet
#define MCLN_PKT 						(MCLN_HDR + MCLN_BTU)
// Serialized length of MCTP Type
#define MCLN_TYPE						1
// Serialized length of MCTP Control UUID 
#define MCLN_UUID    					16

#define MCLN_MSG_PAYLOAD 				8192
#define MCLN_MSG 						(MCLN_HDR + MCLN_TYPE + MCLN_MSG_PAYLOAD)

#define MCTP_MAX_INPROCESS_MESSAGES 	8
#define MCTP_MAX_PACKET_NUM 			1024
#define MCTP_MAX_MESSAGE_NUM 			16

#define MCTP_NUM_TAGS  					8

#define MCTP_RPQ_SIZE 					1024
#define MCTP_TPQ_SIZE 					1024
#define MCTP_RMQ_SIZE 					128
#define MCTP_TMQ_SIZE 					128
#define MCTP_TAQ_SIZE 					128
#define MCTP_ACQ_SIZE 					128

#define MCTP_PKT_POOL_SIZE 				1024
#define MCTP_MSG_POOL_SIZE 				128
#define MCTP_ACTION_POOL_SIZE 			128
#define MCTP_ACTION_DEFAULT_RETRY_NUM	8

// Verbose bit fields
#define MCTP_VERBOSE_ERROR 				(0x01 << 0)
#define MCTP_VERBOSE_THREADS 			(0x01 << 1)
#define MCTP_VERBOSE_STEPS 				(0x01 << 2)
#define MCTP_VERBOSE_PACKET 			(0x01 << 3)
#define MCTP_VERBOSE_MESSAGE			(0x01 << 4)

/* Action Macros */
#define MCTP_ACTION_DELTA_SEC 			0
#define MCTP_ACTION_DELTA_NSEC 			100000000

/* Threads Macros */
#define MCTP_THREAD_ERROR_USLEEP 		1000
#define MCTP_THREAD_SUBMIT_NSLEEP 		1000000

/* MCTP Control Macros */
#define SET_EID_ACCEPTED 				0
#define SET_EID_REJECTED 				1

// Length of MCTP_VERSIONS[] array 
#define MCTP_VERSIONS_NUM				4

/**
 * Message Data Lengths for MCTP Control Messages (LN)
 * The Response lengths include the 1 byte completion code length
 */
#define MCLN_CTRL 								2
#define MCLN_CTRL_SET_EID_REQ 					2
#define MCLN_CTRL_SET_EID_RESP 					4
#define MCLN_CTRL_GET_EID_REQ 					0
#define MCLN_CTRL_GET_EID_RESP 					4
#define MCLN_CTRL_GET_UUID_REQ 					0
#define MCLN_CTRL_GET_UUID_RESP 				17	
#define MCLN_CTRL_GET_VER_SUPPORT_REQ 			1
#define MCLN_CTRL_GET_VER_SUPPORT_RESP 			2	
#define MCLN_CTRL_GET_MSG_TYPE_SUPPORT_REQ 		0
#define MCLN_CTRL_GET_MSG_TYPE_SUPPORT_RESP 	2	

/* ENUMERATIONS ==============================================================*/

/* 
 * MCTP Message Type Codes (MT)
 *
 * See DSP0239 v1.9.0 Table 1.
 */
enum _MCMT 
{
	MCMT_CONTROL						= 0x00,
	MCMT_PLDM 							= 0x01,
	MCMT_NCSI 							= 0x02,
	MCMT_ETHERNET						= 0x03,
	MCMT_NVMEMI							= 0x04,
	MCMT_SPDM 							= 0x05,
	MCMT_SECURE							= 0x06,
	MCMT_CXLFMAPI						= 0x07,
	MCMT_CXLCCI							= 0x08,
	MCMT_CSE 						 	= 0x70,
	MCMT_VDM_PCI						= 0x7E,
	MCMT_VDM_IANA						= 0x7F,
	MCMT_MAX							= 0xFF
};
#define MCMT_BASE 0xFF

/**
 * MCTP Threads Run Mode (RM)
 */
enum _MCRM 
{
	MCRM_SERVER	   	= 0,
	MCRM_CLIENT 	= 1,
	MCRM_MAX
};

/*
 * MCTP Control Completion Codes (CC)
 *
 * See DSP0236 v1.3.0 Table 13.
 */
enum _MCCC 
{
	MCCC_SUCCESS		   				= 0x00,
	MCCC_ERROR		   					= 0x01,
	MCCC_ERROR_INVALID_DATA	   			= 0x02,
	MCCC_ERROR_INVALID_LENGTH  			= 0x03,
	MCCC_ERROR_NOT_READY	   			= 0x04,
	MCCC_ERROR_UNSUPPORTED_CMD 			= 0x05,
	MCCC_MAX							= 0x06
	/* 0x80 - 0xFF are command specific */
};

/*
 * MCTP Control Command IDs (CM)
 *
 * See DSP0236 v1.3.0 Table 12.
 */
enum _MCCM 
{
	MCCM_RESERVED			 			= 0x00,
	MCCM_SET_ENDPOINT_ID		 		= 0x01,
	MCCM_GET_ENDPOINT_ID		 		= 0x02,
	MCCM_GET_ENDPOINT_UUID		 		= 0x03,
	MCCM_GET_VERSION_SUPPORT	 		= 0x04,
	MCCM_GET_MESSAGE_TYPE_SUPPORT		= 0x05,
	MCCM_GET_VENDOR_MESSAGE_SUPPORT		= 0x06,
	MCCM_RESOLVE_ENDPOINT_ID	 		= 0x07,
	MCCM_ALLOCATE_ENDPOINT_IDS	 		= 0x08,
	MCCM_ROUTING_INFO_UPDATE	 		= 0x09,
	MCCM_GET_ROUTING_TABLE_ENTRIES	 	= 0x0A,
	MCCM_PREPARE_ENDPOINT_DISCOVERY 	= 0x0B,
	MCCM_ENDPOINT_DISCOVERY				= 0x0C,
	MCCM_DISCOVERY_NOTIFY				= 0x0D,
	MCCM_GET_NETWORK_ID					= 0x0E,
	MCCM_QUERY_HOP			 			= 0x0F,
	MCCM_RESOLVE_UUID		 			= 0x10,
	MCCM_QUERY_RATE_LIMIT				= 0x11,
	MCCM_REQUEST_TX_RATE_LIMIT			= 0x12,
	MCCM_UPDATE_RATE_LIMIT				= 0x13,
	MCCM_QUERY_SUPPORTED_INTERFACES 	= 0x14,
	MCCM_MAX			 				= 0x15
	/* 0xF0 - 0xFF are transport specific */
};

/*
 * MCTP Control - Get Endpoint EID - Endpoint Type (EP)
 *
 * DSP0236 1.3.1 Table 15
 */
enum _MCEP 
{
	MCEP_SIMPLE_ENDPOINT	= 0,
	MCEP_BRIDGE				= 1,
	MCEP_MAX				
};

/** 
 * Special Endpoint ID values (ID)
 *
 * See DSP0236 v1.3.1 Table 2.
 */
enum _MCID 
{
	MCID_NULL	   	= 0,
	MCID_BROADCAST 	= 0xff
};

/*
 * MCTP Control - Get Endpoint EID - Endpoint ID Type (IT)
 *
 * DSP0236 1.3.1 Table 15
 */
enum _MCIT 
{
	MCIT_DYNAMIC			= 0,
	MCIT_STATIC 			= 1,
	MCIT_STATIC_CURRENT		= 2,
	MCIT_STATIC_DIFFERENT	= 3,
	MCIT_MAX
};

/*
 * MCTP Control Set EID Operations (SE)
 *
 * DSP0236 1.3.1 Table 14 
 */
enum _MCSE 
{
	MCSE_SET 		= 0,
	MCSE_FORCE 		= 1,
	MCSE_RESET 		= 2,
	MCSE_DISCOVER 	= 3, 
	MCSE_MAX
};

/* STRUCTS ===================================================================*/

/*
 * MCTP Transport Header
 *
 * DSP0236 1.3.1 Table 1
 */
struct __attribute__((__packed__)) mctp_hdr 
{
	__u8 ver 	: 4;	//!< MCTP Header version 
	__u8 rsvd1 	: 4;

	__u8 dest; 			//!< Destination Endpoint ID
	__u8 src;  			//!< Source Endpoint ID

	__u8 tag	: 3;	//!< tag to track outstanding commands 
	__u8 owner 	: 1;	//!< Requester is the originator of this cmd
	__u8 seq 	: 2;	//!< Packet Sequence number modulo 4
	__u8 eom 	: 1;	//!< End of Message flag
	__u8 som 	: 1;	//!< Start of Message flag
}; 

/*
 * MCTP Packet
 *
 * This is a packed structure for transmission
 *
 * DSP0236 1.3.1 Table 1
 */
struct __attribute__((__packed__)) mctp_pkt
{
	struct mctp_hdr hdr;
	__u8 payload[MCLN_BTU];
};  

/**
 * MCTP Packet Wrapper object for software use. Not packed. Cannot be sent directly 
 */
struct mctp_pkt_wrapper
{
	struct timespec ts;				//!< Time when this packet was received 
	struct mctp_pkt_wrapper* next;	//!< The next mctp_packet in a linked list
	struct mctp_pkt pkt;			//!< The data of this object 
};

/* 
 * MCTP Type Header 
 *
 * DSP0236 1.3.1 Table 1
 */
struct __attribute__((__packed__)) mctp_type 
{
	__u8 type 	:7; 	//!< MCTP Message Type [MCMT]
	__u8 IC 	:1;		//!< Integrity Check Field 
};

/**
 * MCTP Message
 *
 * DSP0236 1.3.1 Table 1
 */
struct mctp_msg 
{
	__u8 src;			
	__u8 dst;
	__u8 type;
	__u8 owner;
	__u8 tag;
	__u16 len;
	struct timespec ts; 
	__u8 payload[MCLN_MSG_PAYLOAD];
};

/**
 * State of the MCTP endpoint
 *
 * This is used to track MCTP related configuration
 */
struct mctp_state 
{
	__u8 eid;	
	__u8 bus_owner_eid;
	__u32 verbose;
	uuid_t uuid;
};

/*
 * MCPT Control Message Fields (Request)
 * DSP0236 1.3.1 Table 10 
 */
struct __attribute__((__packed__)) mctp_ctrl 
{
	__u8 inst 		: 5; 	//!< Instance ID
	__u8 rsvd		: 1;
	__u8 datagram	: 1; 	//!< Datagram bit
	__u8 req 		: 1; 	//!< Request Bit 

	__u8 cmd;				//!< MCTP Control Command [MCCM]
};

/*
 * MCTP Control - Set EID Request
 *
 * DSP0236 1.3.1 Table 14 
 */
struct __attribute__((__packed__)) mctp_ctrl_set_eid_req 
{
	__u8 operation	: 2;
	__u8 rsvd1 		: 6;

	__u8 eid;
};

/*
 * MCTP Control - Set EID response
 *
 * DSP0236 1.3.1 Table 14 
 */
struct __attribute__((__packed__)) mctp_ctrl_set_eid_resp 
{
	__u8 comp_code; 

	__u8 allocation : 2;
	__u8 rsvd2		: 2;
	__u8 assignment	: 2;
	__u8 rsvd1 		: 2;

	__u8 eid;

	__u8 pool_size;
};

/*
 * MCTP Control - Get Endpoint ID Response
 *
 * DSP0236 1.3.1 Table 15
 */
struct __attribute__((__packed__)) mctp_ctrl_get_eid_resp 
{
	__u8 comp_code; 			// MCCC

	__u8 eid;

	__u8 id_type		: 2; 	// MCIT
	__u8 rsvd2 			: 2;	
	__u8 endpoint_type 	: 2;	// MCEP
	__u8 rsvd1 			: 2;

	__u8 medium_specific;
};

/*
 * MCTP Control - Get UUID Response
 *
 * DSP0236 1.3.1 Table 16
 */
struct __attribute__((__packed__)) mctp_ctrl_get_uuid_resp 
{
       __u8 comp_code;                         // MCCC

       __u8 uuid[16];
};

/*
 * MCTP Control - Version Support 
 *
 * This struct is used in the global variable to define what versions the emulator supports
 * This is not serialized over the wire
 * It is used in a linked list
 *
 * DSP0236 1.3.1 Table 18
 */
struct __attribute__((__packed__)) mctp_version 
{
	__u8 major;
	__u8 minor;
	__u8 update;
	__u8 alpha;
	__u8 type;					// MCTC 
	struct mctp_version *next_entry;
	struct mctp_version *next_type;
};

/*
 * MCTP Control - Version Support 
 *
 * This struct is sent over the wire. 
 *
 * DSP0236 1.3.1 Table 18
 */
struct __attribute__((__packed__)) mctp_ver 
{
	__u8 major;
	__u8 minor;
	__u8 update;
	__u8 alpha;
};

/*
 * MCTP Control - Get Version Support Request
 *
 * This struct is sent over the wire. 
 *
 * DSP0236 1.3.1 Table 18
 */
struct __attribute__((__packed__)) mctp_ctrl_get_ver_req
{
       __u8 type;
};

/*
 * MCTP Control - Get Version Support Response 
 *
 * This struct is sent over the wire. 
 *
 * DSP0236 1.3.1 Table 18
 */
struct __attribute__((__packed__)) mctp_ctrl_get_ver_resp
{
       __u8 comp_code;                                 // MCCC
       __u8 count;                                             // Number of entries returned
       struct mctp_ver versions[15];   // Versions supported   
};

/*
 * MCTP Control - Get Message Type Support Response 
 *
 * This struct is sent over the wire. 
 *
 * DSP0236 1.3.1 Table 18
 */
struct __attribute__((__packed__)) mctp_ctrl_get_msg_type_resp
{
       __u8 comp_code;         // MCCC
       __u8 count;                     // Number of entries returned
       __u8 list[59];          // Versions supported   
};

/**
 * MCTP Control Message Object
 */
struct __attribute__((__packed__)) mctp_ctrl_msg
{
       struct mctp_ctrl hdr;           //!< MCTP Control Message Header 
       union {
               struct mctp_ctrl_set_eid_req            set_eid_req;
               struct mctp_ctrl_set_eid_resp           set_eid_rsp;
               struct mctp_ctrl_get_eid_resp           get_eid_rsp;
               struct mctp_ctrl_get_uuid_resp          get_uuid_rsp;
               struct mctp_ctrl_get_ver_req            get_ver_req;
               struct mctp_ctrl_get_ver_resp           get_ver_rsp;
               struct mctp_ctrl_get_msg_type_resp      get_msg_type_rsp;
       } obj;
       __u8 len;                                       //!< Object Payload length in bytes 
};

/* Establish there is an mctp object so other objects can have a pointer to it */
struct mctp;

/**
 * Submission action object
 */
struct mctp_action
{
	struct mctp_msg *req;		//!< Request Message payload 
	struct mctp_msg *rsp;		//!< Response Message payload 
	struct mctp_pkt_wrapper *pw;//!< Linked list of packets

	struct timespec created;	//!< Time stamp when action was created
	struct timespec submitted;	//!< Time of last submission 
	struct timespec completed;	//!< Time when response was received 

	int valid;					//!< Bool if this object is 1=valid or 0=not 
	int completion_code;		//!< 0=Success, Failure Code otherwise
	int num;					//!< Number of transmission attempted 
	int max; 					//!< Maximum number of transmission attempts 
	void *user_data;			//!< Pointer to user data kept with action until completion

	sem_t *sem;					//!< Semaphore to pend on until action has completed 
	struct timespec timeout; 	//!< Absolute time when the semaphore will expire 

	//!< Function to call when this action is submitted
	void (*fn_submitted)(struct mctp *m, struct mctp_action *a);

	//!< Function to call when this action has completed 
	void (*fn_completed)(struct mctp *m, struct mctp_action *a);

	//!< Function to call if this action fails to complete
	void (*fn_failed)(struct mctp *m, struct mctp_action *a);
};

/**
 * Object passed to Socket Writer thread function 
 */
struct socket_writer 
{
	// Parent pointer
	struct mctp *m;

	// State fields
	__u32 loop;

	// Thread fields
	pid_t threadid;

	// State fields
	__u64 packet_count;
	__u64 dropped_count;
};

/**
 * Object passed to Packet Writer thread function 
 */
struct packet_writer 
{
	// Parent pointer
	struct mctp *m;

	// State fields
	__u32 loop;

	// Thread fields
	pid_t threadid;
	useconds_t sleep_usec;

	// State fields
	__u8 pkt_seq;
	__u64 packet_count;
	__u64 message_count;
};

/**
 * Object passed to Message Handler thread function 
 */
struct message_handler 
{
	// Parent pointer
	struct mctp *m;

	// State fields
	__u32 loop;

	// Thread fields
	pid_t threadid;
	useconds_t sleep_usec;
};

/**
 * Object passed to Packet Reader thread function 
 */
struct packet_reader 
{
	// Parent pointer
	struct mctp *m;

	// Thread fields
	pid_t threadid;

	// State fields
	__u32 loop;
	__u8 pkt_seq;
	__u64 packet_count;
	__u64 message_count;
	__u64 dropped_version;
	__u64 dropped_seqnum;
	__u64 dropped_noeom;
	__u64 dropped_nosom;
	__u64 dropped_wrongto;

	// In process Messages 
	struct mctp_msg *tags[MCTP_NUM_TAGS];
};

/**
 * Object passed to Socket Reader thread function 
 */
struct socket_reader 
{
	// Parent pointer
	struct mctp *m;

	// Thread fields
	pid_t threadid;
	useconds_t sleep_usec;

	// State fields
	__u32 loop;
	__u64 sleep_count;
	__u64 packet_count;
	__u64 dropped_count;
};

/** 
 * Object passed to Connection Handler thread function
 */ 
struct connection_handler 
{
	struct mctp *m;
	pid_t threadid;
	__u32 loop;
	int dontblock;
	sem_t *sem;
};

/**
 * Object passed to Submission Thread function 
 */
struct submission_thread
{
	struct mctp *m; 				//!< Parent pointer 
	pid_t threadid; 				//!< Threadid of this thread 
	__u32 loop;						//!< Thread step / loop counter 

	struct timespec action_delta;	//!< Relative time to wait on an action before resubmitting  	
	struct timespec thread_delta;	//!< Relative time for thread to wait when sleeping 
	struct timespec thread_timeout;	//!< Absolute time when to wake from pthread_cond_wait()

	pthread_mutex_t mtx;			//!< Thread sleep mutex 
	pthread_cond_t cond;			//!< Thread sleep condition 
	int wake;						//!< Request to wake the thread 
};

/**
 * Object passed to Action Completion Thread function 
 */
struct completion_thread
{
	struct mctp *m; 				//!< Parent pointer 
	pid_t threadid; 				//!< Threadid of this thread 
	__u32 loop;						//!< Thread step / loop counter 
	__u64 completed_actions;		//!< Number of actions completed 
	__u64 successful_actions;		//!< Number of actions that completed successfully 
	__u64 failed_actions;			//!< Number of actions that failed 
};

/**
 * State representation of a set of MCTP threads 
 */
struct mctp 
{
	struct mctp_state state;
	uuid_t uuid;
	struct mctp_version *mctp_versions;

	int (*handlers[MCMT_MAX]) (struct mctp *m, struct mctp_action *ma);

	// Thread control 
	pthread_mutex_t mtx;
	pthread_cond_t cond;	// Condition to wake up main thread if there is a failure with the worker threads
	__u32 verbose;
	int use_threads;
	int wait;
	int all_threads_started;
	int stop_threads;
	int dummy;

	// Outstanding commands array 
	struct mctp_action *tags[MCTP_NUM_TAGS];
	pthread_mutex_t tags_mtx;

	// Thread handles
	pthread_t pt_ch;		//!< PThread handle for Connection Thread 
	pthread_t pt_sr;		//!< PThread handle for Socket Reader Thread
	pthread_t pt_pr;		//!< PThread handle for Packet Reader Thread
	pthread_t pt_mh;		//!< PThread handle for Message Handler Thread 
	pthread_t pt_pw;		//!< PThread handle for Packet Writer Thread 
	pthread_t pt_sw;		//!< PThread handle for Socket Writer Thread 
	pthread_t pt_st;		//!< PThread handle for Submission Thread
	pthread_t pt_ct;		//!< PThread handle for Action Completion Thread

	// Thread state
	struct connection_handler ch;
	struct socket_reader sr; 
	struct packet_reader pr; 
	struct message_handler mh; 
	struct packet_writer pw;
	struct socket_writer sw;
	struct submission_thread st;
	struct completion_thread ct;

	// Thread functions
	void *(*fn_sr)(void *arg);
	void *(*fn_pr)(void *arg);
	void *(*fn_mh)(void *arg);
	void *(*fn_pw)(void *arg);
	void *(*fn_sw)(void *arg);
	void *(*fn_st)(void *arg);
	void *(*fn_ct)(void *arg);

	// Object Pools 
	struct ptr_queue *pkts;
	struct ptr_queue *msgs;
	struct ptr_queue *actions;

	// Queue fields
	struct ptr_queue *rpq;	//!< Receive Packet Queue
	struct ptr_queue *tpq;	//!< Transmit Packet Queue
	struct ptr_queue *rmq; 	//!< Receive Message Queue
	struct ptr_queue *tmq;	//!< Transmit Message Queue
	struct ptr_queue *taq;	//!< Transmit Action Queue
	struct ptr_queue *acq;	//!< Action Completed Queue

	// Socket fields
	int port;
	int mode;
	int sock;
	int conn;
	socklen_t client_len;
	struct sockaddr_in sa_server;
	struct sockaddr_in sa_client;
};

/* GLOBAL VARIABLES ==========================================================*/

/* PROTOTYPES ================================================================*/

/* External API */
struct mctp *mctp_init();
int mctp_run(struct mctp *m, int port, __u32 address, int mode, int use_threads, int dontblock);
int mctp_stop(struct mctp *m);
void mctp_request_stop(struct mctp *m);
int mctp_free(struct mctp *m);
void mctp_retire(struct mctp* m, struct mctp_action *a);

/**
 * Submit an object for transmission 
 *
 * Call will pend on a semaphore for a time sepcified in timespec delta if provided. 
 * If delta is not provided, call will submit and return immediately
 *
 * @param m 			struct mctp* 
 * @param type  		mctp message type
 * @param obj   		Pointer to serialized data buffer to send 
 * @param len   		Length of object in bytes 
 * @param retry 		Number of attempts to send the object. -1=forever, -2=default
 * @param delta  		struct timespec* Time to wait for a response. 
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
);

// Verbosity levels 
void mctp_set_verbosity(struct mctp *m, __u32 level);
__u32 mctp_get_verbosity(struct mctp *m);

int mctp_set_version(struct mctp *m, __u8 type, __u8 major, __u8 minor, __u8 update, __u8 alpha);
void mctp_prnt_ver(struct mctp_version *mv, int indent);
int mctp_sprnt_ver(char *buf, struct mctp_version *mv);
int vercmp(struct mctp_version *lhs, struct mctp_version *rhs);

// Set handlers 
void mctp_set_handler(struct mctp *m, int type, int (*func)(struct mctp *m, struct mctp_action *ma));
void mctp_set_mh(struct mctp *m, void *(*fn)(void*arg));

// Functions to populate common MCTP structs 
void mctp_fill_msg_hdr(struct mctp_msg *mm, __u8 dest, __u8 src, __u8 owner, __u8 tag);
void mctp_fill_ctrl(struct mctp_msg *mm, __u8 req, __u8 datagram, __u8 inst, __u8 cmd);

int mctp_ctrl_fill_get_eid(struct mctp_ctrl_msg *m);
int mctp_ctrl_fill_get_type(struct mctp_ctrl_msg *m);
int mctp_ctrl_fill_get_ver(struct mctp_ctrl_msg *m, int type);
int mctp_ctrl_fill_get_uuid(struct mctp_ctrl_msg *m);
int mctp_ctrl_fill_set_eid(struct mctp_ctrl_msg *m, int eid);

int mctp_pkt_count(struct mctp_msg *mm);

/* MCTP Control Message Functions */
int mctp_ctrl_handler(struct mctp *m, struct mctp_action *ma);
struct mctp_ctrl *mctp_get_ctrl(struct mctp_msg *mm);
__u8 *mctp_get_ctrl_payload(struct mctp_msg *mm);
unsigned int mctp_len_ctrl(__u8 *ptr);

/* Thread Functions */
void *mctp_connection_handler(void *arg);
void *mctp_socket_reader(void *arg);
void *mctp_packet_reader(void *arg);
void *mctp_message_handler(void *arg);
void *mctp_socket_writer(void *arg);
void *mctp_packet_writer(void *arg);
void *mctp_submission_thread(void *arg);
void *mctp_completion_thread(void *arg);

/* Print methods for mctp objects*/
void mctp_prnt_hdr(struct mctp_hdr *mh);
void mctp_prnt_pkt_wrapper(struct mctp_pkt_wrapper *pw);
void mctp_prnt_pkt(struct mctp_pkt *mp);
void mctp_prnt_type(struct mctp_type *mt);
void mctp_prnt_msg(struct mctp_msg *mm);
void mctp_prnt_state(struct mctp_state *ms);

/* Return a string representation of enum entries */
const char *mcmt(unsigned u);
const char *mcrm(unsigned u);
const char *mccc(unsigned u);
const char *mccm(unsigned u);
const char *mcep(unsigned u);
const char *mcid(unsigned u);
const char *mcit(unsigned u);
const char *mcse(unsigned u);

#endif //ifndef _MCTP_H
