/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_.h
 * CREATED  : 14 Jun 1999
 *
 * CONTENTS : 
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jun 1999, Michael Vogt, created
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

#ifndef _PH_GUI_SERVER_H
#define _PH_GUI_SERVER_H

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"

/*--- defines ---------------------------------------------------------------*/

#define PH_GUI_PROCESS_NAME  "hp93000_PH_guiProcess"

/*--- typedefs --------------------------------------------------------------*/
//#ifdef __cplusplus
//extern "C" {
//#endif

typedef enum {
    PH_GUI_COMM_SYNCHRON   /* the gui has the control until 
			      an exit button is pressed   */,
    PH_GUI_COMM_ASYNCHRON  /* the control is returned immediately 
			      after the gui is started */
} phGuiSequenceMode_t;

typedef enum {
    PH_GUI_NO_EVENT = 0         /* no important event occured           */,
    PH_GUI_NO_ERROR	       	/* everything ok in previous action     */,
    PH_GUI_COMTEST_RECEIVED	/* comtest request received             */,
    PH_GUI_IDENTIFY_EVENT	/* answer to identify request           */,
    PH_GUI_ERROR_EVENT	       	/* an error occured in the gui          */,
    PH_GUI_PUSHBUTTON_EVENT	/* a buttonpress event occured          */,
    PH_GUI_EXITBUTTON_EVENT	/* an exit buttonpress has been pressed  */
} phGuiEventType_t;

typedef struct {
    phGuiEventType_t type;
    char name[160];
    int secval;
} phGuiEvent_t;

typedef enum {
    PH_GUI_ERR_OK        = 0 /* no error			      	*/,
    PH_GUI_ERR_OTHER         /* not specified error			*/,
    PH_GUI_ERR_FORK          /* can't fork GUI-process		        */,
    PH_GUI_ERR_MEMORY        /* not enough memory			*/,
    PH_GUI_ERR_TIMEOUT       /* a timeout occured on accessing pipe	*/,
    PH_GUI_ERR_WRDATA        /* received wrong data                     */,
    PH_GUI_ERR_INVALIDE_HDL  /* invalide handle		      	        */,
    PH_GUI_ERR_NOHANDLE      /* no such handle	exists	                */,
    PH_GUI_ERR_PARSEGUI      /* error in gui description	      	*/,
    PH_GUI_ERR_GUININIT      /* gui not initialized        	      	*/
} phGuiError_t;

typedef enum {
    PH_GUI_YES_NO             /* A dialog with a multiline label, a yes and
				 a no button. The title is set to 'confirm' */,
    PH_GUI_YES_NO_CANCEL      /* A dialog with a multiline label, a yes, a
				 cancel and a no button. The title is set 
				 to 'confirm'.                              */,
    PH_GUI_OK_CANCEL          /* A dialog with a multiline label, an ok and
				 a cancel button. The title is set to 
				 'confirm'.                                 */,
    PH_GUI_PLAIN_MESSAGE      /* A dialog with a multiline label and an ok
				 button. The title is not set.              */,
    PH_GUI_WARN_MESSAGE       /* A dialog with a multiline label and an ok
				 button. The title is set to 'warning'.     */,
    PH_GUI_ERROR_MESSAGE      /* A dialog with a multiline label and an ok
				 button. The title is set to 'error'.       */,
    PH_GUI_ERROR_ABORT_RETRY  /* A dialog with a multiline label and an
				 abort and retry button. 
				 The title is set to 'error'.               */
} phGuiOptionDialogType_t;

typedef struct phGuiProcessStruct *phGuiProcessId_t;

