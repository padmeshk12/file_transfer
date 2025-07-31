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
 *            Ping Chen, R&D Shanghai, CR28327
 *            Jiawei Lin, R&D Shanghai, CR27090&CR27346
 *            Jiawei Lin, R&D Shanghai, CR38119
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 28 Jul 1999, Michael Vogt, created
 *            10 May 2001, Siegfried Appich new keys added for bin parity line(s)
 *                         and tester ready signal
 *             1 Nov 2005, Ping Chen, added PHKEY_PL_MIRAE_SEND_SITESEL for
 *                         CR28327.
 *            Nov 2005, Jiawei Lin, CR27090&CR27346
 *              support more request commands for Delta Flex&Castle handler
 *            Jan 2008, Jiawei Lin, CR38119
 *              support more GPIB commands for Mirae M660 handler
 *            Aug 2008, Xiaofei Han, CR33527
 *              added configuration parameter PHKEY_PL_MIRAE_MASK_DEVICE_PENDING and
 *
 *            Jan 2008, Jiawei Lin, CR38119
 *              support more GPIB commands for Mirae M660 handler
 *
 *            Oct 2008, Jiawei Lin, CR42750
 *              support the "gpib_end_of_message_action" parameter
 *
 *            Mar 2009, Roger Feng, CR42643
 *              Support temperature query/config for Mutitest handler 9918 and 9928
 *            7 May 2015, Magco Li, CR93846
 *              add rs232 parameter(baud, parity, databits, stopbits, flowcontol, eoi, error flag)
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

/* 
   The following naming scheme is used for better overview:

   _G_  : global configuration
   _IN_ : driver identification
   _MO_ : driver mode
   _OP_ : operator interactions
   _IF_ : interface selection
   _GB_ : GPIB specific definitions
   _SI_ : site controll
   _BI_ : bin controll
   _RP_ : reprobing
   _PL_ : plugin specific values
   _FC_ : framework configuration (internal)
   _TC_ : temperature control
   _SU_ : equipment setup
*/

#define PHKEY_G_PLUGIN            "driver_plugin"
#define PHKEY_G_CONFIG            "configuration"

#define PHKEY_IN_HFAM             "handler_family"
#define PHKEY_IN_MODEL            "model"
#define PHKEY_IN_SUB_TYPE          "sub_type"

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
#define PHKEY_OP_DEVTOACT         "waiting_for_parts_timeout_action"
#define PHKEY_OP_DEVTOTIME        "waiting_for_parts_timeout"
#define PHKEY_OP_LOTTOACT         "waiting_for_lot_timeout_action"
#define PHKEY_OP_LOTTOTIME        "waiting_for_lot_timeout"
#define PHKEY_OP_GENTOACT         "general_timeout_action"
#define PHKEY_OP_GENTOTIME        "general_timeout"

#define PHKEY_OP_DIAGCOMMST       "dialog_communication_start"
#define PHKEY_OP_DIAGCONFST       "dialog_configuration_start"

#define PHKEY_OP_HAND             "hand_test_mode"
#define PHKEY_OP_SST              "single_step_mode"
#define PHKEY_OP_PAUPROB          "stop_handler_on_smartest_pause"
#define PHKEY_OP_PAUDRV           "pause_smartest_on_handler_stop"

#define PHKEY_IF_TYPE             "interface_type"
#define PHKEY_IF_NAME             "symbolic_interface_name"
#define PHKEY_IF_PORT             "gpib_port"
#define PHKEY_IF_LAN_PORT         "server_port"

#define PHKEY_GB_EOS              "end_of_string"

#define PHKEY_SI_HSIDS            "handler_site_ids"
#define PHKEY_SI_HSMASK           "handler_site_mask"
#define PHKEY_SI_STHSMAP          "smartest_site_to_handler_site_map"

//CR42750
#define PHKEY_GB_EOMACTION        "gpib_end_of_message_action"

#define PHKEY_BI_MODE             "bin_mapping"
#define PHKEY_BI_HRTBINS          "handler_retest_bins"
#define PHKEY_BI_HBIDS            "handler_bin_ids"
#define PHKEY_BI_CATEGORIES       "handler_bin_categories"
#define PHKEY_BI_HARDMAP          "hardbin_to_handler_bin_map"
#define PHKEY_BI_SOFTMAP          "softbin_to_handler_bin_map"
#define PHKEY_BI_VERIFY           "verify_bin_data"
#define PHKEY_BI_VERCOUNT         "verify_retry_count"

