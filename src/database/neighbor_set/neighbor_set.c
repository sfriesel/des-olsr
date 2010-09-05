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

#include <linux/if_ether.h>
#include <stdlib.h>
#include <stdio.h>
#include "../timeslot.h"
#include "../../config.h"
#include "neighbor_set.h"
#include "../2hop_neighbor_set/2hop_neighbor_set.h"
#include "../link_set/link_set.h"

olsr_db_ns_tuple_t*		neighbor_set = NULL;
timeslot_t*		ns_ts;

void purge_nstuple(struct timeval* timestamp, void* src_object, void* object) {
	olsr_db_ns_tuple_t* tuple = object;
	olsr_db_2hns_del1hneighbor(tuple->neighbor_main_addr);
	HASH_DEL(neighbor_set, tuple);
	free(tuple);
}

int	olsr_db_ns_init() {
	return timeslot_create(&ns_ts, NULL, purge_nstuple);
}

int ntuple_create(olsr_db_ns_tuple_t** tuple_out, u_int8_t neighbor_main_addr[ETH_ALEN]) {
	olsr_db_ns_tuple_t* tuple = malloc(sizeof(olsr_db_ns_tuple_t));
	if (tuple == NULL) return FALSE;
	memcpy(tuple->neighbor_main_addr, neighbor_main_addr, ETH_ALEN);
	tuple->mpr = FALSE;
	tuple->mpr_selector = FALSE;
	tuple->willingness = WILL_NEVER;
	tuple->best_link.local_iface = NULL;
	tuple->best_link.quality = 0;
	*tuple_out = tuple;
	return TRUE;
}

olsr_db_ns_tuple_t* olsr_db_ns_gcneigh(u_int8_t neighbor_main_addr[ETH_ALEN]) {
	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) {
		if (ntuple_create(&tuple, neighbor_main_addr) == FALSE) return FALSE;
		HASH_ADD_KEYPTR(hh, neighbor_set, tuple->neighbor_main_addr, ETH_ALEN, tuple);
	}
	return tuple;
}

void olsr_db_ns_updatetimeslot(olsr_db_ns_tuple_t* tuple, struct timeval* purgetime) {
	timeslot_addobject(ns_ts, purgetime, tuple);
}

int olsr_db_ns_getneigh(u_int8_t neighbor_main_addr[ETH_ALEN], u_int8_t* mpr_out,
		u_int8_t* mpr_selector_out, u_int8_t* willingness_out) {
	timeslot_purgeobjects(ns_ts);

	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) return FALSE;
	*mpr_out = tuple->mpr;
	*mpr_selector_out = tuple->mpr_selector;
	*willingness_out = tuple->willingness;
	return TRUE;
}

int olsr_db_ns_isneigh(u_int8_t neighbor_main_addr[ETH_ALEN]) {
	timeslot_purgeobjects(ns_ts);

	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) return FALSE;
	return TRUE;
}

int olsr_db_ns_ismprselector(u_int8_t neighbor_main_addr[ETH_ALEN]) {
	timeslot_purgeobjects(ns_ts);

	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) return FALSE;
	return tuple->mpr_selector;
}

int olsr_db_ns_setneigh_mprstatus(u_int8_t neighbor_main_addr[ETH_ALEN], u_int8_t is_mpr) {
	timeslot_purgeobjects(ns_ts);

	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) return FALSE;
	tuple->mpr = is_mpr;
	return TRUE;
}

void olsr_db_ns_removeallmprs() {
	timeslot_purgeobjects(ns_ts);
	olsr_db_ns_tuple_t* entry = neighbor_set;
	while (entry != NULL) {
		entry->mpr = FALSE;
		entry = entry->hh.next;
	}
}

olsr_db_ns_tuple_t* olsr_db_ns_getneighset() {
	timeslot_purgeobjects(ns_ts);
	return neighbor_set;
}

u_int8_t olsr_db_ns_getlinkquality(u_int8_t neighbor_main_addr[ETH_ALEN]) {
	timeslot_purgeobjects(ns_ts);
	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) return 0;
	return tuple->best_link.quality;
}

/**
 * @hint: read lock
 */