/*****************************************************************************
 *
 * Create a userdefined dialog 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function creates a user defined dialog. It is described through the
 * string description. mode sets the communication mode between the application
 * and the GUI. Communication can be synchron (PH_GUI_COMM_SYNCHRON) or
 * asynchron (PH_GUI_COMM_ASYNCHRON). If synchron mode is chosen,
 * phGuiShowDialog() blocks until an exit button is pressed otherwise -
 * in synchron mode - it returns immediately. After return you can get the
 * inscribed data with the phGuiGetData() function or close and destroy the
 * GUI with the phGuiCloseDialog() and the phGuiDestroyDialog() function.
 * phGuiCloseDialog() only hides the dialog; the data remains still accessible.
 * Whereas phGuiDestroyDialog() closes the dialog (if still opened) destroys
 * the Gui-process and frees all allocated memory. In the asynchron mode you
 * can fetch occuring events with the phGuiGetEvent() functions. To reconfigure
 * the GUI on runtime, there exists the phGuiConfigure() function. The shown
 * component data can be changed with phGuiChangeValue().
 * 
 * Description Language Syntax:
 * 
 * Start             -> SimpleGroup
 * 
 * Group             -> Component | Component Group
 * 
 * Component         -> Disabled Widget Tooltip | *
 * 
 * Widget            -> "S" WidgetIdentifier SimpleGroup  |
 *
 * Widget            -> "R" WidgetIdentifier RBGroup      |
 *
 * Widget            -> "T" WidgetIdentifier TBGroup      |
 *
 * Widget            -> "t" WidgetIdentifier ToggleButton |
 *
 * Widget            -> "p" WidgetIdentifier PushButton   |
 *
 * Widget            -> "f" WidgetIdentifier TextField    |
 *
 * Widget            -> "a" WidgetIdentifier TextArea     |
 *
 * Widget            -> "l" WidgetIdentifier              |
 *
 * Widget            -> "L" WidgetIdentifier              |
 * 
 * Widget            -> "o" WidgetIdentifier OptionMenu   |
 * 
 * Widget            -> "b" WidgetIdentifier FileBrowser
 * 
 * WidgetIdentifier  -> ":" Name ":" Label | ":" ":" Label |
 * 
 * WidgetIdentifier  -> ":" Name ":" | ":" ":"
 * 
 * SimpleGroup       -> "{" Orientation Group "}"
 * 
 * Orientation       -> "@h" | "@v" | empty
 * 
 * RBGroup           -> "{" RBGroupList "}" SelectedButton Columns
 * 
 * RBGroupList       -> RadioButton | RadioButton RBGroupList
 * 
 * RadioButton       -> Disabled Name ":" Label
 * 
 * TBGroup           -> "{" TBGroupList "}" Columns
 * 
 * TBGroupList       -> ToggleButtonInBox | ToggleButtonInBox TBGroupList
 * 
 * ToggleButtonInBox -> Enabled Name ":" Label IsSelected
 * 
 * ToggleButton      -> IsSelected
 * 
 * PushButton        -> ExitButton
 * 
 * TextField         -> ":" String
 * 
 * TextArea          -> ":" MultiLineText
 * 
 * OptionMenu        -> TextList SelectedButton
 * 
 * FileBrowser       -> Filter FileSelection TypeMask
 * 
 * Filter            -> ":" RegExp
 * 
 * FileSelection     -> ":" Pathname
 * 
 * TypeMask          -> ":" "D" | empty
 * 
 * TextList          -> "(" TextSubList ")"
 * 
 * TextSubList       -> String | String TextSubList
 * 
 * SelectedButton    -> ":" IntValue
 * 
 * Columns           -> ":" IntValue
 * 
 * Name              -> String
 * 
 * Label             -> String
 * 
 * MultiLineText     -> "`" any chars except \\0 Backquote "`"
 * 
 * String            -> "`" any chars except \\r \\n \\0 Backquote "`"
 * 
 * Tooltip           -> "[" MultiLineText "]" | empty
 * 
 * ExitButton        -> ":" "e" | empty
 * 
 * IsSelected        -> ":" "s" | empty
 * 
 * Disabled          -> "~" | empty
 * 
 *
 * Description Language Semantics and Annotations:
 * Please also refer to the framemaker manual
 *
 * Group:
 * A group consists of components and other groups. 
 *
 * Component:
 * Every component can be enabled or disabled (grayed out). Disabled means,
 * that the component doesn't allow user input. Additionally components can
 * have a tooltip which shows up when the mouse pointer touches the components
 * surface. A glue component is unvisible widget to fill free space. For
 * example when you want to distribute buttons over the width or heights of a
 * dialog with an equal distance to each other, you can place glue components
 * between each pair of the buttons.
 *
 * Widget:
 * can be one of
 * SimpleGroup    - organize components hor. or vertically
 * RBGroup        - choose 1 of n - GroupComponent
 * TBGroup        - choose m of n - GroupComponent
 * ToggleButton   - button with 2 static selection states
 * PushButton     - a simple pushbutton
 * TextField      - single text input line with a label
 * TextArea       - text input with multiple lines and a label
 * Label          - a single text label
 * MultiLineLabel - a multiline text label (label component in
 *                  WidgetIdentifier can be MultiLineText)
 * OptionMenu     - choose 1 of n - pulldown component
 * FileBrowser    - a label, textinput line & filebrowser dialog
 *
 * WidgetIdentifier:
 * Name and Label are optional parameters on defining a component. When the
 * name of a component is defined, it can be accessed and the content can be
 * changed at runtime. The label will be shown in the GUI, when set.
 *
 * SimpleGroup:
 * Simple groups are sets of components which are aligned vertically or
 * horizontaly. When a label is set a frame will be painted around the
 * components with the label as title.  When the label ist an empty backquoted
 * string, the frame won't have a label. A frame will not be painted, when
 * the label is not set.
 *
 * Orientation:
 * The alignment of a SimpleGroup can be horizontal or vertical. When nothing
 * is set horizontal is assumed. The enclosed components of a SimpleGroup will
 * be oriented as defined here.
 *
 * RBGroup:
 * RBGroups makes an 1 of n choice component available. A RBGroup can only
 * handle RadioButtons (they can toggle in and out but just one can/must be
 * selected). The frame is handled like the SimpleGroup does. Alignment is
 * managed through the number of Columns. SelectedButton defines the initial
 * selection.
 *
 * RadioButton:
 * Like all other components RadioButton consists of a Name and a Label
 * and can be Enabled or Disabled.
 *
 * TBGroup:
 * The TBGroup is similar to the RBGroup except that m of n components can
 * be selected.
 *
 * ToggleButtonInBox:
 * Like all other components ToggleButtonInBox consists of a Name and a Label
 * and can be Enabled or Disabled. If IsSelected is set the button will be pressed.
 *
 * ToggleButton:
 * If IsSelected is set the button will be pressed.
 *
 * PushButton:
 * If ExitButton is set phGuiShowGui-functions will return when the button
 * is pressed.
 *
 * TextField:
 * The String defines the default text in a TextField widget.
 *
 * TextArea:
 * The MultiLineText defines the default text in the TextArea widget.
 *
 * OptionMenu:
 * An OptionMenu is like the TBGroup an 1 of n choice component. The entries
 * will be listed in a pulldown component which pops up when the user presses
 * the corresponding button widget.
 *
 * FileBrowser:
 * The FileBrowser component is a set of three widgets oriented horizontally.
 * The Selection-String in the FileBrowser is constructed from the
 * FileSelection and the Filter.
 *
 * Filter:
 * The unix like filter string defines which files/dirs will be shown in
 * the filebrowser.
 *
 * FileSelection:
 * An unix like pathname which will be set as default selection
 *
 * TypeMask:
 * Must be set when you want to select directories instead of files in the
 * filebrowser
 *
 * Tooltip:
 * A hint which will be shown, when the mouse cursor touches the
 * corresponding component
 *
 * ExitButton:
 * Pushbuttons can be defined as exit buttons. Then they will be closed
 * on a button press and will generate an PH_GUI_EXITBUTTON_EVENT instead
 * of a PH_GUI_PUSHBUTTON_EVENT
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiCreateUserDefDialog(
    phGuiProcessId_t *id      /* the resulting Gui-ID                    */,
    phLogId_t logger          /* the data logger to be used              */,
    phGuiSequenceMode_t mode  /* mode in which the gui should be started */,
    const char *description   /* description of the gui-representation   */
    );

