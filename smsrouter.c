/*
                Copyright (C) Dialogic Corporation 1999-2015. All Rights Reserved.

 Name:          mtr.c

 Description:   MAP Test Responder (MTU)
                Simple responder for MTU (MAP test utility)
                This program responds to an incoming dialogue received
                from the MAP module.

                The program receives:
                        MAP-OPEN-IND
                        service indication
                        MAP-DELIMITER-IND

                and it responds with:
                        MAP-OPEN-RSP
                        service response
                        MAP-CLOSE-REQ (or MAP-DELIMITER-REQ)

                The following services are handled:
                        MAP-FORWARD-SHORT-MESSAGE
                        MAP-SEND-IMSI
                        MAP-SEND-ROUTING-INFO-FOR-GPRS
                        MAP-MT-FORWARD-SHORT-MESSAGE
                        MAP-SEND-ROUTING-INFO-FOR-SMS
                        MAP-PROCESS-UNSTRUCTURED-SS
                        MAP-UNSTRUCTURED-SS-REQ

 Functions:     main


 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "system.h"
#include "msg.h"
#include "sysgct.h"
#include "map_inc.h"
#include "smsrouter.h"
#include "pack.h"


//#include "hlr.h"
#include "test.h"
#include "hlr.h"

    int fd1[2]; //pipes descriptorw with hlr_agent



/*
 * Prototypes for local functions:
 */
int MTR_process_map_msg(MSG *m);
int MTR_cfg(u8 _mtr_mod_id, u8 _map_mod_id, u8 _trace_mod_id,u8 _dlg_term_mode);
int MTR_set_default_term_mode(u8 new_term_mode);
int mtr_ent(u8 mtr_id, u8 map_id, u8 trace, u8 dlg_term_mode);

static int init_resources(void);
static u16 MTU_def_alph_to_str(u8 *da_octs, u16 da_olen, u16 da_num,
                                 char *ascii_str, u16 max_strlen);
