/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_keys.h
 * CREATED  : 28 Jul 1999
 *
 * CONTENTS : All used configuration keys
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR 27092 & CR 25172
 *            Garry Xie,R&D Shanghai, CR28427
 *            Kun Xiao, R&D Shanghai, CR21589
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 28 Jul 1999, Michael Vogt, created
 *            August 2005, Jiawei Lin, CR 27092 & CR 25172
 *              Add constants definition for key names of WCR, Lot Number,
 *              Cassette Status, Chuck Temperature
 *            February 2006, Garry Xie , CR28427
 *              add  four commands to support the query.These commands are listed below:
 *                     "E"      --error code request
 *                     "H"      --Multi-site location NO request response
 *                     "w"      --wafer status request
 *              add 3 commands to set value to the prober.These commands are listed below:
 *                     "em"     --set alarm buzzer
 *           July 2006, kun Xiao, CR21589
 *              Add definition for key names of contact test
 *
 * Instructions:
 *
 * 1) Copy this template to as many .h files as you require
 *
 * 2) Use the command 'make depend' to make the new
 *    source files visible to the makefile utility
 *
 * 3) To support automatic man page (documentation) generation, follow the
 *    instructions given for the function template below.
 * 
 * 4) Put private functions and other definitions into separate header
 * files. (divide interface and private definitions)
 *
 *****************************************************************************/

#ifndef _PH_KEYS_H_
#define _PH_KEYS_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/* the following configuration keys may be used by all handler drivers. 
   
   The following naming scheme is used for better overview:

   _G_  : global configuration
   _IN_ : driver identification
   _MO_ : driver mode
   _OP_ : operator interactions
   _IF_ : interface selection
   _GB_ : GPIB specific definitions
   _SI_ : site controll
   _BI_ : bin controll
   _RP_ : probing
   _PL_ : plugin specific values
   _FC_ : framework configuration (internal)
*/

#define PHKEY_G_PLUGIN            "driver_plugin"
#define PHKEY_G_CONFIG            "configuration"

#define PHKEY_IN_HFAM             "prober_family"
#define PHKEY_IN_MODEL            "model"

#define PHKEY_MO_MSGLOG           "driver_message_log"
#define PHKEY_MO_ERRLOG           "driver_error_log"
#define PHKEY_MO_LOGKEEP          "log_files_to_keep"
#define PHKEY_MO_LOGCONF          "log_configuration"
#define PHKEY_MO_DBGLEV           "debug_level"
#define PHKEY_MO_TRACE            "trace_driver_calls"
#define PHKEY_MO_ONLOFF           "communication_mode"
#define PHKEY_MO_STSIM            "smartest_simulation"
#define PHKEY_MO_PLUGSIM          "plugin_simulation"
#define PHKEY_MO_PREFIXTEXT       "log_prefix_text"
#define PHKEY_MO_REPLY_OF_CMD     "waiting_for_reply_of_cmd"


#define PHKEY_OP_ASKCONF          "ask_for_configuration_confirmation"
#define PHKEY_OP_DIETOACT         "waiting_for_parts_timeout_action"
#define PHKEY_OP_DIETOTIME        "waiting_for_parts_timeout"
#define PHKEY_OP_WAFTOACT         "waiting_for_wafer_timeout_action"
#define PHKEY_OP_WAFTOTIME        "waiting_for_wafer_timeout"
#define PHKEY_OP_CASTOACT         "waiting_for_cassette_timeout_action"
#define PHKEY_OP_CASTOTIME        "waiting_for_cassette_timeout"
#define PHKEY_OP_LOTTOACT         "waiting_for_lot_timeout_action"
#define PHKEY_OP_LOTTOTIME        "waiting_for_lot_timeout"
#define PHKEY_OP_GENTOACT         "general_timeout_action"
#define PHKEY_OP_GENTOTIME        "general_timeout"

#define PHKEY_OP_DIAGCOMMST       "dialog_communication_start"
#define PHKEY_OP_DIAGCONFST       "dialog_configuration_start"
#define PHKEY_OP_DIAGLOTST        "dialog_lot_start"
#define PHKEY_OP_DIAGWAFST        "dialog_wafer_start"

#define PHKEY_OP_UNEXPSRQC        "unexpected_SRQ_action"

/* #define PHKEY_OP_HAND             "hand_test_mode" */
#define PHKEY_OP_SST              "single_step_mode"
#define PHKEY_OP_PAUPROB          "stop_prober_on_smartest_pause"
#define PHKEY_OP_PAUDRV           "pause_smartest_on_prober_stop"

/*CR21589, kunxiao, 10 Jul 2006*/
#define PHKEY_OP_CONTACTTEST      "enable_contact_test"

#define PHKEY_IF_TYPE             "interface_type"
#define PHKEY_IF_NAME             "symbolic_interface_name"
#define PHKEY_IF_PORT             "gpib_port"

#define PHKEY_PR_STEPMODE         "stepping_controlled_by"
#define PHKEY_PR_SUBDIE           "perform_subdie_probing"
#define PHKEY_PR_SDPATTERN        "subdie_step_pattern"

#define PHKEY_GB_IGNSRQ           "ignored_SRQs"
#define PHKEY_GB_EOMACTION        "gpib_end_of_message_action"
#define PHKEY_GB_MAX_BINS         "max_bins_allowed"
#define PHKEY_GB_EOS              "end_of_string"

#define PHKEY_SI_HSIDS            "prober_site_ids"
#define PHKEY_SI_HSMASK           "prober_site_mask"
#define PHKEY_SI_STHSMAP          "smartest_site_to_prober_site_map"
#define PHKEY_SI_DIEOFF           "multi_site_die_offsets"