/*****************************************************************************
 *
 * Create a predefined option dialog 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function creates a predefined dialog with synchron communication
 *  
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiShowOptionDialog(
    phLogId_t logger              /* the data logger to be used              */,
    phGuiOptionDialogType_t type  /* type of the dialog e.g. OK-Cancel-Dialog
				     (this is a dialog with only 2 Buttons,
                                     OK and Cancel and a simple messagetext) */,
    const char *text              /* this is the shown message text          */,
    char *response                /* the name of the pressed exit button will
                                     be returned in this argument            */
    );

/*****************************************************************************
 *
 * Show the dialog
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function shows a hidden dialog. When the user presses an exit button,
 * the function will return. To define the behavior of the gui when pressing
 * on an exit button you must set the closeOnReturn parameter. If it is set to
 * 1 the GUI will be closed (not destroyed!); if it is set to 0 it will still
 * remain visible. To finally free the allocate mem you have to call
 * phGuiDestroyDialog().
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiShowDialog(
    phGuiProcessId_t process,      /* the Gui-ID */
    int closeOnReturn              /* if set to 1 dialog will close when
				      exit button is pressed */
    );

/*****************************************************************************
 *
 * Show the dialog and get a response
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function is similar to phGuiShowDialog() except that the name of the
 * pressed button will be returned. This function is only for synchronous
 * mode! In asynchronous mode it behaves exactly like phGuiShowDialog() 
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiShowDialogGetResponse(
    phGuiProcessId_t process      /* the Gui-ID                             */,
    int closeOnReturn             /* if set to 1 dialog will close when
				     exit button is pressed                 */,
    char *name                    /* name of pressed button will be stored
				     in name                                */
    );