static int print_sh_msg(MSG *m);
static dlg_info *get_dialogue_info(u16 dlg_id);
static int MTR_trace_msg(char *prefix, MSG *m);
static int MTR_send_msg(u16 instance, MSG *m);
static int MTR_send_OpenResponse(u16 mtr_map_inst, u16 dlg_id, u8 result);
static int MTR_ForwardSMResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_SendImsiResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_SendRtgInfoGprsResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_UnstructuredSSRequest (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_UnstructuredSSResponse (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_ProcessUnstructuredSSReqRsp (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_UnstructuredSSNotifyRsp (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_send_MapClose(u16 mtr_map_inst, u16 dlg_id, u8 method);
static int MTR_send_Abort(u16 mtr_map_inst, u16 dlg_id, u8 reason);
static int MTR_send_Delimit(u16 mtr_map_inst, u16 dlg_id);
static int MTR_get_invoke_id(MSG *m);
static int MTR_get_applic_context(MSG *m, u8 *dst, u16 dstlen);
static u8  MTR_get_msisdn(MSG *m, u8 *dst, u16 dstlen);
static int MTR_get_sh_msg(MSG *m, u8 *dst, u16 dstlen);
static u8  MTR_get_ati_req_info(MSG *m);
static int MTR_get_parameter(MSG *m, u16 pname, u8 **found_pptr);
static u16 MTR_get_primitive_type(MSG *m);
static int MTR_MT_ForwardSMResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_SendRtgInfoSmsResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_ATIResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MAP_get_destination_address(MSG *m, u8 *dst, u16 dstlen);
static int MAP_get_origin_address(MSG *m, u8 *dst, u16 dstlen);
static u16 alloc_out_dlg_id(u16 *dlg_id_ptr);

/*
 * Static data:
 */
static dlg_info dlg_info_in[MAX_NUM_DLGS_IN];    /* Dialog_info for dialogues initiated from remote end*/
dlg_info dlg_info_out[MAX_NUM_DLGS_OUT];    /* Dialog_info for dialogues initiated from smsrouter*/

static u8 mtr_mod_id;                           /* Module id of this task */
static u8 mtr_map_id;                           /* Module id for all MAP requests */
static u8 mtr_trace;                            /* Controls trace requirements */
static u8 mtr_default_dlg_term_mode;            /* Controls which end terminates a dialog */

static u16 out_dlg_id;// = 0x0000;                   /* current dialogue ID, send from smsrouter to hlr */
static u16 base_out_dlg_id = 0x0000;   
static u16 dlg_active;                   /* number of actibe outgoing dialogues */
static u32 hlr_o_dlg_count;                /* counts number of dialogues opened from smsrouter to hlr*/


static u8 test_dest[]= {0x12, 0x06, 0x00, 0x11, 0x04, 0x97, 0x05, 0x66, 0x15, 0x20, 0x00 };

static u8 test_origin[]= {0x12, 0x08, 0x00, 0x11, 0x04, 0x97, 0x05, 0x66, 0x15, 0x20, 0x09 };




#define MTR_ATI_RSP_SIZE         (8)
#define MTR_ATI_RSP_NUM_OF_RSP   (8)
static u8 mtr_ati_rsp_data[MTR_ATI_RSP_NUM_OF_RSP][MTR_ATI_RSP_SIZE] = 
{ 
    {0x14, 0x10, 0x00, 0x00, 0x80, 0x00, 0x00, 0x14},     //0
    {0x14, 0x20, 0x00, 0x00, 0x70, 0x00, 0x00, 0x14},     //1
    {0x14, 0x30, 0x00, 0x00, 0x60, 0x00, 0x00, 0x14},     //2
    {0x14, 0x40, 0x00, 0x00, 0x50, 0x00, 0x00, 0x14},     //3
    {0x14, 0x50, 0x00, 0x00, 0x40, 0x00, 0x00, 0x14},     //4
    {0x14, 0x60, 0x00, 0x00, 0x30, 0x00, 0x00, 0x14},     //5
    {0x14, 0x70, 0x00, 0x00, 0x20, 0x00, 0x00, 0x14},     //6
    {0x14, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x14}      //7
};

/*
 * MTU_def_alph_to_str()
 * Returns the number of ascii characters formatted into the
 * ascii string. Returns zero if this could not be done.
 */
u16 MTU_def_alph_to_str(da_octs, da_olen, da_num, ascii_str, max_strlen)
  u8   *da_octs;      /* u8 array from which the deft alph chars are recoverd */
  u16   da_olen;      /* The formatted octet length of da_octs  */
  u16   da_num;       /* The number of formatted characters in the array */
  char *ascii_str;    /* location into which chars are written */
  u16   max_strlen;   /* The max space available for the ascii_str */
{
  char *start_ascii_str;   /* The first char */
  u16  i;                  /* The bit position along the da_octs */

  start_ascii_str = ascii_str;

  if ((da_olen * 8) > ((max_strlen + 1) * 7))
    return(0);

  if (  ( (da_num * 7)  >   (da_olen      * 8) )
     || ( (da_num * 7) <= ( (da_olen - 1) * 8) ) )
  {
    /*
     * The number of digits does not agree with the size of the string
     */
    return (0);
  }
  else
  {
    for (i=0; i < da_num; i++)
    {
      *ascii_str++ = DEF2ASCII(unpackbits(da_octs, i * 7, 7));
    }

    *ascii_str++ = '\0';

    return((u16)(ascii_str - start_ascii_str));
  }
}

  /*
   * SMSR_alloc_invoke_id allocates an invoke ID to be used for a MAP request.
   */
  static u8 SMSR_alloc_invoke_id()
  {
    /*
     * This function always uses the same invoke ID (because only one invocation
     * can be in progress at one time in this example program). In a real
     * application this function would have to search for a free invoke ID and
     * allocate that.
     */
    return(DEFAULT_INVOKE_ID);
  }



  /*
   * function SMSR_send_msg(MSG *m)
   * sends a MSG. On failure the
   * message is released and the user notified.
   *
   * INPUT: MSG *m to send
   *
   * Always returns zero.
   */
  static int SMSR_send_msg(MSG *m)
   {

    if (GCT_send(m->hdr.dst, (HDR *)m) != 0)
    {
      //MTU_disp_err("*** failed to send message");
      relm((HDR *)m);
    }
    return 0;
  }
  /* end of SMSR_send_msg()*/



  /*
   * function SMSR_send_dlg_req(SMSR_MSG *req)
   * allocates a message (using the
   * getm() function) then converts the primitive parameters
   * from the 'C' structured representation into the correct
   * format for passing to the MAP module.
   * The formatted message is then sent.
   *
   * INPUT: SMSR_MSG *req - structured primitive request to send
   *
   * Always returns zero.
   */
  static int SMSR_send_dlg_req(SMSR_MSG *req)
    {
    MSG *m;               /* message sent to MAP */

    req->dlg_prim = 1;

    /*
     * Allocate a message (MSG) to send:
     */
    if ((m = getm((u16)MAP_MSG_DLG_REQ, req->dlg_id, NO_RESPONSE,
                                        MTU_MAX_PARAM_LEN)) != 0)
    {
      m->hdr.src = smsrouter_mod_id;
      m->hdr.dst = map_mod_id;

      /*
       * Format the parameter area of the message and
       * (if successful) send it
       */
      if (MTU_dlg_req_to_msg(m, req) != 0)
      {
        MTU_disp_err("failed to format dialogue primitive request");
        relm(&m->hdr);
      }
      else
      {
      //if (mtu_args.options & MTU_TRACE_TX)
          MTU_display_sent_msg(m);

        /*
         * and send to the call processing module:
         */
        SMSR_send_msg(m);
      }
    }
    return 0;
  } /* end of SMSR_send_dlg_req() */




 /*
 * hlr_open_dlg opens a dialogue with hlr by sending:
 *
 *   MAP-OPEN-REQ
 *   service primitive request(SRI_FOR_SM)
 *   MAP-DELIMITER-REQ
 *
 * Returns zero if the dialogue was successfully opened.
 *         -1   if the dialogue could not be opened.
 */
//static int hlr_open_dlg(service, imsi, ptr)//dlg_id)
static int SMSR_open_dlg(dlg_info *root_dlg_info, dlg_info *child_dlg_info, u16 dlg_id)
{
    SMSR_MSG req;          /* structured form of request message */
    dlg_info *dlg;         /* dialogue data structure */
    int i,j;              /* loop counters */
    //  u16 dlg_id;           /* dialogue ID */
    static int numberend;
    char temp_destaddr[256];
    char temp_idx[256];
    char temp;

    printf("1\n");
    /*
     * First allocate a dialogue ID for the dialogue. This dialogue ID is used
     * in all messages sent to and from MAP associated with this dialogue.
     */
    //dialog id already allocated in smsrouter processs

    //  if (hlr_alloc_dlg_id(&dlg_id) != 0)
    //  return(-1);

    /*
     * Increment the dialogue counter and display count every 1000 dialogues
     */
    hlr_o_dlg_count++;

    //  if (((mtu_dlg_count % 1000) == 0) && (mtu_args.options & MTU_TRACE_STATS))
    //  fprintf(stderr, "Number of dialogues opened = %d\n", mtu_dlg_count);

    /*
     * Get a pointer to the data associated with this dialogue id and store
     * the service to be used for this dialogue.
     */
    //printf("2\n");

    //  ptr++;
    u16 dlg_id = *(ptr+1);
    //    ptr++;
    u8 mlen = *(ptr+2);


 //   dlg = MTU_get_dlg_data(dlg_id);

 //   dlg->service = service;
    /*
     * Open the dialogue by sending MAP-OPEN-REQ.
     */
   printf("3\n");
    memset((void *)req.pi, 0, PI_BYTES);
    req.dlg_id = dlg_id;
    req.type = MAPDT_OPEN_REQ;

    printf("4\n");
    //  if(mtu_args.nc !=0)
    // {
    //  req.nc = mtu_args.nc;
    //  bit_set(req.pi, MAPPN_nc);
    // }

    /*
     * Copy the appropriate application context into the message structure
     */
    bit_set(req.pi, MAPPN_applic_context);

    //switch (service)
    //{
        //    case MTU_SEND_IMSI:
        /*
         * Send IMSI
         */
        //for (i=0; i<AC_LEN; i++)
        // req.applic_context[i] = imsiRetrievalContextv2[i];
        // break;

        // case MTU_SRI_GPRS:
        /*
         * Send routing info for GPRS
         */
        // for (i=0; i<AC_LEN; i++)
        // req.applic_context[i] = gprsLocationInfoRetrievalContextv3[i];
        //break;

        //case MTU_MT_FORWARD_SM:
        /*
         * MT Forward short message
         */
        //for (i=0; i<AC_LEN; i++)
        //    req.applic_context[i] = shortMsgMTRelayContext[i];
        //break;

      //case MTU_SRI_SM:
        /*
         * Send Routing Information For SM
         */
        for (i=0; i<AC_LEN; i++)
            req.applic_context[i] = shortMsgGatewayContext[i];
      //  break;

        //case MTU_PROCESS_USS_REQ:
        /*
         * Send ProcessUnstructuredSS-Request
         */
        //for (i=0; i<AC_LEN; i++)
        //  req.applic_context[i] = networkUnstructuredSsContext[i];
        // break;

    //  case MTU_FORWARD_SM:
    //  default:
        /*
         * Forward short message
         */
      // switch (mtu_args.map_version)
      // {
          //case MTU_MAPV1:
      // for (i=0; i<AC_LEN; i++)
      //  req.applic_context[i] = shortMsgRelayContextv1[i];
          //  break;
          //default:
      // for (i=0; i<AC_LEN; i++)
      //  req.applic_context[i] = shortMsgRelayContextv2[i];
      // break;
      //}
   //     break;
   // }


    /* here we should fill correctly originating address and destination address when send OPEN-REQ to HLR from SMSROUTER */


    /* strncpy(temp_destaddr, mtu_args.dest_address, 255); */

    /* if (mtu_args.dest_add_digits != 0) */
    /* { */
    /*   /\*  */
    /*    * Append 6 digits i.e. 000000 to 999999  */
    /*    *\/ */
    /*   numberend++;  */
    /*   if(numberend>999999) */
    /*     numberend = 0; */

    /*   sprintf(temp_idx,"%0.6u",numberend); */

    /*   /\* swap the pairs of digits - there is always an even number (6) *\/ */
    /*   i = 0;   */
    /*   while (temp_idx[i] != '\0') */
    /*   { */
    /*     temp = temp_idx[i]; */
    /*     temp_idx[i] = temp_idx[i+1]; */
    /*     temp_idx[i+1] = temp; */
    /*     i+=2; */
    /*   } */

    /*   /\* append to the existing address *\/ */
    /*   strncat(temp_destaddr, temp_idx, 255); */
    /* } */

    /* if (mtu_args.options & MTU_TRACE_TX) */
    /*   fprintf(stderr, "Dest: %s\n", &temp_destaddr); */

    /* /\* */
    /*  * Copy the destination address parameter into the MAP-OPEN-REQ, */
    /*  * converting from ASCII to hex, and packing into BCD format. */
    /*  *\/ */
     bit_set(req.pi, MAPPN_dest_address);

    /* i = 0; */
    /* j = 0; */
    /* while ((temp_destaddr[j] != '\0') && (i < MAX_ADDR_LEN)) */
    /* { */
     for (i = 0; i<11; i++)
         req.dest_address.data[i] = test_dest[i];
         //req.dest_address.data[i] = (hextobin(temp_destaddr[j])) << 4; */

    /*   if (mtu_args.dest_address[j+1] != '\0') */
    /*   { */
    /*     req.dest_address.data[i++] |= hextobin(temp_destaddr[j+1]); */
    /*     j += 2; */
    /*   } */
    /*   else */
    /*   { */
    /*     i++; */
    /*     break; */
    /*   } */
    /* } */
     req.dest_address.num_bytes = 11;

    /* /\* */
    /*  * Copy the origination address parameter into the MAP-OPEN-REQ, converting */
    /*  * from ASCII to hex, and packing into BCD format. */
    /*  *\/ */
     bit_set(req.pi, MAPPN_orig_address);

    /* i = 0; */
    /* j = 0; */
    /* while ((mtu_args.orig_address[j] != '\0') && (i < MAX_ADDR_LEN)) */
    /* { */
    /*   req.orig_address.data[i] = (hextobin(mtu_args.orig_address[j])) << 4; */
     for (i = 0; i<11; i++)
         req.orig_address.data[i] = test_origin[i];


    /*   if (mtu_args.dest_address[j+1] != '\0') */
    /*   { */
    /*     req.orig_address.data[i++] |= hextobin(mtu_args.orig_address[j+1]); */
    /*     j += 2; */
    /*   } */
    /*   else */
    /*   { */
    /*     i++; */
    /*     break; */
    /*   } */
    /* } */
     req.orig_address.num_bytes = 11;


    //sending OPEN-REQUEST
  int ret = SMSR_send_dlg_req(&req);

   printf("ret = %d\n");
    /*
     * Send the appropriate service primitive request. First, allocate an invoke
     * ID. This invoke ID is used in all messages to and from MAP associated with
     * this request.
     */
    dlg->invoke_id = SMSR_alloc_invoke_id();

    switch (service)
    {
      case MTU_SEND_IMSI:
        /*
         * Send IMSI
         */
      // MTU_send_imsi(dlg_id, dlg->invoke_id);
        break;

      case MTU_SRI_GPRS:
        /*
         * Send routing info for GPRS
         */
        //dlg->imsi = *imsi;
        //MTU_sri_gprs(dlg_id, dlg->invoke_id);
        break;

      case MTU_MT_FORWARD_SM:
        /*
         * Forward short message
         */
        //MTU_MT_forward_sm(dlg_id, dlg->invoke_id);
        break;

      case MTU_SRI_SM:
        /*
         * Send Routing Information For SM
         */
      //      MTU_sri_sm(dlg_id, dlg->invoke_id);
      //	MTU_sri_sm2();
      send_srv_req2(ptr);

        break;

      case MTU_PROCESS_USS_REQ:   /* USSD */
        /*
         * Send ProcessUnstructuredSS-Request
         */
        //MTU_process_uss_req(dlg_id, dlg->invoke_id);
        break;

      case MTU_FORWARD_SM:
      default:
        /*
         * Forward short message
         */
        //MTU_forward_sm(dlg_id, dlg->invoke_id);
        break;
    }

    /*
     * Now send MAP-DELIMITER-REQ to indicate that no more requests will be sent
     * (for now).
     */
    req.type = MAPDT_DELIMITER_REQ;
    memset((void *)req.pi, 0, PI_BYTES);


    /* if(mtu_args.qos_present) */
    /* { */
    /*   req.qos = mtu_args.qos; */
    /*   req.qos_size = mtu_args.qos_size; */
    /*   bit_set(req.pi, MAPPN_qos); */
    /* } */

      SMSR_send_dlg_req(&req);

    /*
     * Set state to wait for the MAP-OPEN-CNF which indicates whether or not
     * the dialogue has been accepted.
     */
    dlg->state = DLG_PENDING_OPEN_CNF;

    printf(" funciton %s dlg info pointer = %p dlg state = %d\n", __PRETTY_FUNCTION__, dlg, dlg->state);

    return 0;
  }
  /* end of SMSR_open_dlg() */


/*
 * print_sh_msg()
 *
 * prints a received short message
 *
 * Always returns zero
 *
 */
int print_sh_msg(m)
  MSG *m;
{
  int plen;                     /* length of primitive data */
  u16 msg_len;                  /* Number of characters in short message */
  u8  raw_SM[MAX_SM_SIZE];      /* Buffer for holding raw SM */
  char ascii_SM[MAX_SM_SIZE];   /* Buffer for holding ascii SM */
  u8 num_semi_oct;              /* number of encoded useful semi-octets */
  u8 num_dig_bytes;             /* number of bytes of digits */
  u8 tot_header_len = SIZE_UI_HEADER_FIXED;  /* start off with the fixed part */

  /*
   * Find SM parameter data in message
   */
  plen = MTR_get_sh_msg(m, raw_SM, MAX_SM_SIZE);

  if (plen > 0)
  {
    /* calc size of TP-OA in bytes*/
    num_semi_oct  = raw_SM[1];
    num_dig_bytes = (num_semi_oct/2) + (num_semi_oct % 2);

    tot_header_len += num_dig_bytes; /* now total header length */

    plen -= tot_header_len;
    msg_len = raw_SM[tot_header_len - 1];
  }
  
  printf("MTR Rx: Short Message User Information:\n");
  if (plen > 0)
  {
    if (MTU_def_alph_to_str(raw_SM + tot_header_len, (u16)plen, msg_len, ascii_SM, MAX_SM_SIZE) > 0)
    {
      printf("MTR Rx: %s\n", ascii_SM);
      return(0);
    }
  }
  printf("MTR Rx: (error decoding)\n");
  return(0);
}
  /*
  *******************************************************************************
  *                                                                             *
  * handle_dlg_service_srism_ind() : process service data in  correct way                     *
  *
  * callback function, invoked when DLG_DELIMIT received after SRI_SM received from
  * SMSC (relayed over HLR from SMSC in first step)
  *******************************************************************************
  */
  int handle_dlg_service_srism_ind(dlg_info *root_dlg_info)
  {
      u16 out_dlg_id = 0;          /*Dialogue ID for outgoing dialogues */
        struct dlg_info *child_dlg_info;

printf("hello world from callback for first srism received !\n");


//here we should send sri_for_sm from smsrouter to HLR
//we use pipe to send some command to hlr agent
//todo
// - allocate dlg id for outgoing dialogue
alloc_out_dlg_id(&out_dlg_id);

child_dlg_info = get_dialogue_info(out_dlg_id);

child_dlg_info->dlg_ref = root_dlg_info;
root_dlg_info->dlg_ref = child_dlg_info;

//hlr_open_dlg(service, &imsi, &buff);
SMSR_open_dlg(root_dlg_info, child_dlg_info, out_dlg_id);


  return 0;
  }

  /*
  *******************************************************************************
  *                                                                             *
  * handle_dlg_service_srism_ack() : process service data in  correct way                     *
  *
  * callback function, invoked when DLG_DELIMIT received after SRI_SM_ACK with true subscriber data received from
  * from HLR
  *******************************************************************************
  */
  int handle_dlg_service_srism_ack()
  {

printf("hello world from callback when HLR replied with IMSI and VLR data!\n");

  return 0;
  }


/*
*******************************************************************************
*                                                                             *
* smsrouter_dlg_ind: handles dialogue indications                                  *
*                                                                             *
*******************************************************************************
*/
/*
* Returns 0 on success
*         INTUE_INVALID_DLG_ID if the dialogue id was invalid
*         INTUE_MSG_DECODE_ERROR if the message could not be decoded
*/
int smsrouter_dlg_ind(MSG *m)
{
    dlg_info * dlg_info, * dlg_info_2;           /* State info for dialogue */
    //dlg_info * dlg_info_2;       /* Outgoing dialogue to HLR */
 

 u16  ic_dlg_id = 0;              /* Dialogue id */
  u16  ptype = 0;               /* Parameter Type */
  u8   *pptr;                   /* Parameter Pointer */
  // dlg_info * dlg_info;           /* State info for dialogue */
  //dlg_info * dlg_inf2;       /* Outgoing dialogue to HLR */
  u8   send_abort = 0;          /* Set if abort to be generated */
  int  invoke_id = 0;           /* Invoke id of received srv req */
  int  length = 0;
  u16 out_dlg_id = 0;          /*Dialogue ID for outgoing dialogues */

  u8 buffer[64];  //data buffer for transer data from smsrouter to hlr_agent


  ic_dlg_id = m->hdr.id;

  printf("function %s, ic_dlg_id = %d\n", __PRETTY_FUNCTION__, ic_dlg_id);
  //printf("function %s, ic_dlg_id = %d\n", __PRETTY_FUNCTION__, ic_dlg_id);

  pptr = get_param(m);
  ptype = *pptr;

  /*
   * Get state information associated with this dialogue
   */
  dlg_info = get_dialogue_info(ic_dlg_id);

  if (dlg_info == 0)
    return 0;

  printf("function %s, dlg info pointer = %p dlg state = %d\n", __PRETTY_FUNCTION__, dlg_info, dlg_info->state);

  switch (dlg_info->state)
  {
    case DLG_IDLE:
      /*
       *
    * suppose we receive first DLG_OPEN and dlg_info->state = IDLE
    *
    *
    */

	if (ptype == MAPDT_OPEN_IND )
	    {
              if (mtr_trace)
                printf("sms_router: Received Open Indication\n");

              /*
               * Save application context and MAP instance
               * We don't do actually do anything further with it though.
               */
              dlg_info->map_inst = (u16)GCT_get_instance((HDR*)m);

              /*
               * Set the termination mode based on the current default
               */
              dlg_info->term_mode = mtr_default_dlg_term_mode;
              dlg_info->mms = 0;
              dlg_info->ac_len = 0;

	      length = MAP_get_destination_address(m, dlg_info->dest_address, 36);

	      printf("length of sccp dest address = %d\n", length);
	      printf(" dest0 : %d\n", dlg_info->dest_address[0]);
	      printf(" dest1 : %d\n", dlg_info->dest_address[1]);
	      printf(" dest2 : %d\n", dlg_info->dest_address[2]);
	      printf(" dest3 : %d\n", dlg_info->dest_address[3]);


	      length = MAP_get_origin_address(m, dlg_info->origin_address, 36);

	      printf("length of sccp orig address = %d\n", length);

	      printf(" orig0 : %d\n", dlg_info->origin_address[0]);
	      printf(" orig1 : %d\n", dlg_info->origin_address[1]);
	      printf(" orig2 : %d\n", dlg_info->origin_address[2]);
	      printf(" orig3 : %d\n", dlg_info->origin_address[3]);



              length = MTR_get_applic_context(m, dlg_info->app_context, MTR_MAX_AC_LEN);
              if (length > 0)
                dlg_info->ac_len = (u8)length;

              if (dlg_info->ac_len != 0)
              {
                /*
                 * Respond to the OPEN_IND with OPEN_RSP and wait for the
                 * service indication
                 */
                MTR_send_OpenResponse(dlg_info->map_inst, ic_dlg_id, MAPRS_DLG_ACC);
                dlg_info->state = DLG_OPEN;
              }
              else
              {
                /*
                 * We do not have a proper Application Context - abort
                 * the dialogue
                 */
                send_abort = 1;
              }
	      //              break;

	    }



	break;

  case DLG_PENDING_OPEN_CNF:

 printf("function %s, dialog state pending open cnf ic_dlg_id = %d\n", __PRETTY_FUNCTION__, ic_dlg_id);

dlg_info->state = DLG_OPEN;


      break;


  case DLG_PENDING_DELIMIT:

	if (ptype == MAPDT_DELIMITER_IND )
	    {

              /*
               * Delimiter indication received. Now send the appropriate
               * response depending on the service primitive that was received.
               */
              if (mtr_trace)
                printf("smsrouter Rx: Received delimiter Indication\n");

              //handle_dlg_service();

	      //here we should send sri_for_sm from smsrouter to HLR
	      //we use pipe to send some command to hlr agent
	      //todo
	      // - allocate dlg id for outgoing dialogue
	      alloc_out_dlg_id(&out_dlg_id);
	      // - send allocated dlg id + command to hlr agent
	      // change state of outgoing dlg id to pending open cnf state
	      dlg_info_2 = get_dialogue_info(out_dlg_id);
	      int l = 26;

	      memcpy(&buffer[1], &out_dlg_id, 2);
	      memcpy(&buffer[2], &dlg_info->map_data.num_bytes, 1);
	      memcpy(&buffer[3], &dlg_info->map_data.data[0], dlg_info->map_data.num_bytes);

	      printf("out dlg id before sending to hlr agent = %d\n", out_dlg_id);
printf("out dlg id state before sending to hlr agent = %d\n", dlg_info_2->state);

	      // write(fd1[1], _SRI_SM", strlen("MAP_SRI_SM")+1);
	      //write(fd1[1], &out_dlg_id, sizeof (out_dlg_id));
 dlg_info->service_handler(dlg_info);

 exit(0);

 write(fd1[1], &buffer[0], sizeof (buffer));

	      dlg_info_2->state = DLG_PENDING_OPEN_CNF;
	      dlg_info_2->type = OUT_HLR_SRI_SM ;

	    }


	if (ptype == MAPDT_CLOSE_IND )
	    {
		printf("in state pending delimit receive MAP_CLOSE\n");
		printf("should process pending request\n");

		if (dlg_info->type == OUT_HLR_SRI_SM)
		    {
			printf("receive MAP_CLOSE for dialogue OUT_HLR_SM");



		    }


		exit(0);
	    }

      break;

  }

    return 0;
}


int smsrouter_srv_ind(MSG *m)
{

  u16  dlg_id = 0;              /* Dialogue id */
  u16  ptype = 0;               /* Parameter Type */
  u8   *pptr;                   /* Parameter Pointer */
  dlg_info *dlg_info;           /* State info for dialogue */
  u8   send_abort = 0;          /* Set if abort to be generated */
  int  invoke_id = 0;           /* Invoke id of received srv req */
  int  length = 0;

  //in MAP could be component type with two octets
  u16 cpt_type;		// Type of the incoming service indication 
  
  dlg_id = m->hdr.id;

  pptr = get_param(m);
  ptype = *pptr;

  /*
   * Get state information associated with this dialogue
   */
  dlg_info = get_dialogue_info(dlg_id);

  if (dlg_info == 0)
    return 0;

          cpt_type = MTR_get_primitive_type(m);

  switch (dlg_info->state)

      {

      case DLG_OPEN:

	  //return result for SRI_SM received
	  if (cpt_type == MAPST_SND_RTISM_IND)
	      {  
                    printf("MTR Rx: Received Send Routing Info for SMS Indication\n");

	  printf("funciton %s cpt type = %d\n", __PRETTY_FUNCTION__, cpt_type);

	      //MAPST_SND_RTISM_IND 

	  //	      exit(0);
	  /* here should be function like smsrouter_handle_invoke(ic dl id, dlg ptr, h ) */

	  //int mlen;

	  //mlen=m->len;
	  dlg_info->type = IN_SMSC_SRI_SM;

	  dlg_info->map_data.num_bytes = m->len;

    u8 *pptr = get_param(m);

    memcpy(&dlg_info->map_data.data[0], pptr, m->len);

         //handle_dlg_service();

         dlg_info->service_handler = &handle_dlg_service_srism_ind;

	      }

	  printf("received srv ind length = %d\n", dlg_info->map_data.num_bytes);



                dlg_info->state = DLG_PENDING_DELIMIT;

	  break; 

}




    return 0;

}


/*
 * smsrouter_ent
 *
 * Waits in a continuous loop for incoming messages from GCT environment
 *
 * Never returns.
 */
int smsrouter_ent(int fd, u8 mtr_id, u8 map_id, u8 trace, u8 dlg_term_mode)
//  u8 mtr_id;       /* Module id for this task */
//  u8 map_id;       /* Module ID for MAP */
//  u8 trace;        /* Trace requirements */
//  u8 dlg_term_mode;/* Default termination mode */
{
    //    int fd1[2]; //pipes descriptorw with hlr_agent
  HDR *h;               /* received message */
  HDR h_recv;
  MSG *m;               /* received message */
  //nbytes = read(fd[0], readbuffer, sizeof(readbuffer));
  int nbytes;
  pid_t pid;

  MTR_cfg(mtr_id, map_id, trace, dlg_term_mode);

  /*
   * Print banner so we know what's running.
   */
  printf("MTR MAP Test Responder (C) Dialogic Corporation 1999-2015. All Rights Reserved.\n");
  printf("===============================================================================\n\n");
  printf("MTR mod ID - 0x%02x; MAP module Id 0x%x; Termination Mode 0x%x\n", mtr_mod_id, mtr_map_id, dlg_term_mode);
  if ( mtr_trace == 0 )
    printf(" Tracing disabled.\n\n");


  if (pipe(fd1) < 0)
      {
	  exit(0);
      }

//no need to use hlr_agent process 06.09.2018

  //should fork for standalone hlr_agent process here

 //  if (   ( pid = fork() ) < 0 )
  //     {
   //	  exit(0);
    //   }

  // else if ( pid == 0 ) /*child process */

    //   {
     // close(fd1[1]); //close writing in child

//	  smsrouter_ent(fd[0], mtr_mod_id, mtr_map_id, mtr_trace, mtr_dlg_term_mode);
      // hlr_agent(fd1[0]);
//}

   //process in parent process

  // close(fd1[0]);

  /*
   * Now enter main loop, receiving messages as they
   * become available and processing accordingly.
   */

  while (1)
  {
      //    if ((h = GCT_receive(mtr_mod_id)) != 0)
      //      if ( (nbytes = read(fd, h, sizeof(int)) ) >= 0 )
      //  {
      nbytes = read(fd, &h, sizeof(int));
	      if (nbytes == -1)
		  {
		      perror("read:");
		      printf("received in child h = %p\n", h);
		      exit(0);
		  }

	      printf("received in child h = %p\n", h);
	      printf("received nbytes in child  = %d\n", nbytes);
      m = (MSG *)h;
      if (m != NULL)
      {
        MTR_trace_msg("MTR Rx:", m);

        switch (m->hdr.type)
        {
          case MAP_MSG_DLG_IND:
	      smsrouter_dlg_ind(m);
	      break;

          case MAP_MSG_SRV_IND:
	      //            MTR_process_map_msg(m);
	      smsrouter_srv_ind(m);
          break;
        }

        /*
         * Once we have finished processing the message
         * it must be released to the pool of messages.
         */
        relm(h);
      }
      //}
  }
  return(0);
}

/*
 * Can be used to configure and initialise mtr
 */
int MTR_cfg(
  u8 _mtr_mod_id, 
  u8 _map_mod_id, 
  u8 _trace_mod_id,
  u8 _dlg_term_mode
  ){
  mtr_mod_id = _mtr_mod_id;
  mtr_map_id = _map_mod_id;
  mtr_trace = _trace_mod_id;
  mtr_default_dlg_term_mode = _dlg_term_mode;

  //  init_resources();
  return 0;
}

/*
 * Can be used to configure new termination mode
 */
int MTR_set_default_term_mode(
  u8 new_mode
  ){
    mtr_default_dlg_term_mode = new_mode;    

    return (0);
  } 

/*
 * Get Dialogue Info
 *
 * Returns pointer to dialogue info or 0 on error.
 */
dlg_info *get_dialogue_info(dlg_id)
  u16 dlg_id;               /* Dlg ID of the incoming message 0x800a perhaps */
{
  u16 dlg_ref;              /* Internal Dlg Ref, 0x000a perhaps */

  if (!(dlg_id & 0x8000) )
  {
    if (mtr_trace)
      printf("MTR Rx:  Outgoing dialogue id, dlg_id == %x\n",dlg_id);
    // return 0;
    dlg_ref = dlg_id;
    return &dlg_info_out[dlg_ref];
  }
  else
  {
    dlg_ref = dlg_id & 0x7FFF;
  }

  if ( dlg_ref >= MAX_NUM_DLGS )
  {
    if (mtr_trace)
      printf("MTR Rx: Bad dialogue id: Out of range dialogue, dlg_id == %x\n",dlg_id);
    return 0;
  }

  return &dlg_info_in[dlg_ref];
}


    /*
     * MTU_get_dlg_data returns the dlg data structure.
     */
    dlg_info *MTU_get_dlg_data(u16 dlg_id)
    {
       return (&(dlg_info_out[dlg_id - hlr_o_base_dlg_id]));
    }




/*
 * MTR_process_map_msg
 *
 * Processes a received MAP primitive message.
 *
 * Always returns zero.
 */
int MTR_process_map_msg(m)
  MSG *m;                       /* Received message */
{
  u16  dlg_id = 0;              /* Dialogue id */
  u16  ptype = 0;               /* Parameter Type */
  u8   *pptr;                   /* Parameter Pointer */
  dlg_info *dlg_info;           /* State info for dialogue */
  u8   send_abort = 0;          /* Set if abort to be generated */
  int  invoke_id = 0;           /* Invoke id of received srv req */
  int  length = 0;
  
  dlg_id = m->hdr.id;

  pptr = get_param(m);
  ptype = *pptr;

  /*
   * Get state information associated with this dialogue
   */
  dlg_info = get_dialogue_info(dlg_id);

  if (dlg_info == 0)
    return 0;

  switch (dlg_info->state)
  {
    case DLG_IDLE :
      /*
       * Null state.
       */
      switch (m->hdr.type)
      {
        case MAP_MSG_DLG_IND :
          ptype = *pptr;

          switch (ptype)
          {
            case MAPDT_OPEN_IND :
              /*
               * Open indication indicates that a request to open a new
               * dialogue has been received
               */
              if (mtr_trace)
                printf("MTR Rx: Received Open Indication\n");

              /*
               * Save application context and MAP instance
               * We don't do actually do anything further with it though.
               */
              dlg_info->map_inst = (u16)GCT_get_instance((HDR*)m);

              /*
               * Set the termination mode based on the current default
               */
              dlg_info->term_mode = mtr_default_dlg_term_mode;
              dlg_info->mms = 0;
              dlg_info->ac_len = 0;

              length = MTR_get_applic_context(m, dlg_info->app_context, MTR_MAX_AC_LEN);
              if (length > 0)
                dlg_info->ac_len = (u8)length;

              if (dlg_info->ac_len != 0)
              {
                /*
                 * Respond to the OPEN_IND with OPEN_RSP and wait for the
                 * service indication
                 */
                MTR_send_OpenResponse(dlg_info->map_inst, dlg_id, MAPRS_DLG_ACC);
		// dlg_info->state = MTR_S_WAIT_FOR_SRV_PRIM;
		 dlg_info->state = DLG_OPEN;
              }
              else
              {
                /*
                 * We do not have a proper Application Context - abort
                 * the dialogue
                 */
                send_abort = 1;
              }
              break;

            default :
              /*
               * Unexpected event - Abort the dialogue.
               */
              send_abort = 1;
              break;
          }
          break;

        default :
          /*
           * Unexpected event - Abort the dialogue.
           */
          send_abort = 1;
          break;
      }
      break;

      //case S_WAIT_FOR_SRV_PRIM :
  case DLG_OPEN:
      /*
       * Waiting for service primitive
       */
      switch (m->hdr.type)
      {
        case MAP_MSG_SRV_IND :
          /*
           * Service primitive indication
           */
          ptype = MTR_get_primitive_type(m);

          switch (ptype)
          {
            case MAPST_FWD_SM_IND :
            case MAPST_SEND_IMSI_IND :
            case MAPST_SND_RTIGPRS_IND :
            case MAPST_MT_FWD_SM_IND:
            case MAPST_SND_RTISM_IND:
            case MAPST_PRO_UNSTR_SS_REQ_IND :
            case MAPST_UNSTR_SS_REQ_CNF :
            case MAPST_UNSTR_SS_REQ_IND :
            case MAPST_UNSTR_SS_NOTIFY_IND :
            case MAPST_ANYTIME_INT_IND :
              if (mtr_trace)
              {
                switch (ptype)
                {
                  case MAPST_FWD_SM_IND :
                    printf("MTR Rx: Received Forward Short Message Indication\n");
                    break;
                  case MAPST_MT_FWD_SM_IND :
                    printf("MTR Rx: Received MT Forward Short Message Indication\n");
                    break;
                  case MAPST_SEND_IMSI_IND :
                    printf("MTR Rx: Received Send IMSI Indication\n");
                    break;
                  case MAPST_SND_RTIGPRS_IND :
                    printf("MTR Rx: Received Send Routing Info for GPRS Indication\n");
                    break;
                  case MAPST_SND_RTISM_IND :
                    printf("MTR Rx: Received Send Routing Info for SMS Indication\n");
                    break;
                  case MAPST_PRO_UNSTR_SS_REQ_IND :
                    printf("MTR Rx: Received ProcessUnstructuredSS-Indication\n");
                    break;
                  case MAPST_UNSTR_SS_REQ_CNF :
                    printf("MTR Rx: Received UnstructuredSS-Req-Confirmation\n");
                    break;
                  case MAPST_UNSTR_SS_REQ_IND :
                    printf("MTR Rx: Received UnstructuredSS-Indication\n");
                    break;
                  case MAPST_UNSTR_SS_NOTIFY_IND :
                    printf("MTR Rx: Received UnstructuredSS-Notify Indication\n");
                    break;
                  case MAPST_ANYTIME_INT_IND :
                    printf("MTR Rx: Received AnyTimeInterrogation Indication\n");
                    break;
                  default :
                    send_abort = 1; 
                  break;

                }
              }

              /*
               * Recover invoke id. The invoke id is used
               * when sending the Forward short message response.
               */
              invoke_id = MTR_get_invoke_id(m);

              /*
               * If recovery of the invoke id succeeded, save invoke id and
               * primitive type and change state to wait for the delimiter.
               */
              if (invoke_id != -1)
              {
                dlg_info->invoke_id = (u8)invoke_id;
                dlg_info->ptype = ptype;
                dlg_info->mms = 0;

                /*
                 * Store MSISDN if available for use with ATI Response test data lookup
                 */
                if (ptype == MAPST_ANYTIME_INT_IND)
                {
                  dlg_info->msisdn_len = MTR_get_msisdn(m, dlg_info->msisdn, MTR_MAX_MSISDN_SIZE);
                  dlg_info->ati_req_info = MTR_get_ati_req_info(m);
                }

                if (ptype == MAPST_FWD_SM_IND || ptype == MAPST_MT_FWD_SM_IND)
                {
                  /*
                   * For FWD_SM, check for MMS parameter and store status
                   */
                  if (MTR_get_parameter(m, MAPPN_more_msgs, NULL) == 0)
                    dlg_info->mms = 1;

                  if (mtr_trace)
                    print_sh_msg(m);
                }

                dlg_info->state = DLG_PENDING_DELIMIT;
                break;
              }
              else
              {
                printf("MTR RX: No invoke ID included in the message\n");
              }
              break;

            default :
                send_abort = 1;
              break;
          }
          break;

        case MAP_MSG_DLG_IND :
          /*
           * Dialogue indication - we were not expecting this!
           */
          ptype = *pptr;

          switch (ptype)
          {
            case MAPDT_NOTICE_IND :
              /*
               * MAP-NOTICE-IND indicates some kind of error. Close the
               * dialogue and idle the state machine.
               */
              if (mtr_trace)
                printf("MTR Rx: Received Notice Indication\n");

              /*
               * Now send Map Close and go to idle state.
               */
              MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
              dlg_info->state = DLG_IDLE;
              send_abort = 0;
              break;

            case MAPDT_CLOSE_IND :
              /*
               * Close indication received.
               */
              if (mtr_trace)
                printf("MTR Rx: Received Close Indication\n");
              dlg_info->state = DLG_IDLE;
              send_abort = 0;
              break;

            default :
              /*
               * Unexpected event - Abort the dialogue.
               */
              send_abort = 1;
              break;
          }
          break;

        default :
          /*
           * Unexpected event - Abort the dialogue.
           */
          send_abort = 1;
          break;
      }
      break;

    case DLG_PENDING_DELIMIT :
      /*
       * Wait for delimiter.
       */
      switch (m->hdr.type)
      {
        case MAP_MSG_DLG_IND :
          ptype = *pptr;

          switch (ptype)
          {
            case MAPDT_DELIMITER_IND :
              /*
               * Delimiter indication received. Now send the appropriate
               * response depending on the service primitive that was received.
               */
              if (mtr_trace)
                printf("MTR Rx: Received delimiter Indication\n");

              switch (dlg_info->ptype)
              {
                case MAPST_FWD_SM_IND :
                  MTR_ForwardSMResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id);
                  /*
                   * If sender passed More Message to Send (MMS), keep the dialog open
                   */
                  if (dlg_info->mms)
                  {
                    MTR_send_Delimit(dlg_info->map_inst, dlg_id);
                    dlg_info->state = DLG_OPEN;
                  }
                  else
                  {
                    MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                    dlg_info->state = DLG_IDLE;
                  }
                  send_abort = 0;
                  break;

                case MAPST_MT_FWD_SM_IND :
                  MTR_MT_ForwardSMResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  /*
                   * If sender passed More Message to Send (MMS), keep the dialog open
                   */
                  if (dlg_info->mms)
                  {
                    MTR_send_Delimit(dlg_info->map_inst, dlg_id);
                    dlg_info->state = DLG_OPEN;
                  }
                  else
                  {
                    MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                    dlg_info->state = DLG_IDLE;
                  }
                  send_abort = 0;
                  break;

                case MAPST_SEND_IMSI_IND :
                  MTR_SendImsiResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                  dlg_info->state = DLG_IDLE;
                  send_abort = 0;
                  break;

                case MAPST_SND_RTIGPRS_IND :
                  MTR_SendRtgInfoGprsResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                  dlg_info->state = DLG_IDLE;
                  send_abort = 0;
                  break;

                case MAPST_SND_RTISM_IND :
                  MTR_SendRtgInfoSmsResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                  dlg_info->state = DLG_IDLE;
                  send_abort = 0;
                  break;

                case MAPST_PRO_UNSTR_SS_REQ_IND :
                  MTR_Send_UnstructuredSSRequest (dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_Delimit(dlg_info->map_inst, dlg_id);
                  dlg_info->state = DLG_OPEN;
                  send_abort = 0;
                  break;

                case MAPST_UNSTR_SS_REQ_CNF :
                  MTR_Send_ProcessUnstructuredSSReqRsp (dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                  dlg_info->state = DLG_IDLE;
                  send_abort = 0;
                  break;

                case MAPST_UNSTR_SS_REQ_IND :
                  MTR_Send_UnstructuredSSResponse (dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_Delimit(dlg_info->map_inst, dlg_id);  
                  dlg_info->state = DLG_OPEN;
                  send_abort = 0;
                  break;

                case MAPST_UNSTR_SS_NOTIFY_IND :
                  MTR_Send_UnstructuredSSNotifyRsp (dlg_info->map_inst, dlg_id, dlg_info->invoke_id);
                  
                  if ((dlg_info->term_mode == DLG_TERM_MODE_AUTO) || (dlg_info->term_mode == DLG_TERM_MODE_LOCAL_CLOSE))
                  {
                    MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                    dlg_info->state = DLG_IDLE;               
                  }
                  else
                  {
                    MTR_send_Delimit(dlg_info->map_inst, dlg_id);  
                    dlg_info->state = DLG_OPEN;
                  } 
                  send_abort = 0;

                  break;

                case MAPST_ANYTIME_INT_IND:
                  MTR_Send_ATIResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id);

                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);  
                  dlg_info->state = DLG_IDLE;
                  send_abort = 0;
                  break;
              }
                break;

            default :
              /*
               * Unexpected event - Abort the dialogue
               */
              send_abort = 1;
              break;
          }
          break;

        default :
          /*
           * Unexpected event - Abort the dialogue
           */
            send_abort = 1;
          break;
      }
      break;
  }
  /*
   * If an error or unexpected event has been encountered, send abort and
   * return to the idle state.
   */
  if (send_abort)
  {
    MTR_send_Abort (dlg_info->map_inst, dlg_id, MAPUR_procedure_error);
    dlg_info->state = DLG_IDLE;
  }
  return(0);
}

/******************************************************************************
 *
 * Functions to send primitive requests to the MAP module
 *
 ******************************************************************************/

/*
 * MTR_send_OpenResponse
 *
 * Sends an open response to MAP
 *
 * Returns zero or -1 on error.
 */
static int MTR_send_OpenResponse(instance, dlg_id, result)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  result;          /* Result (accepted/rejected) */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Open Response\n");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, (u16)(7 + dlg_info->ac_len))) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;
    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Open response
     * Parameter name   = result_tag
     * Parameter length = 1
     * Parameter value  = result
     * Parameter name   = applic_context_tag
     * parameter length = len
     * parameter data   = applic_context
     * EOC_tag
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_OPEN_RSP;
    pptr[1] = MAPPN_result;
    pptr[2] = 0x01;
    pptr[3] = result;
    pptr[4] = MAPPN_applic_context;
    pptr[5] = (u8)dlg_info->ac_len;
    memcpy((void*)(pptr+6), (void*)dlg_info->app_context, dlg_info->ac_len);
    pptr[6+dlg_info->ac_len] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return 0;
}

/*
 * MTR_ForwardSMResponse
 *
 * Sends a forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_ForwardSMResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Forward SM Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Forward SM response
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_FWD_SM_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_SendRtgInfoGprsResponse
 *
 * Sends a send routing info for GPRS response to MAP.
 *
 * Always returns zero.
 */
static int MTR_SendRtgInfoGprsResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send Routing Info for GPRS Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 12)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Send Routing Info for GPRS response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = SGSN address
     * Parameter length = 4
     * Parameter value:
     *   1st octet:  address type = IPv4; addres length = 4
     *   remaining octets = 193.195.185.113
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_SND_RTIGPRS_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_sgsn_address;
    pptr[5] = 5;
    pptr[6] = 4;
    pptr[7] = 193;
    pptr[8] = 195;
    pptr[9] = 185;
    pptr[10] = 113;
    pptr[11] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_SendImsiResponse
 *
 * Sends a forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_SendImsiResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send IMSI Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 14)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = send IMSI response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Primitive name = IMSI
     * Parameter length = 7
     * Parameter value = 60802678000454

     * Parameter name = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_SEND_IMSI_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_imsi;
    pptr[5] = 7;
    pptr[6] = 0x06;
    pptr[7] = 0x08;
    pptr[8] = 0x62;
    pptr[9] = 0x87;
    pptr[10] = 0x00;
    pptr[11] = 0x40;
    pptr[12] = 0x45;
    pptr[13] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_MT_ForwardSMResponse
 *
 * Sends a MT forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_MT_ForwardSMResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending MT Forward SM Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = MT Forward SM response
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_MT_FWD_SM_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* 
 * MTR_SendRtgInfoSmsResponse
 *
 * Sends a send routing info for SMS response to MAP.
 *
 * Always returns zero.
 */
static int MTR_SendRtgInfoSmsResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send Routing Info for SMS Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 19)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Send Routing Info for SMS response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = IMSI
     * Parameter length = 3
     * Parameter value:
     *   remaining octets = 0x1 0x2 0x3
     *
     * Parameter name = MSC Number
     * Parameter length = 7 
     * Parameter value:
     *   1st Octet gives format information as per 29.002 AddressString
     *   remaining octets = 44 12 34 56 78 
     *
     * Parameter name   = terminator (0x00)
     */
    pptr = get_param(m);
    pptr[0] = MAPST_SND_RTISM_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_imsi;
    pptr[5] = 3;
    pptr[6] = 1;
    pptr[7] = 2;
    pptr[8] = 3;
    pptr[9] = MAPPN_msc_num;
    pptr[10] = 7;
    pptr[11] = 0x91;
    pptr[12] = 0x44;
    pptr[13] = 0x21;
    pptr[14] = 0x43;
    pptr[15] = 0x65;
    pptr[16] = 0x87;
    pptr[17] = 0x09;
    pptr[18] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* MTR_Send_UnstructuredSSRequest
 * Formats and sends an UnstructuredSS-Request message
 * in response to a received ProcessUnstructuredSS-Request.
 */

 static int MTR_Send_UnstructuredSSRequest(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send_UnstructuredSS-Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 47)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Send_UnstructuredSS-Request
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = USSD Data Coding Scheme
     * Parameter length = 1
     * Parameter value  = 'GSM default alphabet' 00001111
     *
     * Parameter name = USSD String
     *
     * Use the MTU function 'MTU_USSD_str_to_def_alph' to verify string encoding
     * USSD string below = 'XY Telecom <LF>'
     *                     '1. Balance <LF>'
     *                     '2. Texts Remaining'
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_UNSTR_SS_REQ_REQ;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_USSD_coding;
    pptr[5] = 0x01;   
    pptr[6] = 0x0F;  /* USSD coding set to 'GSM default alphabet' 00001111 */  
    pptr[7] = MAPPN_USSD_string;
    pptr[8] = 37;
    pptr[9] = 0xD8;
    pptr[10] = 0x2C;
    pptr[11] = 0x88;
    pptr[12] = 0x5A;
    pptr[13] = 0x66;
    pptr[14] = 0x97;
    pptr[15] = 0xC7;
    pptr[16] = 0xEF;
    pptr[17] = 0xB6;
    pptr[18] = 0x02;
    pptr[19] = 0x14;
    pptr[20] = 0x73;
    pptr[21] = 0x81;
    pptr[22] = 0x84;
    pptr[23] = 0x61;
    pptr[24] = 0x76;
    pptr[25] = 0xD8;
    pptr[26] = 0x3D;
    pptr[27] = 0x2E;
    pptr[28] = 0x2B;
    pptr[29] = 0x40;
    pptr[30] = 0x32;
    pptr[31] = 0x17;
    pptr[32] = 0x88;
    pptr[33] = 0x5A;
    pptr[34] = 0xC6;
    pptr[35] = 0xD3;
    pptr[36] = 0xE7;
    pptr[37] = 0x20;
    pptr[38] = 0x69;
    pptr[39] = 0xB9;
    pptr[40] = 0x1D;
    pptr[41] = 0x4E;
    pptr[42] = 0xBB;
    pptr[43] = 0xD3;
    pptr[44] = 0xEE;
    pptr[45] = 0x73;
    pptr[46] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* MTR_Send_UnstructuredSSResponse
 * Formats and sends a UnstructuredSS Response message
 * in response to a received UnstructuredSS-Request-Ind.
 */

 static int MTR_Send_UnstructuredSSResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send_UnstructuredSS-Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 27)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = MAPST_UNSTR_SS_IND_RSP
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = USSD Data Coding Scheme
     * Parameter length = 1
     * Parameter value  = 'GSM default alphabet' 00001111
     *
     * Parameter name = USSD String
     *
     * Use the MTU function 'MTU_USSD_str_to_def_alph' to verify string encoding
     * USSD string below = 'This is sample text'
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_UNSTR_SS_REQ_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_USSD_coding;
    pptr[5] = 0x01;   
    pptr[6] = 0x0F;  /* USSD coding set to 'GSM default alphabet' 00001111 */  
    pptr[7] = MAPPN_USSD_string;
    pptr[8] = 17; 
    pptr[9] = 0x54;
    pptr[10] = 0x74;
    pptr[11] = 0x7a;
    pptr[12] = 0x0e;
    pptr[13] = 0x4a;
    pptr[14] = 0xcf;
    pptr[15] = 0x41;
    pptr[16] = 0xf3;
    pptr[17] = 0x70;
    pptr[18] = 0x1b;
    pptr[19] = 0xce;
    pptr[20] = 0x2e;
    pptr[21] = 0x83;
    pptr[22] = 0xe8;
    pptr[23] = 0x65;
    pptr[24] = 0x3c;
    pptr[25] = 0x1d;

    pptr[26] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* MTR_Send_ProcessUnstructuredSSReq-Rsp
 * Formats and sends a ProcessUnstructuredSSReq-Rsp message
 * in response to a received UnstructuredSS-Request-CNF.
 */

 static int MTR_Send_ProcessUnstructuredSSReqRsp(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send_UnstructuredSS-Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 26)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = ProcessUnstructuredSSReq-Rsp
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = USSD Data Coding Scheme
     * Parameter length = 1
     * Parameter value  = 'GSM default alphabet' 00001111
     *
     * Parameter name = USSD String
     *
     * Use the MTU function 'MTU_USSD_str_to_def_alph' to verify string encoding
     * USSD string below = 'Your balance = 350'
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_PRO_UNSTR_SS_REQ_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_USSD_coding;
    pptr[5] = 0x01;   
    pptr[6] = 0x0F;  /* USSD coding set to 'GSM default alphabet' 00001111 */  
    pptr[7] = MAPPN_USSD_string;
    pptr[8] = 16;
    pptr[9] = 0xD9;
    pptr[10] = 0x77;
    pptr[11] = 0x5D;
    pptr[12] = 0x0E;
    pptr[13] = 0x12;
    pptr[14] = 0x87;
    pptr[15] = 0xD9;
    pptr[16] = 0x61;
    pptr[17] = 0xF7;
    pptr[18] = 0xB8;
    pptr[19] = 0x0C;
    pptr[20] = 0xEA;
    pptr[21] = 0x81;
    pptr[22] = 0x66;
    pptr[23] = 0x35;
    pptr[24] = 0x58;
    pptr[25] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}


