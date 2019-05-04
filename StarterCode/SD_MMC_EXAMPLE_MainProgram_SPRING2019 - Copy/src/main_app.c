/**
 * \file
 *
 * \brief SD/MMC card example with FatFs
 *
 * Copyright (c) 2014-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

/**
 * \mainpage SD/MMC Card with FatFs Example
 *
 * \section Purpose
 *
 * This example shows how to implement the SD/MMC stack with the FatFS.
 * It will mount the file system and write a file in the card.
 *
 * The example outputs the information through the standard output (stdio).
 * To know the output used on the board, look in the conf_example.h file
 * and connect a terminal to the correct port.
 *
 * While using Xplained Pro evaluation kits, please attach I/O1 Xplained Pro
 * extension board to EXT1.
 *
 * \section Usage
 *
 * -# Build the program and download it into the board.
 * -# On the computer, open and configure a terminal application.
 * Refert to conf_example.h file.
 * -# Start the application.
 * -# In the terminal window, the following text should appear:
 *    \code
 *     -- SD/MMC Card Example on FatFs --
 *     -- Compiled: xxx xx xxxx xx:xx:xx --
 *     Please plug an SD, MMC card in slot.
 *    \endcode
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */

#include <asf.h>
#include "conf_example.h"
#include <string.h>
#include <system_interrupt.h>
#include "SerialConsole/SerialConsole.h"
#include "conf_board.h"
#include <stdint.h>

//! Structure for UART module connected to EDBG (used for unit test output)
struct usart_module cdc_uart_module;

/** 
* I2C CALLBACK
*/
/** Function definitions */
void i2c_write_complete_callback(struct i2c_master_module *const module);
void configure_i2c(void);
void configure_i2c_callbacks(void);

//#define DATA_LENGTH 8
#define DATA_LENGTH 256
#define CAMERA_ARRAY_SIZE 64

static uint8_t wr_buffer[DATA_LENGTH] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

static uint8_t wr_buffer_reversed[DATA_LENGTH] = {
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
};

static uint8_t rd_buffer[DATA_LENGTH];
//static uint8_t rd_buffer[CAMERA_ARRAY_SIZE];

#define SLAVE_ADDRESS 0x69

struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;  

struct i2c_master_module i2c_master_instance;					/* Init software module instance. */

void i2c_write_complete_callback(struct i2c_master_module *const module)
{
	i2c_master_read_packet_job(&i2c_master_instance,&rd_packet);
}

void i2c_read_complete_callback(struct i2c_master_module *const module)
{
	//i2c_master_read_packet_job(&i2c_master_instance,&rd_packet);
	SerialConsoleWriteString("Reading complete\n\r");
}


/**
* Configure I2c
*/
void configure_i2c(void)
{
	/* Initialize config structure and software module */
	struct i2c_master_config config_i2c_master;
	i2c_master_get_config_defaults(&config_i2c_master);

	/* Change buffer timeout to something longer */
	config_i2c_master.buffer_timeout = 65535; //Check timeout requirements 

// 	#if SAMR30
// 	config_i2c_master.pinmux_pad0    = CONF_MASTER_SDA_PINMUX;
// 	config_i2c_master.pinmux_pad1    = CONF_MASTER_SCK_PINMUX;
// 	#endif

	config_i2c_master.pinmux_pad0    = PINMUX_PA08D_SERCOM2_PAD0; // SDA
	config_i2c_master.pinmux_pad1    = PINMUX_PA09D_SERCOM2_PAD1; // SCK

	/* Initialize and enable device with config */
	while(i2c_master_init(&i2c_master_instance, CONF_I2C_MASTER_MODULE, &config_i2c_master)     \
	!= STATUS_OK);

	i2c_master_enable(&i2c_master_instance);
}


/**
* Configure I2C Callbacks >> For reading 
*/
void configure_i2c_callbacks(void)
{
	/* Register callback function. */
// 	i2c_master_register_callback(&i2c_master_instance, i2c_write_complete_callback,	I2C_MASTER_CALLBACK_WRITE_COMPLETE);
// 	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_register_callback(&i2c_master_instance, i2c_read_complete_callback,	I2C_MASTER_CALLBACK_READ_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_READ_COMPLETE);
}



/**
 * \APPLICATION CODE VERSION 1
 */
int main(void)
{
	//INITIALIZE SYSTEM PERIPHERALS
	system_init();
	delay_init();
	InitializeSerialConsole();
	system_interrupt_enable_global();
	
	/** 
	* I2C
	*/
	configure_i2c();
	configure_i2c_callbacks();
	
	/* Init i2c packet. */
	
	//! [write_packet]
	wr_packet.address     = SLAVE_ADDRESS;
	wr_packet.data_length = DATA_LENGTH;
	wr_packet.data        = wr_buffer;
	//! [write_packet]
	
	//! [read_packet]
	rd_packet.address     = SLAVE_ADDRESS;
	rd_packet.data_length = DATA_LENGTH;
	rd_packet.data        = rd_buffer;
	//! [read_packet]
	
	//SerialConsoleWriteString("** I2C Test **\n\r");	
	i2c_master_read_packet_job(&i2c_master_instance,&rd_packet);
				
		char readed[50];
		int j = 0;
		uint8_t camera_buffer[64];
		for (int i = 0; i < DATA_LENGTH; i++ )
		{
			if (i > 64 && rd_packet.data[i] != 0 && i < 225)
			{
				camera_buffer[j] = rd_packet.data[i];
				
				sprintf(readed,"%d      %d     -\n\r", j, camera_buffer[j]);
				SerialConsoleWriteString(readed);
				
				j++;
			}
				
			delay_ms(10);
		}
		
	

// while (1) 
// {
// 		port_pin_toggle_output_level(LED_0_PIN);
//		delay_ms(100);	
		
		/* Send every other packet with reversed data */
		//! [revert_order]
		/*if (wr_packet.data[0] == 0x00) {
			wr_packet.data = &wr_buffer_reversed[0];
			} else {
			wr_packet.data = &wr_buffer[0];
		}*/
				
// 		i2c_master_write_packet_job(&i2c_master_instance, &wr_packet);
// 		char output[20];
// 		sprintf(output,"Sent to I2C : %d %d %d %d - %d %d %d %d ---\n\r", wr_packet.data[0],wr_packet.data[1],wr_packet.data[2],wr_packet.data[3],wr_packet.data[4],wr_packet.data[5],wr_packet.data[6],wr_packet.data[7]);
// 		SerialConsoleWriteString(output);
// 		
//		i2c_master_read_packet_job(&i2c_master_instance,&rd_packet);
		/*else if (i2c_master_read_packet_job(&i2c_master_instance,&rd_packet) == status_busy)
		{
			serialconsolewritestring("busy\n\r");
		}*/
// 		
// 		char readed[200];
// 		sprintf(readed,"%u %u %u %u - %u %u %u %u ---\n\r", rd_packet.data[0],rd_packet.data[1],rd_packet.data[2],rd_packet.data[3],rd_packet.data[4],rd_packet.data[5],rd_packet.data[6],rd_packet.data[7]);
// 		SerialConsoleWriteString(readed);
	
// 		char readed[1];
// 		for (int i = 0; i < DATA_LENGTH; i++)
// 		{
// 			sprintf(readed,"%d\n\r", rd_packet.data[i]);
// 			SerialConsoleWriteString(readed);	
// 		}
//  }
}