#define PHKEY_RP_RDAUTO           "auto_reprobe_on_retest_device"
#define PHKEY_RP_CDAUTO           "auto_reprobe_on_check_device"
#define PHKEY_RP_BIN              "handler_reprobe_bin"
#define PHKEY_RP_AMODE            "automatic_reprobe_mode"
#define PHKEY_RP_RETESTCOUNT      "handler_retest_count"

#define PHKEY_AUTO_RETEST         "auto_retest"

#define PHKEY_PL_SUMMITRECIPE     "summit_recipe_file"
#define PHKEY_PL_SUMMITWA         "summit_workarounds"
#define PHKEY_PL_MIRAE_SEND_SITESEL     "mirae_send_siteselection_cmd"
#define PHKEY_PL_MIRAE_MASK_DEVICE_PENDING    "mirae_mask_device_pending"   /* added 20Aug2008 Vincent&Xiaofei */

#define PHKEY_PL_ADVANTEST_BINNING_COMMAND_FORMAT "advantest_binning_command_format" 
#define PHKEY_PL_SE_BINNING_COMMAND_FORMAT "se_binning_command_format" /* for Seiko-Epson model */
#define PHKEY_PL_DELTA_BINNING_COMMAND_FORMAT "delta_binning_command_format" /* for Delta Eclipse model */
#define PHKEY_PL_HONTECH_REQUEST_BARCODE "hontech_request_barcode"
#define PHKEY_PL_HONTECH_IS_ECHO_BARCODE "hontech_is_echo_barcode"
#define PHKEY_PL_DELTA_EMULATION_MODE "delta_emulation_mode"

#define PHKEY_FC_HEARTBEAT        "flag_check_interval"
#define PHKEY_FC_BUSYTHRESH       "busy_wait_delay_threshold"
#define PHKEY_FC_WFPMODE          "waiting_for_parts_method"
#define PHKEY_FC_POLLT            "polling_interval"
#define PHKEY_FC_TESTSET_PARA     "test_tray_test_in_parallel"
#define PHKEY_FC_TESTSET_STEP     "test_tray_step"
#define PHKEY_FC_TESTSET_ROW      "test_tray_row"
#define PHKEY_FC_RUNMODE          "run_mode"
#define PHKEY_FC_MASTER_SOT       "master_sot"

#define PHKEY_TC_CONTROL          "temp_control"
#define PHKEY_TC_CUNIT            "temp_conf_unit"
#define PHKEY_TC_HUNIT            "temp_equipment_unit"
#define PHKEY_TC_CHAMB            "temp_chambers"
#define PHKEY_TC_SET              "temp_setpoint"
#define PHKEY_TC_SOAK             "temp_soaktime"
#define PHKEY_TC_DESOAK           "temp_desoaktime"
#define PHKEY_TC_UGUARD           "temp_upper_guard_band"
#define PHKEY_TC_LGUARD           "temp_lower_guard_band"
#define PHKEY_TC_HEAT             "temp_active_heating"
#define PHKEY_TC_COOL             "temp_active_cooling"

#define PHKEY_SU_SRQMASK          "srq_mask"
#define PHKEY_SU_THERMAL_ALARM_SRQ_LIST          "thermal_alarm_srq_list"

#define PHKEY_NAME_ID_DRIVER          "driver"
#define PHKEY_NAME_ID_PLUGIN          "plugin"
#define PHKEY_NAME_ID_EQUIPMENT       "equipment"
#define PHKEY_NAME_ID_LOT             "lot"
#define PHKEY_NAME_ID_STRIP_INDEX_ID  "index"  /* added Oct 14 2004 kaw */
#define PHKEY_NAME_ID_STRIP_ID        "strip"  /* added Oct 14 2004 kaw */

#define PHKEY_NAME_HANDLER_STATUS_GET            "get"             /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_VERSION    "version"     /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_NAME       "name"        /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_TESTSET    "testset"     /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_SETTEMP    "settemp"     /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_MEASTEMP   "meastemp"    /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_STATUS     "status"      /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_JAM        "jam"         /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_JAMCODE    "jamcode"     /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_JAMQUE     "jamque"      /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_JAMCOUNT   "jamcount"    /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_GET_SETLAMP    "setlamp"     /* added 07Feb2005 kaw */