/* MTR_Send_UnstructuredSSNotifyRsp-Rsp
 * Formats and sends a UnstructuredSS Notify-Rsp message
 * in response to a received UnstructuredSS-Notify-IND.
 */
 static int MTR_Send_UnstructuredSSNotifyRsp(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending UnstructuredSS-Notify Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_UNSTR_SS_REQ_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_Send_ATIResponse
 *
 * Sends a forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_Send_ATIResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  u8   len = 0;
  u8   ati_index = 0;

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending ATI Response\n\r");

  /*
   * See if we have MSISDN to use as key into look-up.
   */
  if (dlg_info->msisdn_len != 0) 
  {
    /*
     * Find the last digit and use as index into sample data.
     */
    if ((dlg_info->msisdn[dlg_info->msisdn_len - 1] >> 4) == 0xf)
        ati_index = ((dlg_info->msisdn[dlg_info->msisdn_len - 1]) & 0xf);
    else
        ati_index = (dlg_info->msisdn[dlg_info->msisdn_len - 1] >> 4);
    
    if (ati_index >= MTR_ATI_RSP_NUM_OF_RSP)
       ati_index = 0;
  }

  if (mtr_trace)
    printf("Using ATI sample data index %i\n", (int)ati_index);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 15)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = send ATI response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = terminator
     */
    pptr = get_param(m);
    pptr[len++] = MAPST_ANYTIME_INT_RSP;
    pptr[len++] = MAPPN_invoke_id;
    pptr[len++] = 0x01;
    pptr[len++] = invoke_id;

    if (dlg_info->ati_req_info & 0x01)  /* Request type is location */
    {
      pptr[len++] = MAPPN_geog_info;
      pptr[len++] = 0x8;
      memcpy(&pptr[len], mtr_ati_rsp_data[ati_index], 8);

      /*
       * Length is always 8
       */
      len += 8;
    }
    if (dlg_info->ati_req_info & 0x02)  /* Request type is subscriber state */
    {
      pptr[len++] = MAPPN_sub_state;
      pptr[len++] = 0x1;
      pptr[len++] = 0x00;  /* Assumed Idle */
    }

    pptr[len++] = 0x00;

    /*
     * Reset message length
     */
    m->len = len;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_send_MapClose
 *
 * Sends a Close message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_MapClose(instance, dlg_id, method)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  method;          /* Release method */
{
  MSG  *m;                   /* Pointer to message to transmit */
  u8   *pptr;                /* Pointer to a parameter */
  dlg_info *dlg_info;        /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Close Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Close Request
     * Parameter name   = release method tag
     * Parameter length = 1
     * Parameter value  = release method
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_CLOSE_REQ;
    pptr[1] = MAPPN_release_method;
    pptr[2] = 0x01;
    pptr[3] = method;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return(0);
}

