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

#include <pthread.h>
#include "../database/olsr_database.h"
#include "olsr_pipeline.h"
#include "../config.h"
#include "../helper.h"
#include "../database/rl_seq_t/rl_seq.h"

pthread_rwlock_t pp_rwlock = PTHREAD_RWLOCK_INITIALIZER;
int pending_rtc = FALSE;
u_int32_t broadcast_id;

pthread_rwlock_t rlflock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rlseqlock = PTHREAD_RWLOCK_INITIALIZER;

void _rlfile_log(const u_int8_t src_addr[ETH_ALEN], const u_int8_t dest_addr[ETH_ALEN],
		const u_int32_t seq_num, const u_int8_t hop_count, const u_int8_t in_iface[ETH_ALEN],
		const u_int8_t out_iface[ETH_ALEN], const u_int8_t next_hop_addr[ETH_ALEN]) {
	pthread_rwlock_wrlock(&rlflock);
	FILE* f = fopen(routing_log_file, "a+");
	if (f == NULL) {
		dessert_debug("file = 0");
		return;
	}
	if (out_iface == NULL) {
		fprintf(f, MAC "\t" MAC "\t%u\t%u\t" MAC "\t%s\t%s\n",
		        EXPLODE_ARRAY6(src_addr),
		        EXPLODE_ARRAY6(dest_addr),
		        seq_num,
		        hop_count,
		        EXPLODE_ARRAY6(in_iface),
		        "NULL",
		        "NULL");
	} else if (in_iface == NULL) {
		fprintf(f, MAC "\t" MAC "\t%u\t%u\t%s\t" MAC "\t" MAC "\n",
		        EXPLODE_ARRAY6(src_addr),
		        EXPLODE_ARRAY6(dest_addr),
		        seq_num,
		        hop_count,
		        "NULL",
		        EXPLODE_ARRAY6(out_iface),
		        EXPLODE_ARRAY6(next_hop_addr));
	} else {
		fprintf(f,
		        MAC "\t" MAC "\t%u\t%u\t" MAC "\t" MAC "\t" MAC "\n",
		        EXPLODE_ARRAY6(src_addr),
		        EXPLODE_ARRAY6(dest_addr),
		        seq_num,
		        hop_count,
		        EXPLODE_ARRAY6(in_iface),
		        EXPLODE_ARRAY6(out_iface),
		        EXPLODE_ARRAY6(next_hop_addr));
	}
	fclose(f);
	pthread_rwlock_unlock(&rlflock);
}

// ---------------------------- pipeline callbacks ---------------------------------------------