#define PHKEY_NAME_HANDLER_STATUS_SET            "set"         /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_SET_NAME       "name"        /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_SET_LOADER     "loader"      /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_SET_START      "start"       /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_SET_STOP       "stop"        /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_SET_PLUNGER    "plunger"     /* added 07Feb2005 kaw */
#define PHKEY_NAME_HANDLER_STATUS_SET_RUNMODE    "runmode"     /* added 11Mar2005 kaw */


/* defined for CR38119, for Mirae M660 handler */
#define PHKEY_NAME_HANDLER_STATUS_GET_LOTERRCODE    "loterrcode"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOTALMCODE    "lotalmcode"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOTDATA       "lotdata"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACCSORTDVC    "accsortdvc"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACCJAMCODE    "accjamcode"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACCERRCODE    "accerrcode"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACCDATA       "accdata"
#define PHKEY_NAME_HANDLER_STATUS_GET_TRAYID        "trayid"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPMODE      "tempmode"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOTTOTAL      "lottotal"
#define PHKEY_NAME_HANDLER_STATUS_GET_ALARMRECORD   "alarmrecord"

#define PHKEY_NAME_HANDLER_STATUS_SET_LOTCLEAR      "lotclear"
#define PHKEY_NAME_HANDLER_STATUS_SET_ACCCLEAR      "accclear"
#define PHKEY_NAME_HANDLER_STATUS_SET_CONTACTSEL    "contactsel"
#define PHKEY_NAME_HANDLER_STATUS_SET_SITESEL       "sitesel"  /*same as contactsel*/
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPMODE      "tempmode"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL "temperature_tempcontrol" /*same as tempmode*/

/* defined for DLH*/
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGCYCLE    "templogcycle?"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGLENGTH   "temploglength?"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGSTS      "templogsts"

#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGCYCLE    "templogcycle"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGLENGTH   "temploglength"       
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTART    "templogstart"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTOP     "templogstop"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSEND     "templogsend"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGINI       "pretrgini"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGENABLE    "pretrgenable"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGSIGTYPE   "pretrgsigtype"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGWAITTIME  "pretrgwaittime"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTTIME   "pretrgouttime"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTVALUE  "pretrgoutvalue"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETMETHOD "pretrgretmethod"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETVALUE  "pretrgretvalue"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGTESTEND   "pretrgtestend"
#define PHKEY_NAME_HANDLER_STATUS_SET_SYNCTESTER      "synctester"

/* defined for CR27090 and CR27346, for Delta Flex & Castle handler */
#define PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING                "site_mapping"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_AIRTEMP         "temperature_airtemp"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_COOL            "temperature_cool"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_DESOAKTIME      "temperature_desoaktime"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_HEAT            "temperature_heat"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_LOWERGUARDBAND  "temperature_lowerguardband"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_MASSTEMP        "temperature_masstemp"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT        "temperature_setpoint"
#define PHKEY_NAME_HANDLER_STATUS_GET_SETPOINT_TESTZONE           "temp_default_testzone_for_setpoint"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SOAKTIME        "temperature_soaktime"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPCONTROL     "temperature_tempcontrol"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE     "temperature_temperature"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPSTATUS      "temperature_tempstatus"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TESTTEMP        "temperature_testtemp"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_UPPERGUARDBAND  "temperature_upperguardband"


/* CR42623: introduce new commands for Multitest MT9918,MT9928 handler */
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE         "temperature_temperature"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_SOAKTIME            "temperature_soaktime"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_LOWERUPPERGUARDBAND "temperature_lowerupperguardband"


/* added for Advantest/M48 handler */
#define PHKEY_NAME_HANDLER_STATUS_GET_HANDLER_NUMBER                  "handlernum"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACM_CONTACT                     "acmcontact"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACM_ERROR                       "acmerror"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACM_JAM                         "acmjam"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACM_RETRY                       "acmretry"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACM_TIME                        "acmtime"
#define PHKEY_NAME_HANDLER_STATUS_GET_ACM_TOTAL                       "acmtotal"
#define PHKEY_NAME_HANDLER_STATUS_GET_CURRENT_ALM                     "currentalm"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOT_ERROR                       "loterror"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOT_JAM                         "lotjam"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOT_RETRY                       "lotretry"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOT_TIME                        "lottime"
#define PHKEY_NAME_HANDLER_STATUS_GET_LOT_TOTAL                       "lottotal"

