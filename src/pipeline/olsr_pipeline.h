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

#ifndef OLSR_PIPELINE
#define OLSR_PIPELINE

#include <dessert.h>
#include "../android.h"


// ------------- message formats ----------------------------------------------

// ---- hello -----
// all HELLO messages consists of hello_header and optional specification
// of local interface following by specification of all neighbor
// interfaces connected to current local interface.

struct olsr_msg_hello_hdr {
	/**
	 * Sequence number of HELLO message. Needed to compute link quality
	 */
	u_int16_t		seq_num;
	/**
	 * Hold time of information in this message
	 */
	/*u_int8_t		hold_time;*/
	/**
	 * Interval between two HELLO messages
	 */
	u_int8_t		hello_interval;
	/**
	 * Willingness of router to retransmit broadcast messages
	 */
	u_int8_t		willingness;
	/**
	 * Number of neighbor MANET interfaces  introduced in this HELLO message
	 */
	u_int8_t		n_iface_count;
} __attribute__ ((__packed__));

struct olsr_msg_hello_niface {
	/**
	 * Link code format:
	 * +----+----+----+----+----+----+---------+
	 * | 0  | 0  | 0  | 0  | 0  | 0  | link_t  |
	 * +----+----+----+----+----+----+---------+
	 * where
	 * link_t = UNSPEC_LINK | ASYM_LINK | SYM_LINK | LOST_LINK
	 */
	u_int8_t		link_code;
	/**
	 * Ethernet address neighbor interface connected to previous introduced
	 * local interface of host that have originated this HELLO message.
	 */
	u_int8_t		n_iface_addr[ETH_ALEN];
	/**
	 * Link quality by sending data from neighbor to HELLO originator
	 */
	u_int8_t		quality_from_neighbor;
} __attribute__ ((__packed__));

/**
 * Neighbor description
 */
struct olsr_msg_hello_ndescr {
	/**
	 * Link code format:
	 * +----+----+----+----+----+----+---------+
	 * | 0  | 0  | 0  | 0  | 0  | 0  | neigh_t |
	 * +----+----+----+----+----+----+---------+
	 * where
	 * neigh_t = SYM_NEIGH | MPR_NEIGH | NOT_NEIGH
	 */
	u_int8_t		neigh_code;
	/**
	 * Main address of host
	 */
	u_int8_t		n_main_addr[ETH_ALEN];
	/**
	 * Link quality to neighbor in %
	 */
	u_int8_t		link_quality;
} __attribute__ ((__packed__));

// ---- TC -----

struct olsr_msg_tc_hdr {
	/**
	 * Sequence number of this TC message to avoid multiple re-sending
	 */
	u_int16_t		seq_num;
	/**
	 * Hold time of information in this message
	 */
	/*u_int8_t		hold_time;*/
	/**
	 * Interval between two HELLO messages
	 */
	u_int8_t		tc_interval;
	/**
	 * Number of 1hop neighbors of TC originator introduced in this TC
	 */
	u_int8_t		neighbor_count;
}__attribute__ ((__packed__));

/**
 * Neighbor description
 */
struct olsr_msg_tc_ndescr {
	/**
	 * quality of link between originator and this neighbor
	 */
	u_int8_t		link_quality;
	/**
	 * Main address of host
	 */
	u_int8_t		n_main_addr[ETH_ALEN];
} __attribute__ ((__packed__));

/**
 * Struct for routing log sequence number
 */
struct rl_seq {
	u_int32_t 	seq_num;
	u_int8_t	hop_count;
} __attribute__ ((__packed__));

// ----------

extern int pending_rtc; // pending recalculation of routing table
extern pthread_rwlock_t pp_rwlock;

// -------------------- broadcast id ------------------------------------------

struct olsr_msg_brc {
	u_int32_t		id;
} __attribute__ ((__packed__));

// ------------- pipeline -----------------------------------------------------


int olsr_handle_hello(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);

int olsr_handle_tc(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);

int olsr_fwd2dest(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);


/**
 * Encapsulate packets as dessert_msg,
 * sets NEXT HOP if known and send via OLSR routing protocol
 */
int olsr_sys2rp (dessert_msg_t *msg, size_t len, dessert_msg_proc_t *proc,
		dessert_sysif_t *tunif, dessert_frameid_t id);

/** forward packets received via OLSR to tun interface */
int rp2sys(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);


/** drop errors (drop corrupt packets, packets from myself and etc.)*/
int olsr_drop_errors(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);

// ----- Pipeline callbacks ---------- //


// ------------------------------ periodic ----------------------------------------------------

int olsr_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval);

int olsr_periodic_send_tc(void *data, struct timeval *scheduled, struct timeval *interval);

int olsr_periodic_build_routingtable(void *data, struct timeval *scheduled, struct timeval *interval);

/** clean up database from old entrys */
int olsr_periodic_cleanup_database(void *data, struct timeval *scheduled, struct timeval *interval);

#endif
