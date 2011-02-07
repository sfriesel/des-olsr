#include <pthread.h>
#include "../database/olsr_database.h"
#include "olsr_pipeline.h"
#include "../config.h"
#include "../helper.h"

u_int16_t	tc_seq_num = 0; // no need to lock, since TC never send parallel
u_int16_t	hello_seq_num = 0; // no need to lock, since HELLO never send parallel

pthread_rwlock_t hello_seq_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t tc_seq_lock = PTHREAD_RWLOCK_INITIALIZER;

size_t hello_max_links_count = DESSERT_MAXEXTDATALEN / sizeof(struct olsr_msg_hello_niface);
size_t hello_max_neigths_count = DESSERT_MAXEXTDATALEN / sizeof(struct olsr_msg_hello_ndescr);

int olsr_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	const dessert_meshif_t* iface = dessert_meshiflist_get();
	void* neighs_desc_pointer = NULL;
	void* pointer;

	// Create hello part with description of all known 1-hop neighbors.
	// This part is for all currently generated HELLOs the same.
	olsr_db_wlock();
	olsr_db_ns_tuple_t* neighbors = olsr_db_ns_getneighset();
	size_t neighs_count = HASH_COUNT(neighbors);
	neighs_count = (neighs_count > hello_max_neigths_count)? hello_max_neigths_count : neighs_count;
	size_t neighs_desc_size = 0;
	if (neighs_count > 0) {
		neighs_desc_pointer = pointer = malloc(neighs_count * sizeof(struct olsr_msg_hello_ndescr));
		if (pointer != NULL) {
			neighs_desc_size = neighs_count * sizeof(struct olsr_msg_hello_ndescr);
			while(neighbors != NULL && neighs_count-- > 0) {
				struct olsr_msg_hello_ndescr* ndesc = pointer;
				pointer += sizeof(struct olsr_msg_hello_ndescr);
				ndesc->neigh_code = (neighbors->mpr == TRUE)? MPR_NEIGH : SYM_NEIGH;
				memcpy(ndesc->n_main_addr, neighbors->neighbor_main_addr, ETH_ALEN);
				ndesc->link_quality = olsr_db_ns_getlinkquality(neighbors->neighbor_main_addr);
				neighbors = neighbors->hh.next;
			}
		}
	}
	olsr_db_unlock();


	if (verbose == TRUE)
		dessert_debug("send HELLO with hello_interval = %i", hello_interval);

	// generate link_data for all local interfaces
	while(iface != NULL) {
		size_t ext_length = sizeof(struct olsr_msg_hello_hdr);
		olsr_db_wlock();
		olsr_db_linkset_nl_entry_t* link_list = olsr_db_ls_getlinkset(iface);
		u_int8_t link_count = HASH_COUNT(link_list);
		link_count = (link_count > hello_max_links_count)? hello_max_links_count : link_count;
		ext_length += sizeof(struct olsr_msg_hello_niface) * link_count;

		dessert_msg_t* msg;
		dessert_ext_t* ext;
		dessert_msg_new(&msg);
		msg->ttl = 1;

		// add l2.5 header
		dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
		struct ether_header* l25h = (struct ether_header*) ext->data;
		memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
		memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);

		// add hello extension
		dessert_msg_addext(msg, &ext, HELLO_EXT_TYPE, ext_length);

		// add HELLO header
		struct olsr_msg_hello_hdr* hdr = (struct olsr_msg_hello_hdr*) ext->data;
		hdr->seq_num = hello_seq_num;
		hdr->hello_interval = hf_sparce_time(hello_interval);
		hdr->willingness = willingness;
		hdr->n_iface_count = link_count;
		pointer = ext->data + sizeof(struct olsr_msg_hello_hdr);
		struct timeval curr_time;
		gettimeofday(&curr_time, NULL);

		// add description of neighbors connected to actual reported interface in msg->l2h.ether_shost
		while(link_list != NULL && link_count-- > 0) {
			struct olsr_msg_hello_niface* neighbor_iface = pointer;
			pointer += sizeof(struct olsr_msg_hello_niface);

			// set LINK_CODE
			if (hf_compare_tv(&link_list->SYM_time, &curr_time) >= 0) { // SYM link
				neighbor_iface->link_code = SYM_LINK;
			} else if (hf_compare_tv(&link_list->ASYM_time, &curr_time) >= 0) { // ASYM link
				neighbor_iface->link_code = ASYM_LINK;
			} else { // LOST link
				neighbor_iface->link_code = LOST_LINK;
			}

			neighbor_iface->quality_from_neighbor =
					olsr_db_ls_getlinkquality_from_neighbor(iface, link_list->neighbor_iface_addr);
			memcpy(neighbor_iface->n_iface_addr, link_list->neighbor_iface_addr, ETH_ALEN);
			link_list = link_list->hh.next;
		}
		olsr_db_unlock();

		// add list of 1-Hop neighbors
		dessert_ext_t* ndesc_ext;
		dessert_msg_addext(msg, &ndesc_ext, HELLO_NEIGH_DESRC_TYPE, neighs_desc_size);
		memcpy(ndesc_ext->data, neighs_desc_pointer, neighs_desc_size);

		void* payload;
		uint16_t size = max(hello_size - sizeof(dessert_msg_t) - sizeof(struct ether_header) - 2, 0);
		dessert_msg_addpayload(msg, &payload, size);
		memset(payload, 0xA, size);

		// HELLO message is ready to send
		dessert_meshsend_fast(msg, iface);
		dessert_msg_destroy(msg);
		iface = iface->next;
	}
	pthread_rwlock_wrlock(&hello_seq_lock);
	hello_seq_num++;
	pthread_rwlock_unlock(&hello_seq_lock);

	if (neighs_desc_size > 0)
		free(neighs_desc_pointer);
	return 0;
}