/*
 * MTR_send_Abort
 *
 * Sends an abort message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_Abort(instance, dlg_id, reason)
  u16 instance;         /* Destination instance */
  u16 dlg_id;           /* Dialogue id */
  u8  reason;           /* user reason for abort */
{
  MSG  *m;              /* Pointer to message to transmit */
  u8   *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info;   /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending User Abort Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Close Request
     * Parameter name   = user reason tag
     * Parameter length = 1
     * Parameter value  = reason
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_U_ABORT_REQ;
    pptr[1] = MAPPN_user_rsn;
    pptr[2] = 0x01;
    pptr[3] = reason;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return(0);
}

/*
 * MTR_send_Delimit
 *
 * Sends a Delimit message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_Delimit(instance, dlg_id)
  u16 instance;         /* Destination instance */
  u16 dlg_id;           /* Dialogue id */
{
  MSG  *m;              /* Pointer to message to transmit */
  u8   *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info;   /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Delimit \n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 2)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Delimit Request
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_DELIMITER_REQ;
    pptr[1] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return(0);
}

/*
 * MTR_send_msg sends a MSG. On failure the
 * message is released and the user notified.
 *
 * Always returns zero.
 */
static int MTR_send_msg(instance, m)
  u16   instance;       /* Destination instance */
  MSG   *m;             /* MSG to send */
{
  GCT_set_instance((unsigned int)instance, (HDR*)m);

  MTR_trace_msg("MTR Tx:", m);

  /*
   * Now try to send the message, if we are successful then we do not need to
   * release the message.  If we are unsuccessful then we do need to release it.
   */

  if (GCT_send(m->hdr.dst, (HDR *)m) != 0)
  {
    if (mtr_trace)
      fprintf(stderr, "*** failed to send message ***\n");
    relm((HDR *)m);
  }
  return(0);
}


