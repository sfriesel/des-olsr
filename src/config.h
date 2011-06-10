/******************************************************************************
Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

These sources were developed at the Freie Universitaet Berlin,
Computer Systems and Telematics / Distributed, embedded Systems (DES) group
(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/ .
--------------------------------------------------------------------------------
For further information and questions please use the web site
    http://www.des-testbed.net
*******************************************************************************/

#ifndef OLSR_CONFIG
#define OLSR_CONFIG

#include <stdlib.h>
#include <dessert.h>

enum bool {TRUE = 1, FALSE = 0};

enum extension_types {
    HELLO_EXT_TYPE = DESSERT_EXT_USER,
    HELLO_NEIGH_DESRC_TYPE,
    TC_EXT_TYPE,
    BROADCAST_ID_EXT_TYPE,
    RL_EXT_TYPE
};

#define SEQNO_MAX                   (1 << 16) - 1

// emission intervals
#define HELLO_INTERVAL              2000
#define TC_INTERVAL                 5000

// holding times
#define LINK_HOLD_TIME_COEFF        7
#define TC_HOLD_TIME_COEFF          20
#define BRCLOG_HOLD_TIME            3

// link types
#define UNSPEC_LINK                 0
#define ASYM_LINK                   1
#define SYM_LINK                    2
#define LOST_LINK                   3
#define LINK_MASK                   3

enum neighbor_types {
    NOT_NEIGH = 0,
    SYM_NEIGH,
    MPR_NEIGH
};

#define WILL_NEVER                  0
#define WILL_LOW                    1
#define WILL_DEFAULT                3
#define WILL_HIGH                   6
#define WILL_ALLWAYS                7

// build intervals
#define BUILD_RT_INTERVAL           1000

// link quality
#define WINDOW_SIZE                 50
#define MPR_QUALITY_THRESHOLD       75

#define C_INV_COEFF                 64

extern int                          hello_interval;
extern int                          tc_interval;
extern int                          tc_hold_time_coeff;
extern int                          willingness;
extern int                          rc_metric;
extern char*                        routing_log_file;
extern int                          hello_size;
extern int                          tc_size;
extern dessert_periodic_t*          periodic_send_hello;
extern dessert_periodic_t*          periodic_send_tc;

// window size for calculation of PDR or ETX
extern int                          window_size;

enum olsr_metric {
    RC_METRIC_PLR = 1,
    RC_METRIC_HC,
    RC_METRIC_ETX,
    RC_METRIC_ETX_ADD
};

#define HELLO_SIZE                  128
#define TC_SIZE                     128

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#endif
