/***************************************************************************//**
 * @file sl_cli_command_table.c
 * @brief Declarations of relevant command structs for cli framework.
 * @version x.y.z
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <stdlib.h>

#include "sl_cli_config.h"
#include "sl_cli_command.h"
#include "sl_cli_arguments.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *****************************   TEMPLATED FILE   ******************************
 ******************************************************************************/

/*******************************************************************************
 * Example syntax (.slcc or .slcp) for populating this file:
 *
 *   template_contribution:
 *     - name: cli_command          # Register a command
 *       value:
 *         name: status             # Name of command
 *         handler: status_command  # Function to be called. Must be defined
 *         help: "Prints status"    # Optional help description
 *         shortcuts:               # Optional shorcut list
 *           - name: st
 *         argument:                # Argument list, if apliccable
 *           - type: uint8          # Variable type
 *             help: "Channel"      # Optional description
 *           - type: string
 *             help: "Text"
 *     - name: cli_group            # Register a group
 *       value:
 *         name: shell              # Group name
 *         help: "Shell commands"   # Optional help description
 *         shortcuts:               # Optional shorcuts
 *           - name: sh
 *     - name: cli_command
 *       value:
 *         name: repeat
 *         handler: repeat_cmd
 *         help: "Repeat commands"
 *         shortcuts:
 *           - name: r
 *           - name: rep
 *         group: shell            # Associate command with group
 *         argument:
 *           - type: string
 *             help: "Text"
 *           - type: additional
 *             help: "More text"
 *
 * For subgroups, an optional unique id can be used to allow a particular name to
 * be used more than once. In the following case, from the command line the
 * following commands are available:
 *
 * >  root_1 shell status
 * >  root_2 shell status
 *
 *     - name: cli_group            # Register a group
 *       value:
 *         name: root_1             # Group name
 *
 *     - name: cli_group            # Register a group
 *       value:
 *         name: root_2             # Group name
 *
 *    - name: cli_group             # Register a group
 *       value:
 *         name: shell              # Group name
 *         id: shell_root_1         # Optional unique id for group
 *         group: root_1            # Add group to root_1 group
 *
 *    - name: cli_group             # Register a group
 *       value:
 *         name: shell              # Group name
 *         id: shell_root_2         # Optional unique id for group
 *         group: root_2            # Add group to root_1 group
 *
 *    - name: cli_command           # Register a command
 *       value:
 *         name: status
 *         handler: status_1
 *         group: shell_root_1      # id of subgroup
 *
 *    - name: cli_command           # Register a command
 *       value:
 *         name: status
 *         handler: status_2
 *         group: shell_root_2      # id of subgroup
 *
 ******************************************************************************/

