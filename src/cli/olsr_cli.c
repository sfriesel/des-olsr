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

#include <dessert.h>
#include <time.h>
#include <stdio.h>
#include "olsr_cli.h"
#include "../database/olsr_database.h"
#include "../config.h"

// -------------------- config ------------------------------------------------------------
int cli_beverbose(struct cli_def* cli, char* command, char* argv[], int argc) {
	u_int32_t mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1|| (mode != 0 && mode != 1)) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	if (mode == 1) {
		dessert_info("be verbose = TRUE");
		be_verbose = TRUE;
	} else {
		dessert_info("be verbose = FALSE");
		be_verbose = FALSE;
	}
	return CLI_OK;
}

int olsr_cli_helloint(struct cli_def* cli, char* command, char* argv[], int argc) {
	unsigned int mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	hello_interval = (uint8_t) mode;
	dessert_debug("set HELLO_INTERVAL to %i", hello_interval);
	return CLI_OK;
}

int olsr_cli_tcint(struct cli_def* cli, char* command, char* argv[], int argc) {
	unsigned int mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	tc_interval = (uint8_t) mode;
	dessert_debug("set TC_INTERVAL to %i", tc_interval);
	return CLI_OK;
}

int olsr_cli_validitycoeff(struct cli_def* cli, char* command, char* argv[], int argc) {
	unsigned int mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	if (mode >= LINK_HOLD_TIME_COEFF) {
		tc_hold_time_coeff = (uint8_t) mode;
		dessert_debug("set TC_HOLD_TIME_COEFF to %i", tc_hold_time_coeff);
	} dessert_debug("TC_HOLD_TIME_COEFF must be greater than LINK_HOLD_TIME_KOEFF");
	return CLI_OK;
}

int olsr_cli_willingness(struct cli_def* cli, char* command, char* argv[], int argc) {
	unsigned int mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	willingness = (uint8_t) mode;
	dessert_debug("set WILLINGNESS to %i", willingness);
	return CLI_OK;
}

int olsr_cli_window_size(struct cli_def* cli, char* command, char* argv[], int argc) {
	unsigned int mode;

	if (argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
		cli_print(cli, "usage of %s command [0, 1]\n", command);
		return CLI_ERROR_ARG;
	}
	window_size = (uint8_t) mode;
	dessert_debug("set WINDOW_SIZE to %i", window_size);
	return CLI_OK;
}

int olsr_cli_rc_metric(struct cli_def* cli, char* command, char* argv[], int argc) {
	if (argc != 1 || (strcmp(argv[0], "PLR") != 0 && strcmp(argv[0], "HC") != 0 && strcmp(argv[0], "ETX") != 0 && strcmp(argv[0], "ETX-ADD") != 0)) {
		cli_print(cli, "usage of %s command [PLR, HC, ETX, ETX-ADD]\n", command);
		dessert_err("usage of %s command [PLR, HC, ETX, ETX-ADD]");
		return CLI_ERROR_ARG;
	}
	if (strcmp(argv[0], "PLR") == 0) {
		rc_metric = RC_METRIC_PLR;
		dessert_debug("set metric to PLR (packet lost rate)");
	} else if (strcmp(argv[0], "HC") == 0) {
		rc_metric = RC_METRIC_HC;
		dessert_debug("set metric to HC (hop count)");
	} else if (strcmp(argv[0], "ETX") == 0) {
		rc_metric = RC_METRIC_ETX;
		dessert_debug("set metric to ETX (probabilistic path ETX)");
	} else if (strcmp(argv[0], "ETX-ADD") == 0) {
		rc_metric = RC_METRIC_ETX_ADD;
		dessert_debug("set metric to ETX-ADD (additive path ETX)");
	}
	return CLI_OK;
}

// -------------------- Testing ------------------------------------------------------------

/**
 * Print neighbor set table
 */
int olsr_cli_print_ns(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_wlock();
	int result = olsr_db_ns_report(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

/**
 * Print neighbor set table (simple output)
 */
int olsr_cli_print_ns_so(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_wlock();
	int result = olsr_db_ns_report_so(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

/**
 * Print link set table
 */
int olsr_cli_print_ls(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_wlock();
	int result = olsr_db_ls_report(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

/**
 * Print 2hop neighbor set table
 */
int olsr_cli_print_2hns(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_wlock();
	int result = olsr_db_2hns_report(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

/**
 * Print TC set table
 */
int olsr_cli_print_tc(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_wlock();
	int result = olsr_db_tc_report(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

/**
 * Print routing table
 */
int olsr_cli_print_rt(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_rlock();
	int result = olsr_db_rt_report(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

/**
* Print routing table (simple output)
*/
int olsr_cli_print_rt_so(struct cli_def* cli, char* command, char* argv[], int argc){
	char* report;
	olsr_db_rlock();
	int result = olsr_db_rt_report_so(&report);
	olsr_db_unlock();
	if (result == TRUE) {
		cli_print(cli, "\n%s\n", report);
		free(report);
	}
	return CLI_OK;
}

// -------------------- common cli functions ----------------------------------------------

int cli_setrouting_log(struct cli_def *cli, char *command, char *argv[], int argc) {
	routing_log_file = malloc(strlen(argv[0]));
	strcpy(routing_log_file, argv[0]);
	FILE* f = fopen(routing_log_file, "a+");
	time_t lt;
	lt = time(NULL);
	fprintf(f, "\n--- %s\n", ctime(&lt));
	fclose(f);
	dessert_info("logging routing data at file %s", routing_log_file);
	return CLI_OK;
}