u_int8_t max_tc_neigh_count = ((DESSERT_MAXEXTDATALEN) - sizeof(struct olsr_msg_tc_hdr)) / sizeof(struct olsr_msg_tc_ndescr);

int olsr_periodic_send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_msg_t* msg;
	dessert_ext_t* ext;
	dessert_msg_new(&msg);
	void* pointer;

	// add l2.5 header
	dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header*) ext->data;
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);

	// add TC extension
	olsr_db_wlock();
	olsr_db_ns_tuple_t* neighbors = olsr_db_ns_getneighset();
	u_int8_t neighbor_count = HASH_COUNT(neighbors);
	u_int8_t tc_neigh_count = (neighbor_count > max_tc_neigh_count)? max_tc_neigh_count : neighbor_count;
	dessert_msg_addext(msg, &ext, TC_EXT_TYPE, sizeof(struct olsr_msg_tc_hdr) + tc_neigh_count * sizeof(struct olsr_msg_tc_ndescr));
	struct olsr_msg_tc_hdr* hdr = (struct olsr_msg_tc_hdr*)ext->data;
	hdr->tc_interval = hf_sparce_time(tc_interval);
	pthread_rwlock_wrlock(&tc_seq_lock);
	hdr->seq_num = tc_seq_num++;
	pthread_rwlock_unlock(&tc_seq_lock);
	hdr->neighbor_count = tc_neigh_count;
	pointer = ext->data + sizeof(struct olsr_msg_tc_hdr);

	while(neighbors != NULL && tc_neigh_count-- > 0) {
		struct olsr_msg_tc_ndescr* neighbor_descr = pointer;
		pointer += sizeof(struct olsr_msg_tc_ndescr);
		neighbor_descr->link_quality = neighbors->best_link.quality;
		memcpy(neighbor_descr->n_main_addr, neighbors->neighbor_main_addr, ETH_ALEN);
		neighbors = neighbors->hh.next;
	}
	olsr_db_unlock();

	void* payload;
	uint16_t size = max(tc_size - sizeof(dessert_msg_t) - sizeof(struct ether_header) - 2, 0);
	dessert_msg_addpayload(msg, &payload, size);
	memset(payload, 0xA, size);

	dessert_meshsend_fast(msg, NULL);
	dessert_msg_destroy(msg);
	return 0;
}

int olsr_periodic_build_routingtable(void *data, struct timeval *scheduled, struct timeval *interval) {
	pthread_rwlock_rdlock(&pp_rwlock);
	int pending = pending_rtc;
	pthread_rwlock_unlock(&pp_rwlock);
	if (pending != FALSE) {
		if (verbose == TRUE)
			dessert_debug("re-build routing table");
		olsr_db_wlock();
		olsr_db_rt_destroy();
		olsr_db_rc_dijkstra();
		olsr_db_unlock();
		pthread_rwlock_wrlock(&pp_rwlock);
		pending_rtc = FALSE;
		pthread_rwlock_unlock(&pp_rwlock);
	}
	return 0;
}

int olsr_periodic_cleanup_database(void *data, struct timeval *scheduled, struct timeval *interval) {
	struct timeval timestamp;
	gettimeofday(&timestamp, NULL);
	if (olsr_db_cleanup(&timestamp)== TRUE) {
		return 0;
	} else
		return 1;
}