int olsr_drop_errors(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	// drop packets sent by myself.
	if (proc->lflags & DESSERT_LFLAG_PREVHOP_SELF) {
		return DESSERT_MSG_DROP;
	}
	if (proc->lflags & DESSERT_LFLAG_SRC_SELF) {
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

int olsr_handle_hello(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	dessert_ext_t* ext;

	if (dessert_msg_getext(msg, &ext, HELLO_EXT_TYPE, 0) != 0) {
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		struct olsr_msg_hello_hdr* hdr = (struct olsr_msg_hello_hdr*) ext->data;
		void* pointer = ext->data + sizeof(struct olsr_msg_hello_hdr);

		struct timeval curr_time;
		gettimeofday(&curr_time, NULL);

		struct timeval expired_time;
		expired_time.tv_sec = curr_time.tv_sec - 1;
		expired_time.tv_usec = curr_time.tv_usec;

		float hello_inf_f = hf_parce_time(hdr->hello_interval);
		if (verbose == TRUE)
			dessert_debug("get HELLO with hello_inf = %f", hello_inf_f);
		float hello_hold_time_f = hello_inf_f * (LINK_HOLD_TIME_COEFF + 1);
		struct timeval hold_time;
		hold_time.tv_sec = hello_hold_time_f;
		hold_time.tv_usec = (hello_hold_time_f - hold_time.tv_sec) * 1000000;
		hf_add_tv(&curr_time, &hold_time, &hold_time);
		float msg_interval_f = hf_parce_time(hdr->hello_interval);
		struct timeval msg_interval;
		msg_interval.tv_sec = msg_interval_f;
		msg_interval.tv_usec = (msg_interval_f - msg_interval.tv_sec) * 1000000;

		olsr_db_wlock();
		// be careful while setting link status to ASYN. It may be SYN before.
		// therefore don't make DB unlock, before finish setting link status
		olsr_db_linkset_ltuple_t* link_iface = olsr_db_ls_getif(iface);
		olsr_db_linkset_nl_entry_t* link_neigh = olsr_db_ls_getneigh(link_iface,
				msg->l2h.ether_shost, l25h->ether_shost);
		olsr_db_ns_tuple_t* neighbor = NULL;
		link_neigh->SYM_time.tv_sec = expired_time.tv_sec;
		link_neigh->SYM_time.tv_usec = expired_time.tv_usec;
		link_neigh->ASYM_time.tv_sec = hold_time.tv_sec;
		link_neigh->ASYM_time.tv_usec = hold_time.tv_usec;
		link_neigh->quality_to_neighbor = 0;

		int recalculate_mpr_set = FALSE;

		// parse neighbor_interface section
		while (hdr->n_iface_count-- > 0) {
			struct olsr_msg_hello_niface* neighbor_iface = pointer;
			pointer += sizeof (struct olsr_msg_hello_niface);

			if (((neighbor_iface->link_code & LINK_MASK) != LOST_LINK) &&
				(olsr_db_lis_islocaliface(neighbor_iface->n_iface_addr)) == TRUE &&
				memcmp(iface->hwaddr, neighbor_iface->n_iface_addr, ETH_ALEN) == 0)
				// HELLO generator is in SYM neighborhood
				{
				link_neigh->SYM_time.tv_sec = hold_time.tv_sec;
				link_neigh->SYM_time.tv_usec = hold_time.tv_usec;
				link_neigh->quality_to_neighbor = neighbor_iface->quality_from_neighbor;
				// update link quality from neighbor to current neighbor interface
				olsr_sw_addsn(link_neigh->sw, hdr->seq_num);
				// update best link to current neighbor
				u_int8_t quality;
				if (rc_metric == RC_METRIC_ETX) {
					u_int8_t quality_from_neighbor =
							olsr_sw_getquality(link_neigh->sw);
					u_int8_t quality_to_neighbor = neighbor_iface->quality_from_neighbor;
					// (1 / ETX) * 100 %
					quality = (quality_from_neighbor * quality_to_neighbor) / 100;
				} else {
					quality = neighbor_iface->quality_from_neighbor;
				}
				// set SYM link type to neighbor and discard MPR protperties
				neighbor = olsr_db_ns_gcneigh(l25h->ether_shost);
				neighbor->mpr_selector = FALSE;
				neighbor->mpr = FALSE;
				neighbor->willingness = hdr->willingness;
				// update best link
				if (neighbor->best_link.local_iface == iface
					&& memcmp(neighbor->best_link.neighbor_iface_addr, msg->l2h.ether_shost, ETH_ALEN) == 0)
					// if the same link - update quality
				{
					neighbor->best_link.quality = quality;
				} else if (neighbor->best_link.quality < quality)
					// another best link -> rewrite
				{
					neighbor->best_link.quality = quality;
					neighbor->best_link.local_iface = iface;
					memcpy(neighbor->best_link.neighbor_iface_addr, msg->l2h.ether_shost, ETH_ALEN);
				}
				olsr_db_ns_updatetimeslot(neighbor, &hold_time);
				// host in 1hop neighborhood can not be 2hop neighbor
				olsr_db_2hns_del2hneighbor(l25h->ether_shost);
				// remove old 2hop neighbors from 1hop host
				olsr_db_2hns_clear1hn(l25h->ether_shost);
				recalculate_mpr_set = TRUE;
			}
		}
		timeslot_addobject(link_iface->ts, &hold_time, link_neigh);

		// parse neighbor description section
		dessert_ext_t* ndesc_ext;
		dessert_msg_getext(msg, &ndesc_ext, HELLO_NEIGH_DESRC_TYPE, 0);
		u_int8_t n_count = ndesc_ext->len / sizeof(struct olsr_msg_hello_ndescr);
		pointer = ndesc_ext->data;
		while (n_count-- > 0) {
			struct olsr_msg_hello_ndescr* neighbor_descr = pointer;
			pointer += sizeof(struct olsr_msg_hello_ndescr);

			if (memcmp(neighbor_descr->n_main_addr, dessert_l25_defsrc, ETH_ALEN) == 0 &&
					neighbor_descr->neigh_code == MPR_NEIGH && neighbor != NULL) {
				// set HELLO originator as MPR selector
				neighbor->mpr_selector = TRUE;
			}

			if (olsr_db_ns_isneigh(neighbor_descr->n_main_addr) != TRUE && neighbor != NULL &&
				memcmp(neighbor_descr->n_main_addr, dessert_l25_defsrc, ETH_ALEN) != 0) {
				if (neighbor_descr->neigh_code != NOT_NEIGH) {
					// SYM link and not in 1hop neighborhood -> 2hop neighbor
					olsr_db_2hns_add2hneighbor(l25h->ether_shost, neighbor_descr->n_main_addr,
							neighbor_descr->link_quality, &hold_time);
					recalculate_mpr_set = TRUE;
				}
			}
		}
		if (recalculate_mpr_set == TRUE)
			olsr_db_rc_chose_mprset();
		olsr_db_unlock();
		pthread_rwlock_wrlock(&pp_rwlock);
		pending_rtc = TRUE;
		pthread_rwlock_unlock(&pp_rwlock);
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

int olsr_handle_tc(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t* ext;

	if (dessert_msg_getext(msg, &ext, TC_EXT_TYPE, 0) != 0) {
		u_int8_t prev_hop_main_addr[ETH_ALEN];
		struct olsr_msg_tc_hdr* hdr = (struct olsr_msg_tc_hdr*) ext->data;
		void* pointer = ext->data + sizeof(struct olsr_msg_tc_hdr);

		struct timeval curr_time, hold_time, purge_time;
		gettimeofday(&curr_time, NULL);
		float tc_int_f = hf_parce_time(hdr->tc_interval);
		float tc_hold_time_f = tc_int_f * tc_hold_time_coeff;
		hold_time.tv_sec = tc_hold_time_f;
		hold_time.tv_usec = (tc_hold_time_f - hold_time.tv_sec) * 1000000;
		hf_add_tv(&curr_time, &hold_time, &purge_time);

		struct ether_header* l25h = dessert_msg_getl25ether(msg);

		olsr_db_wlock();
		int seq_update_result = olsr_db_tc_updateseqnum(l25h->ether_shost, hdr->seq_num, &purge_time);
		olsr_db_unlock();

		if (seq_update_result != TRUE) {
			return DESSERT_MSG_DROP;
		}

		int ncount = hdr->neighbor_count;
		int iam_MPR = FALSE;

		olsr_db_wlock();
		if (ncount == 0) {
			olsr_db_tc_removetc(l25h->ether_shost);
		} else {
			// remove all old neighbor entrys for this host;
			olsr_db_tc_removeneighbors(l25h->ether_shost);
			while (ncount-- > 0) {
				struct olsr_msg_tc_ndescr* ndescr = pointer;
				pointer += sizeof(struct olsr_msg_tc_ndescr);
				olsr_db_tc_settuple(l25h->ether_shost, ndescr->n_main_addr, ndescr->link_quality, &purge_time);
			}
		}
		// re-send only if previous host has selected me as MPR
		int result = olsr_db_ls_getmainaddr(iface, msg->l2h.ether_shost, prev_hop_main_addr);
		if (result) iam_MPR = olsr_db_ns_ismprselector(prev_hop_main_addr);
		olsr_db_unlock();

		if ((iam_MPR == TRUE)
			//|| (willingness >= WILL_ALLWAYS)
			) {
			dessert_meshsend_fast_randomized(msg);
		}

		pthread_rwlock_wrlock(&pp_rwlock);
		pending_rtc = TRUE;
		pthread_rwlock_unlock(&pp_rwlock);
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

int olsr_fwd2dest(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	dessert_ext_t* rl_ext;
	u_int32_t rl_seq_num = 0;
	u_int8_t rl_hop_count = 0;
	if (dessert_msg_getext(msg, &rl_ext, RL_EXT_TYPE, 0) != 0) {
		struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
		rl_seq_num = rl_data->seq_num;
		pthread_rwlock_wrlock(&rlseqlock);
		int pk = rl_check_seq(l25h->ether_shost, l25h->ether_dhost, rl_seq_num);
		pthread_rwlock_unlock(&rlseqlock);
		if (pk == TRUE) {
			// this packet was already processed
			if (verbose == TRUE)
				dessert_debug("DUP! from %x:%x:%x:%x:%x:%x to %x:%x:%x:%x:%x:%x hops %i",
					l25h->ether_shost[0], l25h->ether_shost[1], l25h->ether_shost[2],
					l25h->ether_shost[1], l25h->ether_shost[4], l25h->ether_shost[5],
					l25h->ether_dhost[0], l25h->ether_dhost[1], l25h->ether_dhost[2],
					l25h->ether_dhost[1], l25h->ether_dhost[4], l25h->ether_dhost[5],
					rl_data->hop_count + 1);
			return DESSERT_MSG_DROP;
		}
		pthread_rwlock_wrlock(&rlseqlock);
		rl_add_seq(l25h->ether_shost, l25h->ether_dhost, rl_seq_num);
		pthread_rwlock_unlock(&rlseqlock);
		if (rl_data->hop_count != 255) rl_hop_count = ++rl_data->hop_count;
	}

	if (proc->lflags & DESSERT_LFLAG_DST_BROADCAST ||
		proc->lflags & DESSERT_LFLAG_DST_MULTICAST) { // BROADCAST
		dessert_ext_t* ext;
		if (dessert_msg_getext(msg, &ext, BROADCAST_ID_EXT_TYPE, 0) != 0) {
			u_int8_t prev_hop_main_addr[ETH_ALEN];
			struct olsr_msg_brc* brc_data = (struct olsr_msg_brc*) ext->data;
			struct timeval curr_time, hold_time, purge_time;
			gettimeofday(&curr_time, NULL);
			hold_time.tv_sec = BRCLOG_HOLD_TIME;
			hold_time.tv_usec = 0;
			hf_add_tv(&curr_time, &hold_time, &purge_time);

			olsr_db_wlock();
			int result = olsr_db_brct_addid(l25h->ether_shost, brc_data->id, &purge_time);
			if (result == TRUE) {
				result = olsr_db_ls_getmainaddr(iface, msg->l2h.ether_shost, prev_hop_main_addr);
			} else {
				olsr_db_unlock();
				if (verbose == TRUE) {
					dessert_debug("drop broadcast %i duplicate", brc_data->id);
				}
				return DESSERT_MSG_DROP;
			}
			if (result == TRUE) {
				//if (willingness < WILL_ALLWAYS) {
					result = olsr_db_ns_ismprselector(prev_hop_main_addr);
				//}
			}
			olsr_db_unlock();
			if (result == TRUE) {
				// resend only if combination of source address and broadcast id is new
				// AND
				// previous host has selected me as MPR
				dessert_meshsend_fast_randomized(msg);
			}
		}
		return DESSERT_MSG_KEEP;
	} else if (((proc->lflags & DESSERT_LFLAG_NEXTHOP_SELF && !(proc->lflags & DESSERT_LFLAG_NEXTHOP_SELF_OVERHEARD)) ||
				proc->lflags & DESSERT_LFLAG_NEXTHOP_BROADCAST) &&
				!(proc->lflags & DESSERT_LFLAG_DST_SELF)){ // Directed message
		u_int8_t next_hop[ETH_ALEN];
		u_int8_t next_hop_iface[ETH_ALEN];
		const dessert_meshif_t* output_iface;
		// find and set (if found) NEXT HOP towards destination
		olsr_db_rlock();
		int result = olsr_db_rt_getnexthop(l25h->ether_dhost, next_hop);
		if (result == TRUE) {
			result = olsr_db_ns_getbestlink(next_hop, &output_iface, next_hop_iface);
		}
		olsr_db_unlock();
		if (result == TRUE) {
			memcpy(msg->l2h.ether_dhost, next_hop_iface, ETH_ALEN);
			if (routing_log_file != NULL) {
				_rlfile_log(l25h->ether_shost, l25h->ether_dhost, rl_seq_num, rl_hop_count, iface->hwaddr, output_iface->hwaddr, next_hop_iface);
			}
			// forward to the next hop
			dessert_meshsend_fast(msg, output_iface);
		}
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}
// --------------------------- TUN ----------------------------------------------------------

int olsr_sys2rp (dessert_msg_t *msg, size_t len, dessert_msg_proc_t *proc,
		dessert_sysif_t *tunif, dessert_frameid_t id) {
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	if (memcmp(l25h->ether_dhost, ether_broadcast, ETH_ALEN) == 0) { // next hop broadcast
		// add broadcast id to prevent unlimited circulation
		dessert_ext_t* ext;
		dessert_msg_addext(msg, &ext, BROADCAST_ID_EXT_TYPE, sizeof(struct olsr_msg_brc));
		struct olsr_msg_brc* brc_data = (struct olsr_msg_brc*) ext->data;
		pthread_rwlock_wrlock(&pp_rwlock);
		brc_data->id = broadcast_id++;
		pthread_rwlock_unlock(&pp_rwlock);
		// send out
		dessert_meshsend_fast(msg, NULL);
	} else {
		u_int8_t next_hop[ETH_ALEN];
		u_int8_t next_hop_iface[ETH_ALEN];
		const dessert_meshif_t* output_iface;
		// find and set (if found) NEXT HOP towards destination
		olsr_db_rlock();
		int result = olsr_db_rt_getnexthop(l25h->ether_dhost, next_hop);
		if (result == TRUE) {
			result = olsr_db_ns_getbestlink(next_hop, &output_iface, next_hop_iface);
		}
		olsr_db_unlock();
		if (result == TRUE) {
			u_int32_t seq_num = 0;
			pthread_rwlock_wrlock(&rlseqlock);
			seq_num = rl_get_nextseq(dessert_l25_defsrc, l25h->ether_dhost);
			pthread_rwlock_unlock(&rlseqlock);
			dessert_ext_t* rl_ext;
			dessert_msg_addext(msg, &rl_ext, RL_EXT_TYPE, sizeof(struct rl_seq));
			struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
			rl_data->seq_num = seq_num;
			rl_data->hop_count = 0;
			if (routing_log_file != NULL)
				_rlfile_log(dessert_l25_defsrc, l25h->ether_dhost, seq_num, 0, NULL, output_iface->hwaddr, next_hop_iface);
			memcpy(msg->l2h.ether_dhost, next_hop_iface, ETH_ALEN);
			// forward to the next hop
			dessert_meshsend_fast(msg, output_iface);
		}
	}
    return DESSERT_MSG_DROP;
}

// ----------------- common callbacks ---------------------------------------------------

/**
 * Forward packets addressed to me to tun pipeline
 */
int rp2sys(dessert_msg_t* msg, size_t len,
		dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	if((proc->lflags & DESSERT_LFLAG_DST_SELF && !(proc->lflags & DESSERT_LFLAG_DST_SELF_OVERHEARD)) ||
		proc->lflags & DESSERT_LFLAG_DST_BROADCAST ||
		proc->lflags & DESSERT_LFLAG_DST_MULTICAST ) {
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		dessert_ext_t* rl_ext;
		u_int32_t rl_seq_num = 0;
		u_int8_t rl_hop_count = 0;
		if (dessert_msg_getext(msg, &rl_ext, RL_EXT_TYPE, 0) != 0) {
			struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
			pthread_rwlock_wrlock(&rlseqlock);
			rl_add_seq(dessert_l25_defsrc, l25h->ether_shost, rl_data->seq_num);
			pthread_rwlock_unlock(&rlseqlock);
			rl_seq_num = rl_data->seq_num;
			rl_hop_count = rl_data->hop_count;
		}
		if (routing_log_file != NULL && !(proc->lflags & DESSERT_LFLAG_DST_BROADCAST) && !(proc->lflags & DESSERT_LFLAG_DST_MULTICAST))
			_rlfile_log(l25h->ether_shost, l25h->ether_dhost, rl_seq_num, rl_hop_count, iface->hwaddr, NULL, NULL);
		dessert_syssend_msg(msg);
	}
	return DESSERT_MSG_DROP;
}
