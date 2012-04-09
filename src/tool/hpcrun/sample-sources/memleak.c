// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include "sample_source_obj.h"
#include "common.h"
#include <main.h>
#include <hpcrun/sample_sources_registered.h>
#include "simple_oo.h"
#include <hpcrun/thread_data.h>

#include <messages/messages.h>

static int receive_metric_id = -1;
static int receive_tsc_metric_id = -1;
static int receive_freq_id = -1;

/******************************************************************************
 * method definitions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  self->state = INIT; 

  // reset static variables to their virgin state
  receive_metric_id = -1;
  receive_tsc_metric_id = -1;
  receive_freq_id = -1;
}


static void
METHOD_FN(thread_init)
{
  TMSG(MEMLEAK, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(MEMLEAK, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  TMSG(MEMLEAK,"starting RCCE");

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(MEMLEAK, "thread fini (noop)");
}

static void
METHOD_FN(stop)
{
  TMSG(MEMLEAK,"stopping RCCE");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"RCCE") != NULL);
}
 

// MEMLEAK creates two metrics: bytes allocated and Bytes Freed.

static void
METHOD_FN(process_event_list,int lush_metrics)
{
  receive_metric_id = hpcrun_new_metric();
  receive_freq_id = hpcrun_new_metric();
  receive_tsc_metric_id = hpcrun_new_metric();

  TMSG(MEMLEAK, "Setting up metrics for RCCE wrapper");

  hpcrun_set_metric_info(receive_metric_id, "Bytes Received");
  hpcrun_set_metric_info(receive_freq_id, "Receive Frequency");
  hpcrun_set_metric_info(receive_tsc_metric_id, "CYCLE for Receive");
}


//
// Event sets not relevant for this sample source
// Events are generated by user code
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD; 
}


//
//
//
static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available RCCE wrapper events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("MEMLEAK\t\tThe number of bytes transfered per dynamic context\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

//
// sync class is "SS_SOFTWARE" so that both synchronous and asynchronous sampling is possible
// 

#define ss_name rcce
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


// ***************************************************************************
//  Interface functions
// ***************************************************************************

// increment the return count
//
// N.B. : This function is necessary to avoid exposing the retcnt_obj.
//        For the case of the retcnt sample source, the handler (the trampoline)
//        is separate from the sample source code.
//        Consequently, the interaction with metrics must be done procedurally

int
hpcrun_rcce_receive_id() 
{
  return receive_metric_id;
}

int
hpcrun_rcce_receive_tsc_id()
{
  return receive_tsc_metric_id;
}

int
hpcrun_rcce_receive_freq_id()
{
  return receive_freq_id;
}

int
hpcrun_rcce_active() 
{
  if (hpcrun_is_initialized()) {
    return (TD_GET(ss_state)[obj_name().evset_idx] == START);
  } else {
    return 0;
  }
}


void
hpcrun_receive_tsc_inc(cct_node_t* node, int incr)
{
  if (node != NULL) {
    TMSG(MEMLEAK, "\treceive (cct node %p): metric[%d] += %d", 
	 node, receive_metric_id, incr);
    
    cct_metric_data_increment(receive_tsc_metric_id,
			      hpcrun_cct_metrics(node) + receive_tsc_metric_id,
			      (cct_metric_data_t){.i = incr});
  }
}

void
hpcrun_receive_freq_inc(cct_node_t* node, int incr)
{
  if (node != NULL) {
    TMSG(MEMLEAK, "\treceive (cct node %p): metric[%d] += %d", 
	 node, receive_metric_id, incr);
    
    cct_metric_data_increment(receive_freq_id,
			      hpcrun_cct_metrics(node) + receive_freq_id,
			      (cct_metric_data_t){.i = incr});
  }
}