/******************************************************************************
 *
 * Functions to recover parameters from received MAP format primitives
 *
 ******************************************************************************/

/*
 * MTR_get_invoke_id
 *
 * recovers the invoke id parameter from a parameter array
 *
 * Returns the recovered value or -1 if not found.
 */
static int MTR_get_invoke_id(MSG *m)
{
  int  invoke_id = -1;   /* Recovered invoke_id */
  u8  *found_pptr;

  if (MTR_get_parameter(m, MAPPN_invoke_id, &found_pptr) == 1)
    invoke_id = (int)*found_pptr;

  return(invoke_id);
}

/*
 * MTR_get_applic_context
 *
 * Recovers the Application Context parameter from a parameter array
 *
 * Returns the length of parameter data recovered (-1 on failure).
 */
static int MTR_get_applic_context(m, dst, dstlen)
  MSG *m;       /* Message */
  u8  *dst;     /* Start of destination for recovered ac */
  u16 dstlen;   /* Space available at dst */
{
  int  retval = -1;  /* Return value */
  int  length;
  u8  *found_pptr;

  length = MTR_get_parameter(m, MAPPN_applic_context, &found_pptr);
  if (length > 0 && length <= dstlen)
  {
    memcpy(dst, found_pptr, length);
    retval = length;
  }

  return(retval);
}