#define PHKEY_BI_MODE             "bin_mapping"
#define PHKEY_BI_PARTIAL_MODE     "partial_binning_mode"
#define PHKEY_BI_HRTBINS          "prober_retest_bins"
#define PHKEY_BI_HBIDS            "prober_bin_ids"
#define PHKEY_BI_HARDMAP          "hardbin_to_prober_bin_map"
#define PHKEY_BI_SOFTMAP          "softbin_to_prober_bin_map"
#define PHKEY_BI_INKBAD           "ink_bad_dies"
#define PHKEY_BI_DONTBIN          "dont_bin_dies"

#define PHKEY_RP_RDAUTO           "auto_reprobe_on_retest_device"
#define PHKEY_RP_CDAUTO           "auto_reprobe_on_check_device"
#define PHKEY_RP_BIN              "prober_reprobe_bin"

#define PHKEY_RP_CLEANPDIE        "per_die_probe_cleaning_rate"
#define PHKEY_RP_CLEANPWAFER      "per_wafer_probe_cleaning_rate"
#define PHKEY_RP_CLEANPHBIN       "probe_cleaning_hardbins"
#define PHKEY_RP_CLEANPSBIN       "probe_cleaning_softbins"

#define PHKEY_PL_EGRECIPE         "electroglas_recipe_file"
#define PHKEY_PL_EGSENDSETUPCMD   "electroglas_send_setup_cmd"
#define PHKEY_PL_EGQRYDIEPOS      "electroglas_query_die_position"
#define PHKEY_PL_TELREVBIN        "tel_reversed_binning"
#define PHKEY_PL_TELOCMD          "tel_o_command_style"
#define PHKEY_PL_TELIDQRY         "tel_send_id_query"
#define PHKEY_PL_TELWAFABT        "tel_send_wafer_abort"
#define PHKEY_PL_TELBINNINGMETHOD "tel_binning_method"    /* CR 15358, Jiawei Lin @2005/6/6 */
#define PHKEY_PL_TELSENDZ         "tel_send_z_command"  
#define PHKEY_PL_TSKWBCMD         "tsk_wafer_ID_command"
#define PHKEY_PL_TSKWAFIDCHAR     "tsk_wafer_ID_remove_first_char"

#define PHKEY_FC_HEARTBEAT        "flag_check_interval"
#define PHKEY_FC_BUSYTHRESH       "busy_wait_delay_threshold"
#define PHKEY_FC_WFPMODE          "waiting_for_parts_method"
#define PHKEY_FC_POLLT            "polling_interval"

#define PHKEY_NAME_ID_DRIVER      "driver"
#define PHKEY_NAME_ID_PLUGIN      "plugin"
#define PHKEY_NAME_ID_EQUIPMENT   "equipment"
#define PHKEY_NAME_ID_LOT         "lot"
#define PHKEY_NAME_ID_CASS        "cassette"
#define PHKEY_NAME_ID_WAFER       "wafer"
#define PHKEY_NAME_ID_PROBE       "probe_card"


/* CR 27092 & CR 25172 */
#define PHKEY_NAME_PROBER_STATUS_WAFER_SIZE     "wafer_size"
#define PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT     "die_height"
#define PHKEY_NAME_PROBER_STATUS_DIE_WIDTH      "die_width"
#define PHKEY_NAME_PROBER_STATUS_WAFER_UNITS    "wafer_units"
#define PHKEY_NAME_PROBER_STATUS_WAFER_FLAT     "wafer_flat"
#define PHKEY_NAME_PROBER_STATUS_CENTER_X       "center_x"
#define PHKEY_NAME_PROBER_STATUS_CENTER_Y       "center_y"
#define PHKEY_NAME_PROBER_STATUS_POS_X          "pos_x"
#define PHKEY_NAME_PROBER_STATUS_POS_Y          "pos_y"
#define PHKEY_NAME_PROBER_STATUS_LOT_NUMBER     "lot_number"
#define PHKEY_NAME_PROBER_STATUS_WAFER_ID       "wafer_id"
#define PHKEY_NAME_PROBER_STATUS_WAFER_NUMBER   "wafer_number"
#define PHKEY_NAME_PROBER_STATUS_CASS_STATUS    "cass_status"
#define PHKEY_NAME_PROBER_STATUS_CHUCK_TEMP     "chuck_temp"
#define PHKEY_NAME_PROBER_STATUS_PROBER_SETUP_NAME     "prober_setup_name"

/* CR28427 */
#define PHKEY_NAME_PROBER_STATUS_WAFER_STATUS                 "wafer_status"
#define PHKEY_NAME_PROBER_STATUS_MULTISITE_LOCATION_NUMBER    "multisite_location_number"
#define PHKEY_NAME_PROBER_STATUS_ERROR_CODE                   "error_code"
#define PHKEY_NAME_PROBER_STATUS_ALARM_BUZZER                 "alarm_buzzer"

                                                                                      
/* CR21589, Kun Xiao, 7 Jul 2006*/
#define PHKEY_NAME_Z_CONTACT_HEIGHT       "z_contact_height"
#define PHKEY_NAME_PLANARITY_WINDOW       "planarity_window"

/*Begin CR91531, Adam Huang, 24 Nov 2014*/
#define PHKEY_NAME_PROBER_STATUS_CARD_CONTACT_COUNT     "card_contact_count"
#define PHKEY_NAME_PROBER_STATUS_DEVICE_PARAMETER       "device_parameter"
#define PHKEY_NAME_PROBER_STATUS_GROSS_VALUE_REQUEST    "gross_value_request"
/*End CR91531*/

/*--- typedefs --------------------------------------------------------------*/

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

#endif /* ! _PH_KEYS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
