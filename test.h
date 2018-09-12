#ifndef TEST_H
#define TEST_H


#define MAX_NUM_DLGS_IN                (2048)
#define MAX_NUM_DLGS_OUT                (2048)

#define MAX_ADDR_LEN      (36)    /* Max length of SCCP address */

#define MAX_BCDSTR_LEN    (10)    /* Max size of BCD octet string */
#define MAX_ADDSTR_LEN    (8)     /* Max size used to store addr digits */


/*
 * Structure used for origination and destination addresses
 */
typedef struct
{
  u8 num_bytes;                 /* number of digits */
  u8 data[MAX_ADDR_LEN];        /* contains the digits */
} SCCP_ADDR;




/*
 * Structure used for parameters of format TBCD_STRING
 */
typedef struct
{
  u8 num_bytes;                 /* number of bytes of data */
  u8 data[MAX_BCDSTR_LEN];      /* contains the data */
} BCDSTR;


/*
 * Structure used for parameters of format AddressString
 */
typedef struct
{
  u8 num_dig_bytes;             /* number of bytes of digits */
  u8 noa;                       /* nature of address */
  u8 npi;                       /* numbering plan indicator */
  u8 digits[MAX_ADDSTR_LEN];    /* contains the digits */
} ADDSTR;




/*
 * Structure used for parameters of format SM-RP-OA
 */
typedef struct
{
  u8 toa;                       /* type of address */
  u8 num_dig_bytes;             /* number of bytes of digits */
  u8 noa;                       /* nature of address */
  u8 npi;                       /* numbering plan indicator */
  u8 digits[MAX_ADDSTR_LEN];    /* contains the digits */
} OASTR;



/*
 * HLR agent process dialogues state definitions
 */
//#define HLR_WAIT_OPEN_CNF       (1)     /* Wait for Open confirmation */
//#define HLR_WAIT_SERV_CNF       (2)     /* Wait for MT SMS confirm */
//#define HLR_WAIT_CLOSE_IND      (3)     /* Wait for Close ind */



/*
 * dialogues state definitions
 */

#define DLG_IDLE               (0)     /* Idle */
#define DLG_OPEN               (1) 
#define DLG_PENDING_OPEN_CNF   (2)     /* Waiting for Open confirmation (Open CNF IND) */
#define DLG_PENDING_DELIMIT    (3)   /* Wait for MAP Delimiter Ind */
#define DLG_CLOSING            (4)    /* Wait for Close ind */


#define MSISDN_MAX_SIZE         (32)
#define AC_MAX_LEN              (255) /* max length of app context */





struct map_data{

    u8 num_bytes;
    u8 data[64];

};



/*
 * Structure Definitions. Note that this structure is simplified because MTR
 * only allows one service primitive per dialogue so storage is only needed
 * for one service primitive type and one invoke ID.
 * this dialogues is incoming dialogues from HLR, MAP_SRI_SM receiving
 */

//typedef struct dlg_info;

typedef struct dlg_info {
    u16 dlg_id;
    u8  state;                          /* state */
    u8 type;                            /* dialogue type */ 
    u16 ptype;                          /* service primitive type */
    u8 service;
    u8  invoke_id;                      /* invoke ID */
    u16 map_inst;                       /* instance of MAP module */
    u8  ac_len;                         /* Length of application context */
    u8  app_context[AC_MAX_LEN];    /* Saved application context */
    u8  msisdn[MSISDN_MAX_SIZE];    /* Saved MSISDN */
    u8  msisdn_len;                     /* Length of MSISDN */
    u8  term_mode;                      /* Termination Mode */
    u8  ati_req_info;                   /* ATI Request Info */
    u8  mms;                            /* More Messages to Send (MMS) parameter */
    u8 dest_address[36];                 /*sccp destination address */
    u8 origin_address[36];                 /*sccp destination address */
    int (*service_handler)(struct dlg_info *);   /*pointer to callback function, to process service data */
    struct dlg_info *dlg_ref;   /*pointer to opposite dialogue info */
    struct map_data map_data;
} dlg_info;



/* dialogue type */

#define IN_SMSC_SRI_SM  (0)   /*incoming dialogue from SMSC (relayed from HLR) for MAP_SRI_SM_REQ */ 
#define OUT_HLR_SRI_SM  (1)
#define IN_SMSC_MT_FSM  (2)
#define OUT_VLR_MT_FSM  (3)

#endif