/*****************************************************************************
 *
 * Close the dialog
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function closes the dialog. The Gui-Process will not be terminated.
 * The data struct will stay accessable furthermore to enable that the
 * application can access the inscribed data. To free finally the allocate
 * memory you have to call phGuiDestroyDialog()
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiCloseDialog(
    phGuiProcessId_t process      /* the Gui-ID */
    );

/*****************************************************************************
 *
 * Kill the GUI-process and free the data struct
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function checks if the gui-process still exists and kills it if  
 * essentially. Afterwards all allocated memory will be freed. If you
 * want to access the data struct furthermore you have to call
 * phGuiCloseDialog() instead of this function.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiDestroyDialog(
    phGuiProcessId_t process      /* the Gui-ID */
    );

/*****************************************************************************
 *
 * Wait for the next event for a maximum of <timeout> seconds and return it.
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * If an event exists in the EventPipe it will be returned. Else 
 * this function blocks for at most timeout seconds until an event occured. 
 * After the timer is expired it returns and reports an timeout error.
 * If timeout is set to zero, the function returns immediately without 
 * waiting. This can be used for polling.
 * Caution: read operations from the pipe can be interrupted from signals. 
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiGetEvent(
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    phGuiEvent_t *event,           /* the variable where the event type and
                                      the triggering buttonname will be stored */
    int timeout                    /* timeout after which phGuiGetEvent returns */
    );

/*****************************************************************************
 *
 * Return the entry of a component
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function requests the inscribed data of the given component. After
 * timeout seconds it cancels and returns an timeout error.
 * If the timeout is set to 0 it is predefined to 60 seconds.
 * For 1 of n components (Radio- and Togglebuttons) "True" or "False" will
 * be returned
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiGetData(
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    const char *componentName,     /* name of the component */
    char *data,                    /* the variable where the data will be stored */
    int timeout                    /* timeout after which phGuiGetData returns */
    );

/*****************************************************************************
 *
 * Modify the Guiform dynamically 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * To change the appearance of a shown dialog, call this function with the
 * changed description and the Id of the dialog which shall be changed. The
 * dialog will then be closed, reconfigured and shown again. Remember that
 * the inscribed data will not be automatically recovered!
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiConfigure(
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    char *description              /* description of the gui-representation 
				      (more info at phGuiCreateUserDefDialog)*/
    );

/*****************************************************************************
 *
 * Change the entry of a component
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function changes the entry of component specified by its name to the
 * string given in newValue. For boolean values enter "True" or "False". Up
 * to now you can modify the data of PushButtons (Label), ToggleButtons,
 * RadioButtons, TextFields, TextAreas, OptionMenus and FileBrowsers.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiChangeValue(
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    const char *componentName,     /* name of the component to change */
    const char *newValue           /* new value */
    );

/*****************************************************************************
 *
 * Enable (activate) component for user input
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Enable (activate) component for user input. Enabled components are 
 * sensitive for user actions. When the component is already
 * enabled or doesn't exist nothing happens. 
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiEnableComponent(
    phGuiProcessId_t process,  /* the Gui-ID */
    const char *componentName  /* name of the component to disable */
    );

