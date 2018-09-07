/* smsrouter_main.c
 *
 * main function of smsrouter */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "system.h"
#include "msg.h"
#include "sysgct.h"
#include "ss7_inc.h"
#include "map_inc.h"
#include "strtonum.h"
#include "smsrouter.h"



//mapgw
//struct GW_SRI_SM_DATA GW_sri_sm_data; // init the structure with sri_sm_resp info
//struct GW_SMS_DATA GW_sms_data;
//char filename[] = "testSMS.txt";


/*
 * Default values for MTR's command line options:
 */
#define DEFAULT_MODULE_ID        (0x2e)
#define DEFAULT_MAP_ID           (MAP_TASK_ID)


u8  smsrouter_mod_id;
u8  map_mod_id;
static u8  mtr_trace;
static u8  mtr_dlg_term_mode;

static char *program;

int fd[2];//File descriptor for creating a pipe

extern int smsrouter_ent(int fd, u8 mtr_mod_id,  u8 mtr_map_id, u8 trace, u8 dlg_term_mode);

int smsrouter_init_res();

//This function continously reads fd[0] for any input data byte
//If available, prints

void *reader()
{

//  pid_t pid;


// if (   ( pid = fork() ) < 0 ) 
//     { 
// 	  exit(0); 
//     } 

// else if ( pid > 0 ) /*parent process */

/*       { */
/* 	  close(fd[0]); //close reading */

	  smsrouter_ent(fd[0], smsrouter_mod_id, map_mod_id, mtr_trace, mtr_dlg_term_mode);

//   else




}


//	  smsrouter_ent(fd[0], mtr_mod_id, mtr_map_id, mtr_trace, mtr_dlg_term_mode);

//This function continously writes value of h pointer into fd[1]
//Waits if no more space is available

void *gct_receiver_thread()
{
//int     result;
//char    ch='A';

 HDR *h;               /* received message */

  while (1)
  {
    /*
     * GCT_receive will attempt to receive messages
     * from the task's message queue and block until
     * a message is ready.
     */
    if ((h = GCT_receive(smsrouter_mod_id)) != 0)
	{
printf("h received in writer pthread  = %p\n", h);
write(fd[1], &h, sizeof(int));
	}
  }

}

/*******************************************************************************
*
* main function of sms_router
*
********************************************************************************/
int main(int argc, char *argv[])
{
  //int failed_arg;
  //int cli_error;
//    int fd[2];
//  pid_t pid;
pthread_t       tid1,tid2;

  HDR *h;               /* received message */
HDR *h1;

  smsrouter_mod_id = DEFAULT_MODULE_ID;
  map_mod_id = DEFAULT_MAP_ID;
  mtr_trace = 1;


  if (pipe(fd) < 0)
      {
	  exit(0);
      }

  smsrouter_init_res();

pthread_create(&tid1, NULL, reader, NULL);
pthread_create(&tid2, NULL, gct_receiver_thread, NULL);

pthread_join(tid1,NULL);
pthread_join(tid2,NULL);


  /* if (   ( pid = fork() ) < 0 ) */
  /*     { */
  /* 	  exit(0); */
  /*     } */

/*   else if ( pid > 0 ) /\*parent process *\/ */

/*       { */
/* 	  close(fd[0]); //close reading */

/*   while (1) */
/*   { */
/*     /\* */
/*      * GCT_receive will attempt to receive messages */
/*      * from the task's message queue and block until */
/*      * a message is ready. */
/*      *\/ */
/*     if ((h = GCT_receive(mtr_mod_id)) != 0) */
/* 	{ */
/* printf("h received in parent = %p\n", h); */
/* 	    write(fd[1], &h, sizeof(int)); */
/* 	} */
/*   } */

//      }
//  else  /*child process */
	//    {
//close(fd[1]); //close writing
//	  smsrouter_ent(fd[0], mtr_mod_id, mtr_map_id, mtr_trace, mtr_dlg_term_mode);
//read(fd[0], h1, sizeof(int)); 
	  
//	      printf("received in child h = %p\n", h1);
//}



	  //smsrouter_ent(mtr_mod_id, mtr_map_id, mtr_trace, mtr_dlg_term_mode);

  return 0;
}