#define PHKEY_NAME_HANDLER_STATUS_SET_CHANGE_PAGE                     "changepage"
#define PHKEY_NAME_HANDLER_STATUS_SET_GPIB_TIMEOUT                    "gpibtimeout"
#define PHKEY_NAME_HANDLER_STATUS_SET_SOUND_OFF                       "soundoff"
#define PHKEY_NAME_HANDLER_STATUS_SET_SOFT_RESET                      "softreset"
#define PHKEY_NAME_HANDLER_STATUS_SET_ACM_CLEAR                       "acmclear"
#define PHKEY_NAME_HANDLER_STATUS_SET_ACM_RETEST_CLEAR                "acmretestclear"
#define PHKEY_NAME_HANDLER_STATUS_SET_LOT_RETEST_CLEAR                "lotretestclear"
#define PHKEY_NAME_HANDLER_STATUS_SET_PROFILE                         "profile"
#define PHKEY_NAME_HANDLER_STATUS_SET_SITE_PROFILE                    "siteprofile"
#define PHKEY_NAME_HANDLER_STATUS_SET_LOT_END                         "lotend"
#define PHKEY_NAME_HANDLER_STATUS_SET_INPUT_QTY                       "inputqty"

/*added for DLT enhance*/
#define PHKEY_NAME_HANDLER_STATUS_GET_LDPOCKET                        "ldpocket"
#define PHKEY_NAME_HANDLER_STATUS_GET_LDTRAY                          "ldtray"

/*added for Yushan enhance*/
#define PHKEY_NAME_HANDLER_STATUS_GET_TLOOP                           "tloop"
#define PHKEY_NAME_HANDLER_STATUS_GET_TSSET                           "tsset"
#define PHKEY_NAME_HANDLER_STATUS_GET_DVID                            "dvid"

/* get device id */
#define PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID                        "deviceid"

/* added for HonTech handler (which is re-using the Seiko-Epson driver) */
#define PHKEY_NAME_HANDLER_STATUS_GET_SETSOAK                          "setsoak"
#define PHKEY_NAME_HANDLER_STATUS_GET_SETSITEMAP                       "setsitemap"
#define PHKEY_NAME_HANDLER_STATUS_GET_TESTARM                          "testarm"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPARM                          "temparm"
#define PHKEY_NAME_HANDLER_STATUS_GET_FORCE                            "force"
#define PHKEY_NAME_HANDLER_STATUS_GET_BINCOUNT                         "bincount"
#define PHKEY_NAME_HANDLER_STATUS_GET_BARCODE                          "barcode"
/*add for MCT/HF2000 handler*/
#define PHKEY_NAME_HANDLER_STATUS_GET_XY_COORDINATES                   "xy_coordinates"
#define PHKEY_NAME_HANDLER_STATUS_GET_X_COORDINATE                     "x_coordinate"
#define PHKEY_NAME_HANDLER_STATUS_GET_Y_COORDINATE                     "y_coordinate"
#define PHKEY_NAME_HANDLER_STATUS_GET_X_COORDINATE_INT                 "x_coordinate_int"

/* added for the SPEA H3570 handler */
#define PHKEY_NAME_HANDLER_STATUS_SET_SABON                            "socket_air_blow_on"
#define PHKEY_NAME_HANDLER_STATUS_SET_SABOFF                           "socket_air_blow_off"

/* added for the Ismeca NY20/NX16 handler*/
#define PHKEY_NAME_RS232_BAUDRATE                                      "rs232_baudRate"
#define PHKEY_NAME_RS232_PARITY                                        "rs232_parity"
#define PHKEY_NAME_RS232_DATABITS                                      "rs232_dataBits"
#define PHKEY_NAME_RS232_STOPBITS                                      "rs232_stopBits"
#define PHKEY_NAME_RS232_FLOWCONTROL                                   "rs232_flowControl"
#define PHKEY_NAME_RS232_EOIS                                          "rs232_eois"
#define PHKEY_NAME_ISMECA_HANDLER_CA_ABORT                             "ismeca_handler_abort_error_ca_response"

/* added for sending SECS protocol message */
#define PHKEY_NAME_IS_SECS                                              "is_secs_message"           




/*--- typedefs --------------------------------------------------------------*/

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

#endif /* ! _PH_KEYS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