/*
 * MAP_get_destination_address
 *
 * Recovers the Destination Address parameter from a parameter array
 *
 * Returns the length of parameter data recovered (-1 on failure).
 */
static int MAP_get_destination_address(m, dst, dstlen)
  MSG *m;       /* Message */
  u8  *dst;     /* Start of destination for recovered ac */
  u16 dstlen;   /* Space available at dst */
{
  int  retval = -1;  /* Return value */
  int  length;
  u8  *found_pptr;

  length = MTR_get_parameter(m, MAPPN_dest_address, &found_pptr);
  if (length > 0 && length <= dstlen)
  {
    memcpy(dst, found_pptr, length);
    retval = length;
  }

  return(retval);
}



/*
 * MAP_get_origin_address
 *
 * Recovers the Originating Address parameter from a parameter array
 *
 * Returns the length of parameter data recovered (-1 on failure).
 */
static int MAP_get_origin_address(MSG *m, u8 *dst, u16 dstlen)
//  MSG *m;       /* Message */
//  u8  *dst;     /* Start of destination for recovered ac */
//  u16 dstlen;   /* Space available at dst */
{
  int  retval = -1;  /* Return value */
  int  length;
  u8  *found_pptr;

  length = MTR_get_parameter(m, MAPPN_orig_address, &found_pptr);
  if (length > 0 && length <= dstlen)
  {
    memcpy(dst, found_pptr, length);
    retval = length;
  }

  return(retval);
}


