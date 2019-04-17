
/**************************************************************************//**
* @file        SerialConsole.h
* @ingroup 	   Serial Console
* @brief       This file has the code necessary to run the CLI and Serial Debugger. It initializes an UART channel and uses it to receive command from the user
*				as well as print debug information.
* @details     This file has the code necessary to run the CLI and Serial Debugger. It initializes an UART channel and uses it to receive command from the user
*				as well as print debug information.
*
*				The code in this file will:
*				--Initialize a SERCOM port (SERCOM # ) to be an UART channel operating at 115200 baud/second, 8N1
*				--Register callbacks for the device to read and write characters asycnhronously as required by the CLI
*				--Initialize the CLI and Debug Logger datastructures
*
*				Usage:
*
* @copyright
* @author
* @date        January 26, 2019
* @version		0.1
*****************************************************************************/

#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H


/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include "string.h"
#include "circular_buffer.h"
#include <stdarg.h>
#include <string.h>

/******************************************************************************
* Defines
******************************************************************************/
#define RX_BUFFER_SIZE 256	///<Size of character buffer for RX, in bytes
#define TX_BUFFER_SIZE 256	///<Size of character buffers for TX, in bytes

/******************************************************************************
* Structures and Enumerations
******************************************************************************/
enum eDebugLogLevels {
	LOG_INFO_LVL = 0,	//Logs an INFO message
	LOG_DEBUG_LVL = 1,	//Logs a DEBUG message
	LOG_WARNING_LVL = 2,	//Logs a WARNING MSG
	LOG_ERROR_LVL = 3,	//Logs an Error message
	LOG_FATAL_LVL = 4,	//Logs a FATAL message (a non-recoverable error)
	LOG_OFF_LVL = 5,	//Enum to indicate levels are off
	N_DEBUG_LEVELS = 6	//Max number of log levels
};



/******************************************************************************
* Global Function Declarations
******************************************************************************/
void InitializeSerialConsole(void);
void SerialConsoleWriteString(char * string);
int SerialConsoleReadCharacter(uint8_t *rxChar);
void LogMessage(enum eDebugLogLevels level, const char *format, ...);
void setLogLevel(enum eDebugLogLevels debugLevel);
void CLI_Entercommander();
void CLI_Backspacecommander();
enum eDebugLogLevels getLogLevel(void);

/******************************************************************************
* Local Functions
******************************************************************************/

/******************************************************************************
* Global variables
******************************************************************************/
int check_flag ;                    ///< Check flag variable for checking Enter key and Backspace key
char device_string[RX_BUFFER_SIZE]; ///< Stores Device Name 
char read_string[RX_BUFFER_SIZE];   ///< Stores read characters from cbufRx 

#endif /* SERIAL_CONSOLE_H */


