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

#ifndef RL_SEQ
#define RL_SEQ

#include <linux/if_ether.h>

int rl_table_init();

uint16_t rl_get_nextseq(uint8_t src_addr[ETH_ALEN], uint8_t dest_addr[ETH_ALEN]);

/**
 * Return true if given seq_num is equals to thin in database
 * (i.e. this packet was already processed)
 */
uint8_t rl_check_seq(uint8_t src_addr[ETH_ALEN], uint8_t dest_addr[ETH_ALEN], uint16_t seq_num);

void rl_add_seq(uint8_t src_addr[ETH_ALEN], uint8_t dest_addr[ETH_ALEN], uint16_t seq_num);

#endif