/*
 * MTR_get_msisdn
 *
 * Recovers the MSISDN parameter from a parameter array
 *
 * Returns the length of parameter data recovered (0 on failure).
 */
static u8 MTR_get_msisdn(m, dst, dstlen)
  MSG *m;       /* Message */
  u8  *dst;     /* Start of destination for recovered param */
  u16 dstlen;   /* Space available at dst */
{
  u8  retval = 0;   /* Return value */
  int  length;
  u8  *found_pptr;

  length = MTR_get_parameter(m, MAPPN_msisdn, &found_pptr);

  if (length > 0 && length <= dstlen && length <= 0xff)
  {
    memcpy(dst, found_pptr, length);
    retval = (u8)length;
  }
  return(retval);
}




/*
 * MTR_get_ati_req_info
 *
 * Recovers the Req Info parameter from a parameter array
 *
 * Returns the length of parameter data recovered (0 on failure).
 */
static u8 MTR_get_ati_req_info(MSG *m)
{
  u8  retval = 0;   /* Return value */
  int  length;
  u8  *found_pptr;

  length = MTR_get_parameter(m, MAPPN_req_info, &found_pptr);

  if (length == 1)
  {
    retval = *found_pptr;
  }
  return(retval);
}

/*
 * MTR_get_sh_msg
 *
 * recovers the short message parameter from a parameter array
 *
 * Returns the length of the recovered data or -1 if error.
 */