/*****************************************************************************
 *
 * Disable (deactivate) component for user input
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 * Disable (deactivate) component for user input. Disabled components are
 * greyed out and insensitive for user actions. When the component is already
 * disabled or doesn't exist nothing happens. 
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiDisableComponent(
    phGuiProcessId_t process, /* the Gui-ID */
    const char *componentName /* name of the component to enable */
    );

/*****************************************************************************
 *
 * Test the communication between client and server
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 * This function tests the communication between client and server by 
 * sending teststring through the pipes
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiComTest(
    phGuiProcessId_t process /* the Gui-ID */
    );

/*****************************************************************************
 *
 * Simulates button press
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * This function simulates the press of a buttons given by the corresponding 
 * name
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiSimulateAction(
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    char *name                     /* buttons on which press will be
				      simulated */
    );

/*****************************************************************************
 *
 * Simulates button presses in interactive mode
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * This function simulates the press of a sequence of buttons given by a
 * NULL terminated list of corresponding names. Use only in interactive mode
 * To simulate buttonpresses in simulation mode, use function 
 * <phGuiSimulationStart()>.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiSimulateActions(
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    char **names                   /* array of buttons on which presses will
				      be simulated */
    );

/*****************************************************************************
 *
 * start simulation mode and define user button presses
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 * This function turns the GuiServer to simulation mode. To simulates a
 * sequence of occuring events, the function expects an array of the
 * corresponding event objects. The return to interactive mode call
 * phGuiSimulationEnd(). After processing the event-array the GuiServer
 * returns to interactiveMode automatically. simulateAll should always be
 * set since it is used for testing purposes only.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiSimulationStart(
    phLogId_t logger,    /* the data logger to be used              */
    int eventCount,      /* the number of events defined in the event array   */
    phGuiEvent_t *event, /* the event array, values will be used one
		            after the other until <eventCount> is hit. Then
		            the GuiServer will turn back to interactive Mode. */
    int simulateAll      /* should always be set to value != 0 */
    );

/*****************************************************************************
 *
 * end simulation mode
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 * This function turns the GuiServer back to interactive mode call.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiSimulationEnd(
    phLogId_t logger /* the data logger to be used              */
);

/*****************************************************************************
 *
 * Display a standard 8 button dialog
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 * This function shows a standard dialog with up to 8 buttons. It is similar
 * to the MsgDisplayMsgGetResponse() function from CPI except that only the
 * used buttons will be shown. You can define a dialog title and wether the
 * quit- and continue-button will be shown or not.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiMsgDisplayMsgGetResponse(
    phLogId_t logger            /* the data logger to be used */,
    int  needQuitAndCont        /* we really need the default QUIT
				   and CONTINUE buttons to be
				   displayed */,
    const char *title           /* title of the dialog */,
    const char *msg             /* message to be displayed */,
    const char *b2s             /* user button 2 string */,
    const char *b3s             /* user button 3 string */,
    const char *b4s             /* user button 4 string */,
    const char *b5s             /* user button 5 string */,
    const char *b6s             /* user button 6 string */,
    const char *b7s             /* user button 7 string */,
    long *response              /* return button selected */
    );

/*****************************************************************************
 *
 * Display a standard 8 button dialog with a textinput field
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 * This function shows a standard dialog with up to 8 buttons and a textinput
 * field. It is similar to the MsgDisplayMsgGetStringResponse() function from
 * CPI except that only the used buttons will be shown. You can define a
 * dialog title and wether the quit- and continue-button will be shown or not.
 *
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiMsgDisplayMsgGetStringResponse(
    phLogId_t logger            /* the data logger to be used */,
    int  needQuitAndCont        /* we really need the default QUIT
				   and CONTINUE buttons to be
				   displayed */,
    const char *title           /* title of the dialog */,
    const char *msg             /* message to be displayed */,
    const char *b2s             /* user button 2 string */,
    const char *b3s             /* user button 3 string */,
    const char *b4s             /* user button 4 string */,
    const char *b5s             /* user button 5 string */,
    const char *b6s             /* user button 6 string */,
    const char *b7s             /* user button 7 string */,
    long *response              /* return button selected */,
    char *string                /* return string */
    );

//#ifdef __cplusplus
//}
//#endif

#endif