// Provide function declarations
void cli_throughput_central_stop(sl_cli_command_arg_t *arguments);
void cli_throughput_central_start(sl_cli_command_arg_t *arguments);
void cli_throughput_central_status(sl_cli_command_arg_t *arguments);
void cli_throughput_central_mode_set(sl_cli_command_arg_t *arguments);
void cli_throughput_central_mode_get(sl_cli_command_arg_t *arguments);
void cli_throughput_central_tx_power_set(sl_cli_command_arg_t *arguments);
void cli_throughput_central_tx_power_get(sl_cli_command_arg_t *arguments);
void cli_throughput_central_data_set(sl_cli_command_arg_t *arguments);
void cli_throughput_central_data_get(sl_cli_command_arg_t *arguments);
void cli_throughput_central_phy_scan_set(sl_cli_command_arg_t *arguments);
void cli_throughput_central_phy_conn_set(sl_cli_command_arg_t *arguments);
void cli_throughput_central_phy_get(sl_cli_command_arg_t *arguments);
void cli_throughput_central_connection_set(sl_cli_command_arg_t *arguments);
void cli_throughput_central_connection_get(sl_cli_command_arg_t *arguments);
void cli_throughput_central_allowlist_add(sl_cli_command_arg_t *arguments);
void cli_throughput_central_allowlist_clear(sl_cli_command_arg_t *arguments);
void cli_throughput_central_allowlist_get(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_stop(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_start(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_status(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_mode_set(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_mode_get(sl_cli_command_arg_t *arguments);
void cli_throughput_tx_power_set(sl_cli_command_arg_t *arguments);
void cli_throughput_tx_power_get(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_data_set(sl_cli_command_arg_t *arguments);
void cli_throughput_peripheral_data_get(sl_cli_command_arg_t *arguments);

// Command structs. Names are in the format : cli_cmd_{command group name}_{command name}
// In order to support hyphen in command and group name, every occurence of it while
// building struct names will be replaced by "_hyphen_"
static const sl_cli_command_info_t cli_cmd_throughput_central_stop = \
  SL_CLI_COMMAND(cli_throughput_central_stop,
                 "Stops remote transmission",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_throughput_central_start = \
  SL_CLI_COMMAND(cli_throughput_central_start,
                 "Starts remote transmission",
                  "Type: 1: notification, 2: indication" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_throughput_central_status = \
  SL_CLI_COMMAND(cli_throughput_central_status,
                 "Prints current status",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_central_mode_set = \
  SL_CLI_COMMAND(cli_throughput_central_mode_set,
                 "Set reception mode",
                  "Mode: 0:Continous, 1:Fixed time, 2: Fixed size" SL_CLI_UNIT_SEPARATOR "Value to set (in case of fixed time/size)" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT32, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_central_mode_get = \
  SL_CLI_COMMAND(cli_throughput_central_mode_get,
                 "Get reception mode, 0:Continous, 1:Fixed time, 2: Fixed size",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_central_tx_power_set = \
  SL_CLI_COMMAND(cli_throughput_central_tx_power_set,
                 "Set power",
                  "Power in dBm" SL_CLI_UNIT_SEPARATOR "Enable adaptive power control" SL_CLI_UNIT_SEPARATOR "Enable deep sleep" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_INT16, SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_central_tx_power_get = \
  SL_CLI_COMMAND(cli_throughput_central_tx_power_get,
                 "Read power settings",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_central_data_set = \
  SL_CLI_COMMAND(cli_throughput_central_data_set,
                 "Set data size",
                  "Maximum MTU size" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_central_data_get = \
  SL_CLI_COMMAND(cli_throughput_central_data_get,
                 "Read data settings",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_phy_scan_set = \
  SL_CLI_COMMAND(cli_throughput_central_phy_scan_set,
                 "Set PHY used for scanning",
                  "Scan PHY, 1:1M 4:Coded PHY" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_phy_conn_set = \
  SL_CLI_COMMAND(cli_throughput_central_phy_conn_set,
                 "Set PHY used for the connection, 1:1M 2:2M 4:Coded 125k, 8:Coded 500k PHY",
                  "Connection PHY" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_phy_get = \
  SL_CLI_COMMAND(cli_throughput_central_phy_get,
                 "Read PHY settings for scan and connection, 1:1M 2:2M 4:Coded 125k, 8:Coded 500k PHY",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_connection_set = \
  SL_CLI_COMMAND(cli_throughput_central_connection_set,
                 "Set connection settings",
                  "Minimum connection interval (in 1.25 ms steps)" SL_CLI_UNIT_SEPARATOR "Maximum connection interval (in 1.25 ms steps)" SL_CLI_UNIT_SEPARATOR "Responder latency (in connection intervals)" SL_CLI_UNIT_SEPARATOR "Supervision timeout (in 10 ms steps)" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_UINT32, SL_CLI_ARG_UINT32, SL_CLI_ARG_UINT32, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_connection_get = \
  SL_CLI_COMMAND(cli_throughput_central_connection_get,
                 "Read connection settings",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_allowlist_add = \
  SL_CLI_COMMAND(cli_throughput_central_allowlist_add,
                 "Add address to allowlist",
                  "Bluetooth address in 'XX:XX:XX:XX:XX:XX' format" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_allowlist_clear = \
  SL_CLI_COMMAND(cli_throughput_central_allowlist_clear,
                 "Clear allowlist",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_allowlist_get = \
  SL_CLI_COMMAND(cli_throughput_central_allowlist_get,
                 "Read allowlist table",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_throughput_peripheral_stop = \
  SL_CLI_COMMAND(cli_throughput_peripheral_stop,
                 "Stops transmission",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_throughput_peripheral_start = \
  SL_CLI_COMMAND(cli_throughput_peripheral_start,
                 "Starts transmission",
                  "Type: 1: notification, 2: indication" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_throughput_peripheral_status = \
  SL_CLI_COMMAND(cli_throughput_peripheral_status,
                 "Prints current status",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_mode_set = \
  SL_CLI_COMMAND(cli_throughput_peripheral_mode_set,
                 "Set transmission mode",
                  "Mode: 0:Continous, 1:Fixed size, 2: Fixed time" SL_CLI_UNIT_SEPARATOR "Value to set (in case of fixed time/size)" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT32, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_mode_get = \
  SL_CLI_COMMAND(cli_throughput_peripheral_mode_get,
                 "Get transmission mode, 0:Continous, 1:Fixed size, 2: Fixed time",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_power_set = \
  SL_CLI_COMMAND(cli_throughput_tx_power_set,
                 "Set power",
                  "Power in dBm" SL_CLI_UNIT_SEPARATOR "Enable adaptive power control" SL_CLI_UNIT_SEPARATOR "Enable deep sleep" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_INT16, SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_power_get = \
  SL_CLI_COMMAND(cli_throughput_tx_power_get,
                 "Read power settings",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_data_set = \
  SL_CLI_COMMAND(cli_throughput_peripheral_data_set,
                 "Set data sizes",
                  "Maximum MTU size" SL_CLI_UNIT_SEPARATOR "Indication transfer size" SL_CLI_UNIT_SEPARATOR "Notification transfer size" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_data_get = \
  SL_CLI_COMMAND(cli_throughput_peripheral_data_get,
                 "Read data settings",
                  "",
                 {SL_CLI_ARG_END, });


// Create group command tables and structs if cli_groups given
// in template. Group name is suffixed with _group_table for tables
// and group commands are cli_cmd_grp_( group name )
static const sl_cli_command_entry_t central_mode_group_table[] = {
  { "set", &cli_cmd_central_mode_set, false },
  { "s", &cli_cmd_central_mode_set, true },
  { "get", &cli_cmd_central_mode_get, false },
  { "g", &cli_cmd_central_mode_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_central_mode = \
  SL_CLI_COMMAND_GROUP(central_mode_group_table, "Mode");

static const sl_cli_command_entry_t central_tx_power_group_table[] = {
  { "set", &cli_cmd_central_tx_power_set, false },
  { "s", &cli_cmd_central_tx_power_set, true },
  { "get", &cli_cmd_central_tx_power_get, false },
  { "g", &cli_cmd_central_tx_power_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_central_tx_power = \
  SL_CLI_COMMAND_GROUP(central_tx_power_group_table, "Power settings");

static const sl_cli_command_entry_t central_data_group_table[] = {
  { "set", &cli_cmd_central_data_set, false },
  { "s", &cli_cmd_central_data_set, true },
  { "get", &cli_cmd_central_data_get, false },
  { "g", &cli_cmd_central_data_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_central_data = \
  SL_CLI_COMMAND_GROUP(central_data_group_table, "Data settings");

static const sl_cli_command_entry_t phy_group_table[] = {
  { "scan_set", &cli_cmd_phy_scan_set, false },
  { "s", &cli_cmd_phy_scan_set, true },
  { "conn_set", &cli_cmd_phy_conn_set, false },
  { "c", &cli_cmd_phy_conn_set, true },
  { "get", &cli_cmd_phy_get, false },
  { "g", &cli_cmd_phy_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_phy = \
  SL_CLI_COMMAND_GROUP(phy_group_table, "PHY settings");

static const sl_cli_command_entry_t connection_group_table[] = {
  { "set", &cli_cmd_connection_set, false },
  { "s", &cli_cmd_connection_set, true },
  { "get", &cli_cmd_connection_get, false },
  { "g", &cli_cmd_connection_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_connection = \
  SL_CLI_COMMAND_GROUP(connection_group_table, "Connection settings");

static const sl_cli_command_entry_t allowlist_group_table[] = {
  { "add", &cli_cmd_allowlist_add, false },
  { "a", &cli_cmd_allowlist_add, true },
  { "clear", &cli_cmd_allowlist_clear, false },
  { "c", &cli_cmd_allowlist_clear, true },
  { "get", &cli_cmd_allowlist_get, false },
  { "g", &cli_cmd_allowlist_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_allowlist = \
  SL_CLI_COMMAND_GROUP(allowlist_group_table, "Allowlist management");

static const sl_cli_command_entry_t throughput_central_group_table[] = {
  { "stop", &cli_cmd_throughput_central_stop, false },
  { "x", &cli_cmd_throughput_central_stop, true },
  { "start", &cli_cmd_throughput_central_start, false },
  { "s", &cli_cmd_throughput_central_start, true },
  { "status", &cli_cmd_throughput_central_status, false },
  { "t", &cli_cmd_throughput_central_status, true },
  { "central_mode", &cli_cmd_grp_central_mode, false },
  { "m", &cli_cmd_grp_central_mode, true },
  { "central_tx_power", &cli_cmd_grp_central_tx_power, false },
  { "p", &cli_cmd_grp_central_tx_power, true },
  { "central_data", &cli_cmd_grp_central_data, false },
  { "d", &cli_cmd_grp_central_data, true },
  { "phy", &cli_cmd_grp_phy, false },
  { "y", &cli_cmd_grp_phy, true },
  { "connection", &cli_cmd_grp_connection, false },
  { "c", &cli_cmd_grp_connection, true },
  { "allowlist", &cli_cmd_grp_allowlist, false },
  { "w", &cli_cmd_grp_allowlist, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_throughput_central = \
  SL_CLI_COMMAND_GROUP(throughput_central_group_table, "Throughput Central");

static const sl_cli_command_entry_t mode_group_table[] = {
  { "set", &cli_cmd_mode_set, false },
  { "s", &cli_cmd_mode_set, true },
  { "get", &cli_cmd_mode_get, false },
  { "g", &cli_cmd_mode_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_mode = \
  SL_CLI_COMMAND_GROUP(mode_group_table, "Mode");

static const sl_cli_command_entry_t power_group_table[] = {
  { "set", &cli_cmd_power_set, false },
  { "s", &cli_cmd_power_set, true },
  { "get", &cli_cmd_power_get, false },
  { "g", &cli_cmd_power_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_power = \
  SL_CLI_COMMAND_GROUP(power_group_table, "Power settings");

static const sl_cli_command_entry_t data_group_table[] = {
  { "set", &cli_cmd_data_set, false },
  { "s", &cli_cmd_data_set, true },
  { "get", &cli_cmd_data_get, false },
  { "g", &cli_cmd_data_get, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_data = \
  SL_CLI_COMMAND_GROUP(data_group_table, "Data settings");

static const sl_cli_command_entry_t throughput_peripheral_group_table[] = {
  { "stop", &cli_cmd_throughput_peripheral_stop, false },
  { "x", &cli_cmd_throughput_peripheral_stop, true },
  { "start", &cli_cmd_throughput_peripheral_start, false },
  { "s", &cli_cmd_throughput_peripheral_start, true },
  { "status", &cli_cmd_throughput_peripheral_status, false },
  { "t", &cli_cmd_throughput_peripheral_status, true },
  { "mode", &cli_cmd_grp_mode, false },
  { "m", &cli_cmd_grp_mode, true },
  { "power", &cli_cmd_grp_power, false },
  { "p", &cli_cmd_grp_power, true },
  { "data", &cli_cmd_grp_data, false },
  { "d", &cli_cmd_grp_data, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_throughput_peripheral = \
  SL_CLI_COMMAND_GROUP(throughput_peripheral_group_table, "Throughput Peripheral");

// Create root command table
const sl_cli_command_entry_t sl_cli_default_command_table[] = {
  { "throughput_central", &cli_cmd_grp_throughput_central, false },
  { "central", &cli_cmd_grp_throughput_central, true },
  { "throughput_peripheral", &cli_cmd_grp_throughput_peripheral, false },
  { "peripheral", &cli_cmd_grp_throughput_peripheral, true },
  { NULL, NULL, false },
};


#ifdef __cplusplus
}
#endif