static int MTR_get_sh_msg(m, dst, dstlen)
  MSG *m;       /* Message */
  u8  *dst;     /* Start of destination for recovered SM */
  u16 dstlen;   /* Space available at dst */
{
  int  retval = -1;   /* Return value */
  int  length;
  u8  *found_pptr;

  length = MTR_get_parameter(m, MAPPN_sm_rp_ui, &found_pptr);

  if (length >= 0 && length <= dstlen)
  {
    memcpy(dst, found_pptr, length);
    retval = length;
  }

  return(retval);
}

/*
 * MTR_get_parameter()
 *
 * Looks for the given parameter in the message data.
 *
 * Returns parameter length
 *         or -1 if parameter was not found
 */
static int MTR_get_parameter(MSG *m, u16 pname, u8 **found_pptr)
{
  u8   *pptr;
  int   mlen;
  u16   name;     /* Parameter name */
  u16   plen;     /* Parameter length */
  u8   *end_pptr;
  u8    code_shift = 0;

  pptr = get_param(m);
  mlen = m->len;

  if (mlen < 1)
    return(-1);

  /*
   * Skip past primitive type octet
   */
  pptr++;
  mlen--;

  end_pptr = pptr + mlen;

  while (pptr < end_pptr)
  {
    name = *pptr++;
    if (name == MAPPN_LONG_PARAM_CODE_EXTENSION)
    {
      /*
       * Process extended parameter name format data
       *
       * Skip first length octet(s)
       */
      pptr++;
      if (code_shift)
        pptr++;

      /*
       * Read 2 octet parameter name
       */
      name  = *pptr++ << 8;
      name |= *pptr++;
    }

    /*
     * Read parameter length from 1 or 2 octets
     */
    if (code_shift)
    {
      plen  = *pptr++ << 8;
      plen |= *pptr++;
    }
    else
    {
      plen = *pptr++;
    }

    /*
     * Test parameter name for match, end and code_shift
     */
    if (name == 0)
      break;

    if (name == MAPPN_CODE_SHIFT)
      code_shift = *pptr;

    if (name == pname)
    {
      /*
       * Just return the length if the given return data pointer is NULL
       */
      if (found_pptr != NULL)
        *found_pptr = pptr;
      return(plen);
    }

    pptr += plen;
  }
  return(-1);
}

/*
 * MTR_get_primitive_type()
 *
 * Looks for the primitive type of message.
 *
 * Returns primitive type
 *         or MAPST_EXTENDED_SERVICE_TYPE if a valid primitive type was not found
 */
static u16 MTR_get_primitive_type(MSG *m)
{
  u16   ptype = 0;
  int   len = 0;
  u8   *found_pptr;
  u8   *pptr;
  int   mlen;

  pptr = get_param(m);
  mlen = m->len;

  if (mlen < 1)
    return(-1);

  /*
   * Read first octet as primitive type
   */
  ptype = *pptr;

  if (ptype == MAPST_EXTENDED_SERVICE_TYPE)
  {
    len = MTR_get_parameter(m, MAPPN_SERVICE_TYPE, &found_pptr);
    if (len == 2)
    {
      ptype  = *found_pptr++ << 8;
      ptype |= *found_pptr++;
    }
  }
  return(ptype);
}

/*
 * MTR_trace_msg
 *
 * Traces (prints) any message as hexadecimal to the console.
 *
 * Always returns zero.
 */
static int MTR_trace_msg(prefix, m)
  char *prefix;
  MSG  *m;               /* received message */
{
  HDR   *h;              /* pointer to message header */
  int   instance;        /* instance of MAP msg received from */
  u16   mlen;            /* length of received message */
  u8    *pptr;           /* pointer to parameter area */

  /*
   * If tracing is disabled then return
   */
  if (mtr_trace == 0)
    return(0);

  h = (HDR*)m;
  instance = GCT_get_instance(h);
  printf("%s I%04x M t%04x i%04x f%02x d%02x s%02x", prefix, instance, h->type,
          h->id, h->src, h->dst, h->status);

  if ((mlen = m->len) > 0)
  {
    if (mlen > MAX_PARAM_LEN)
      mlen = MAX_PARAM_LEN;
    printf(" p");
    pptr = get_param(m);
    while (mlen--)
    {
      printf("%c%c", BIN2CH(*pptr/16), BIN2CH(*pptr%16));
      pptr++;
    }
  }
  printf("\n");
  return(0);
}

/*
 * init_resources
 *
 * Initialises all mtr system resources
 * This includes dialogue state information.
 *
 * Always returns zero
 *
 */
int smsrouter_init_res()
{
  int i;    /* for loop index */


  //incoming dialogues
  for (i=0; i<MAX_NUM_DLGS_IN; i++)
  {
    dlg_info_in[i].state = DLG_IDLE;
    dlg_info_in[i].term_mode = mtr_default_dlg_term_mode;
  }

  //outgoing dialogues

  for (i=0; i<MAX_NUM_DLGS_OUT; i++)
  {
    dlg_info_out[i].state = DLG_IDLE;
    dlg_info_out[i].term_mode = mtr_default_dlg_term_mode;
  }




  return 0;
}



/*
 * hlr_alloc_dlg_id allocates a dialogue ID to be used when opening a dialogue with HLR to send MAP_SRI_FOR_SM.
 *
 * Returns zero if a dialogue ID was allocated.
 *         -1   if no dialogue ID could be allocated.
 */
static u16 alloc_out_dlg_id(u16 *dlg_id_ptr)
//u16 *dlg_id_ptr;        /* updated to point to a free dialogue id */
{
  u16 i;                /* dialogue ID loop counter */
  int found;            /* has an idle dialogue been found? */

  found = 0;


  /*
   * Look for an idle dialogue id starting at out_dlg_id
   */
  for (i = out_dlg_id; i < (base_out_dlg_id + MAX_NUM_DLGS_OUT); i++)
  {
    if (dlg_info_out[i - base_out_dlg_id].state == DLG_IDLE)
    {
      found = 1;
      break;
    }
  }

  /*
   * If we haven't found one yet, start looking again, this time from the
   * base id.
   */
  if (found == 0)
  {
    for (i = base_out_dlg_id; i < out_dlg_id; i++)
    {
      if (dlg_info_out[i - base_out_dlg_id].state == DLG_IDLE)
      {
        found = 1;
        break;
      }
    }
  }

  if (found)
  {
    /*
     * Update the dialogue id to return and increment the active dialogue count
     */
    *dlg_id_ptr = i;
    dlg_active++;

    /*
     * Select the next dialogue id to start looking for an idle one.
     * If we've reached the end of the range then start from the base id.
     */
    if (out_dlg_id == (base_out_dlg_id + MAX_NUM_DLGS_OUT - 1))
      out_dlg_id = base_out_dlg_id;
    else
      out_dlg_id++;

    return 0;
  }
  else
  {
    /*
     * No idle dialogue id found
     */
    return (-1);
  }
} /* end of hlr_alloc_dlg_id() */



