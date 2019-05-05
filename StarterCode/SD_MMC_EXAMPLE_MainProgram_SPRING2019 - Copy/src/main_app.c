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
#define DATA_LENGTH 256 /** Pattern of 256 bytes found from the Thermal Camera */
#define CAMERA_ARRAY_SIZE 64

static uint8_t wr_buffer[DATA_LENGTH] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

static uint8_t wr_buffer_reversed[DATA_LENGTH] = {
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
};

/** The following 2 variables are important */
static uint8_t rd_buffer[DATA_LENGTH];
#define SLAVE_ADDRESS 0x69

struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;  

struct i2c_master_module i2c_master_instance;					/* Init software module instance. */

void i2c_write_complete_callback(struct i2c_master_module *const module)
{
	i2c_master_read_packet_job(&i2c_master_instance,&rd_packet);
}

/** Callback after read operation completion */
void i2c_read_complete_callback(struct i2c_master_module *const module)
{
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
	
	/** Configurations compatible with SAMW25 and Delos PCB board */
// 	config_i2c_master.pinmux_pad0    = PINMUX_PA08D_SERCOM2_PAD0; // SDA
// 	config_i2c_master.pinmux_pad1    = PINMUX_PA09D_SERCOM2_PAD1; // SCK
	
	config_i2c_master.pinmux_pad0    = PINMUX_PA08C_SERCOM0_PAD0; // SDA
	config_i2c_master.pinmux_pad1    = PINMUX_PA09C_SERCOM0_PAD1; // SCK

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
	/* Register callback function. > Writing to slave*/
// 	i2c_master_register_callback(&i2c_master_instance, i2c_write_complete_callback,	I2C_MASTER_CALLBACK_WRITE_COMPLETE);
// 	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_WRITE_COMPLETE);

	/** Register callback for read from slave (Thercam) operation */
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
	
	i2c_master_read_packet_job(&i2c_master_instance,&rd_packet);
				
		char readed[50];
		int j = 0;
		uint8_t camera_buffer[64];
		for (int i = 0; i < DATA_LENGTH; i++ )
		{
			if (i > 64 && rd_packet.data[i] != 0 && i < 255)
			{
				/** The camera buffer gets populated */
				camera_buffer[j] = rd_packet.data[i];
				
				/** The below code is for printing it as an 8x8 array */
				static uint8_t k = 7;
				if (k)
				{
					sprintf(readed,"%d ",camera_buffer[j]);
					SerialConsoleWriteString(readed);	
					k--;
				}
				else
				{
					sprintf(readed,"%d; \n\r",camera_buffer[j]);
					SerialConsoleWriteString(readed);
					k = 7;
				}
				
				j++;
			}
				
			delay_ms(10);
		}
		
		/** 
		* Calculate average temperature
		* the center 4x4 sensor readings are considered for pinpoint calculation
		* The sensor is placed at 8" height 
		*/
		uint16_t sum_r1 = (camera_buffer[18] + camera_buffer[19] + camera_buffer[20] + camera_buffer[21]);
		uint16_t sum_r2 = (camera_buffer[26] + camera_buffer[27] + camera_buffer[28] + camera_buffer[29]);
		uint16_t sum_r3 = (camera_buffer[34] + camera_buffer[35] + camera_buffer[36] + camera_buffer[37]);
		uint16_t sum_r4 = (camera_buffer[42] + camera_buffer[43] + camera_buffer[44] + camera_buffer[45]);
		uint16_t avg_temperature = (sum_r1 + sum_r2 + sum_r3 + sum_r4)/16; // Average calculation
		
		uint16_t correction = (0.6 * avg_temperature) + 20; // Calibration equation
		
		char temperature[3];
		sprintf(temperature,"%d\n\r",correction);
		SerialConsoleWriteString(temperature);

	
/** Blink LEDS to test the board >> SAMW25 compatible with DElos Board */
// while (1) 
// {
// 		port_pin_toggle_output_level(LED_0_PIN);
//		delay_ms(100);	
// }
}