int olsr_db_ns_getbestlink(u_int8_t neighbor_main_addr[ETH_ALEN], const dessert_meshif_t** output_iface_out,
		u_int8_t neighbor_iface[ETH_ALEN]) {
	olsr_db_ns_tuple_t* tuple;
	HASH_FIND(hh, neighbor_set, neighbor_main_addr, ETH_ALEN, tuple);
	if (tuple == NULL) return FALSE;
	if (tuple->best_link.local_iface == NULL) return FALSE;
	*output_iface_out = tuple->best_link.local_iface;
	memcpy(neighbor_iface, tuple->best_link.neighbor_iface_addr, ETH_ALEN);
	return TRUE;
}

// ------------------- reporting -----------------------------------------------

int olsr_db_ns_report(char** str_out) {
	int report_ns_str_len = 98;
	olsr_db_ns_tuple_t* current_entry = neighbor_set;
	char* output;
	char entry_str[report_ns_str_len  + 1];

	output = malloc(sizeof (char) * report_ns_str_len * (4 + HASH_COUNT(current_entry)) + 1);
	if (output == NULL) return FALSE;

	// initialize first byte to \0 to mark output as empty
	*output = '\0';
	strcat(output, "+-------------------+-------+--------------+-------------+----------------------------+---------+\n");
	strcat(output, "| neighbor address  |  MPR  | MPR selector | willingness |         best link          | quality |\n");
	strcat(output, "+-------------------+-------+--------------+-------------+----------------------------+---------+\n");
	while(current_entry != NULL) {
		if (current_entry->best_link.local_iface == NULL) {
			snprintf(entry_str, report_ns_str_len + 1, "| %02x:%02x:%02x:%02x:%02x:%02x | %5s | %12s | %11i |                       NULL |       - |\n",
					current_entry->neighbor_main_addr[0], current_entry->neighbor_main_addr[1],
					current_entry->neighbor_main_addr[2], current_entry->neighbor_main_addr[3],
					current_entry->neighbor_main_addr[4], current_entry->neighbor_main_addr[5],
					(current_entry->mpr == TRUE)? "TRUE" : "FALSE",
					(current_entry->mpr_selector == TRUE)? "TRUE" : "FALSE",
					current_entry->willingness);
		} else {
			snprintf(entry_str, report_ns_str_len + 1, "| %02x:%02x:%02x:%02x:%02x:%02x | %5s | %12s | %11i | (%7s)%02x:%02x:%02x:%02x:%02x:%02x | %7i |\n",
					current_entry->neighbor_main_addr[0], current_entry->neighbor_main_addr[1],
					current_entry->neighbor_main_addr[2], current_entry->neighbor_main_addr[3],
					current_entry->neighbor_main_addr[4], current_entry->neighbor_main_addr[5],
					(current_entry->mpr == TRUE)? "TRUE" : "FALSE",
					(current_entry->mpr_selector == TRUE)? "TRUE" : "FALSE",
					current_entry->willingness, current_entry->best_link.local_iface->if_name,
					current_entry->best_link.neighbor_iface_addr[0], current_entry->best_link.neighbor_iface_addr[1],
					current_entry->best_link.neighbor_iface_addr[2], current_entry->best_link.neighbor_iface_addr[3],
					current_entry->best_link.neighbor_iface_addr[4], current_entry->best_link.neighbor_iface_addr[5],
					current_entry->best_link.quality);
		}
		strcat(output, entry_str);
		current_entry = current_entry->hh.next;
	}
	strcat(output, "+-------------------+-------+--------------+-------------+----------------------------+---------+\n");
	*str_out = output;
	return TRUE;
}

int olsr_db_ns_report_so(char** str_out) {
	int report_ns_str_len = 25;
	olsr_db_ns_tuple_t* current_entry = neighbor_set;
	char* output;
	char entry_str[report_ns_str_len  + 1];

	output = malloc(sizeof (char) * report_ns_str_len * (HASH_COUNT(current_entry)) + 1);
	if (output == NULL) return FALSE;

	// initialize first byte to \0 to mark output as empty
	*output = '\0';
	while(current_entry != NULL) {
		snprintf(entry_str, report_ns_str_len + 1, "%02x:%02x:%02x:%02x:%02x:%02x\t%i\n",
					current_entry->neighbor_main_addr[0], current_entry->neighbor_main_addr[1],
					current_entry->neighbor_main_addr[2], current_entry->neighbor_main_addr[3],
					current_entry->neighbor_main_addr[4], current_entry->neighbor_main_addr[5],
					current_entry->best_link.quality);
		strcat(output, entry_str);
		current_entry = current_entry->hh.next;
	}
	*str_out = output;
	return TRUE;
}

