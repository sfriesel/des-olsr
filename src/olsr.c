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

#include "cli/olsr_cli.h"
#include "pipeline/olsr_pipeline.h"
#include "database/olsr_database.h"
#include "config.h"
#include <dessert.h>
#include <dessert-extra.h>

int be_verbose = BE_VERBOSE;
u_int8_t hello_interval = HELLO_INTERVAL;
u_int8_t tc_interval = TC_INTERVAL;
u_int8_t tc_hold_time_coeff = TC_HOLD_TIME_COEFF;
u_int8_t willingness = WILL_DEFAULT;
u_int8_t window_size = WINDOW_SIZE;
int rc_metric = RC_METRIC_ETX;
char* routing_log_file = NULL;

int main(int argc, char** argv) {
	FILE *cfg = NULL;
	if ((argc == 2) && (strcmp(argv[1], "-nondaemonize") == 0)) {
		dessert_info("starting OLSR in non daemonize mode");
		dessert_init("OLSR", 0x02, DESSERT_OPT_NODAEMONIZE);
		char cfg_file_name[] = "/etc/des-olsr.conf";
	 	cfg = fopen(cfg_file_name, "r");
		if (cfg == NULL) {
			printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
			return EXIT_FAILURE;
		}
	} else {
		dessert_init("OLSR", 0x02, DESSERT_OPT_DAEMONIZE);
		cfg = dessert_cli_get_cfg(argc, argv);
	}

	// routing table initialisation
	olsr_db_init();

	/* initalize logging */
    dessert_logcfg(DESSERT_LOG_STDERR);

	// cli initialization
	struct cli_command* cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
	cli_register_command(dessert_cli, cli_cfg_set, "verbose", cli_beverbose, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "be more verbose");
	cli_register_command(dessert_cli, cli_cfg_set, "hellointerval", olsr_cli_helloint, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO interval in sek");
	cli_register_command(dessert_cli, cli_cfg_set, "tcinterval", olsr_cli_tcint, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set TC interval in sek");
	cli_register_command(dessert_cli, cli_cfg_set, "validitycoeff", olsr_cli_validitycoeff, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set validity time coefficient");
	cli_register_command(dessert_cli, cli_cfg_set, "willingness", olsr_cli_willingness, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set willingness of host to re-send broadcast messages");
	cli_register_command(dessert_cli, cli_cfg_set, "windowsize", olsr_cli_window_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set WINDOW_SIZE for calculation of link quality (PDR or ETX)");
	cli_register_command(dessert_cli, cli_cfg_set, "routinglog", cli_setrouting_log, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set path to routing logging file");
	cli_register_command(dessert_cli, cli_cfg_set, "metric", olsr_cli_rc_metric, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set metric that must be used on route calculation (PLR | HC)");
	cli_register_command(dessert_cli, cli_cfg_set, "port", cli_setport, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "configure TCP port the daemon is listening on");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

	struct cli_command* cli_command_print =
			cli_register_command(dessert_cli, NULL, "print", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print table");
	cli_register_command(dessert_cli, cli_command_print, "ns", olsr_cli_print_ns, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print neighbor set table");
	cli_register_command(dessert_cli, cli_command_print, "ns_so", olsr_cli_print_ns_so, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print neighbor set table (simple output)");
	cli_register_command(dessert_cli, cli_command_print, "ls", olsr_cli_print_ls, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print link set table");
	cli_register_command(dessert_cli, cli_command_print, "2hns", olsr_cli_print_2hns, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print 2-hop neighbor set table");
	cli_register_command(dessert_cli, cli_command_print, "tc", olsr_cli_print_tc, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print TC set table");
	cli_register_command(dessert_cli, cli_command_print, "rt", olsr_cli_print_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print routing table");
	cli_register_command(dessert_cli, cli_command_print, "rt_so", olsr_cli_print_rt_so, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print routing table (simple output)");

	// registering callbacks
	dessert_meshrxcb_add(dessert_msg_check_cb, 10); // check message
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20); // set lflags,
	dessert_meshrxcb_add(olsr_drop_errors, 30);
	dessert_meshrxcb_add(olsr_handle_hello, 40);
	dessert_meshrxcb_add(olsr_handle_tc, 45);
	//dessert_meshrxcb_add(dessert_mesh_ipttl, 75);
	dessert_meshrxcb_add(olsr_fwd2dest, 80);
	dessert_meshrxcb_add(rp2sys, 100);

	dessert_sysrxcb_add(olsr_sys2rp, 10);

	// configure
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);

	// registering periodic tasks
	struct timeval hello_interval_tv;
	hello_interval_tv.tv_sec = hello_interval;
	hello_interval_tv.tv_usec = 0;
	dessert_periodic_add(olsr_periodic_send_hello, NULL, NULL, &hello_interval_tv);

	struct timeval tc_interval_tv;
	tc_interval_tv.tv_sec = tc_interval;
	tc_interval_tv.tv_usec = 0;
	dessert_periodic_add(olsr_periodic_send_tc, NULL, NULL, &tc_interval_tv);

	struct timeval build_rt_interval;
	build_rt_interval.tv_sec = BUILD_RT_INTERVAL;
	build_rt_interval.tv_usec = 0;
	dessert_periodic_add(olsr_periodic_build_routingtable, NULL, NULL, &build_rt_interval);

	// DES-SERT and CLI Run!
	dessert_cli_run();
	dessert_run();

	return EXIT_SUCCESS;
}
