#ifndef SMSROUTER_H
#define SMSROUTER_H


/*
 * General definitions
 */
#define MAX_NUM_DLGS                (2048)
#define MTR_MAX_MSISDN_SIZE         (32)

/*
 * Dialog termination mode
 * Auto is service specific
 * Local means close dialog on completion of service handling
 * Remote means let the far end control the termination of the dialog
 */
#define DLG_TERM_MODE_AUTO          (0)
#define DLG_TERM_MODE_LOCAL_CLOSE   (1)
#define DLG_TERM_MODE_REMOTE_CLOSE  (2)


#define MAX_PARAM_LEN               (320)
#define MTR_MAX_AC_LEN              (255) /* max length of app context */
#define MAX_SM_SIZE                 (200) /* max length of short message */
#define SIZE_UI_HEADER_FIXED        (13)  /* Short Message User Info header fixed part*/

/*
 * Local macros.
 */
#define NO_RESPONSE                 (0)
#define BIN2CH(b)                   ((char)(((b)<10)?'0'+(b):'a'-10+(b)))


/*
 * Alphabet conversion.
 * def_alph   ascii
 * 0x27 '->' (=39)
 *
 *
 * Any ascii characters without default alphabet symbols are replaced with
 * a '@'. N.B. Default alphabet 0x00 is a '@'.
 */
static u8 default_alphabet_to_ascii[0x80]= {
/*0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
 '@','£','$','@','@','@','@','@','@','@', 10,'@', 13,'@','@','@', /* 0x00 */
 '@','_','@','@','@','@','@','@','@','@','@', 32,'@','@','@','@', /* 0x10 */
 ' ','!','"','#','@','%','&', 39,'(',')','*','+',',','-','.','/', /* 0x20 */
 '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?', /* 0x30 */
 '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O', /* 0x40 */
 'P','Q','R','S','T','U','V','W','X','Y','Z','@','@','@','@','@', /* 0x50 */
 '@','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o', /* 0x60 */
 'p','q','r','s','t','u','v','w','x','y','z','@','@','@','@','@'  /* 0x70 */
};

#define DEF2ASCII(def_alp) (default_alphabet_to_ascii[def_alp])


/*
 * Structure Definitions. Note that this structure is simplified because MTR
 * only allows one service primitive per dialogue so storage is only needed
 * for one service primitive type and one invoke ID.
 * this dialogues is incoming dialogues from HLR, MAP_SRI_SM receiving
 */

//typedef struct dlg_info {
//   u8  state;                          /* state */
//   u16 ptype;                          /* service primitive type */
///   u8  invoke_id;                      /* invoke ID */
//   u16 map_inst;                       /* instance of MAP module */
//   u8  ac_len;                         /* Length of application context */
//   u8  app_context[MTR_MAX_AC_LEN];    /* Saved application context */
//   u8  msisdn[MTR_MAX_MSISDN_SIZE];    /* Saved MSISDN */
//   u8  msisdn_len;                     /* Length of MSISDN */
//   u8  term_mode;                      /* Termination Mode */
//   u8  ati_req_info;                   /* ATI Request Info */
//   u8  mms;                            /* More Messages to Send (MMS) parameter */
//   u8 dest_address[36];                 /*sccp destination address */
//   u8 origin_address[36];                 /*sccp destination address */
//}dlg_info;




/*
 * Service Type Definitions
 *
 * Values chosen to align with those of the service type primitives in
 * INAP_INC.H
 */
//#define CPT_INVOKE                  (1)
//#define CPT_RESULT_L                (2)
//#define INCPT_U_ERROR                 (3)
//#define INCPT_REJECT                  (4)

//#define CPT_NULL                    (0) /* NULL component, no equivalent in */
                                          /* INAP_INC.H  */



#endif
