/*
 * thercam.c
 *
 * THERMAL CAMERA OPERATIONS I2C
 * Created: 5/4/2019 11:03:42 PM
 *  Author: tghed
 */ 


#include "main.h"
#include "stdio_serial.h"
#include "delay.h"
#include "asf.h"



static uint8_t rd_buffer[DATA_LENGTH];
#define SLAVE_ADDRESS 0x69

struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;

struct i2c_master_module i2c_master_instance;					/* Init software module instance. */



/** 
* Callback after read operation completion 
*/
void i2c_read_complete_callback(struct i2c_master_module *const module)
{
	printf("Thercam: >> One frame read\n\r");
}

/** Register callback for read from slave (Thercam) operation */
void configure_i2c_callbacks(void)
{
	i2c_master_register_callback(&i2c_master_instance, i2c_read_complete_callback,	I2C_MASTER_CALLBACK_READ_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_READ_COMPLETE);
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

/** THERCAM OPERATION */
uint16_t thercam_read(void)
{
	/** Read packet DSA */	
	rd_packet.address     = SLAVE_ADDRESS;
	rd_packet.data_length = DATA_LENGTH;
	rd_packet.data        = rd_buffer;
	
	i2c_master_read_packet_job(&i2c_master_instance,&rd_packet); // Actual Read function call >> will read Data_length 256
				
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
			printf("%d ",camera_buffer[j]);
			k--;
		}
		else
		{
			printf("%d; \n\r",camera_buffer[j]);
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
		
	printf("%d\n\r",correction);
	
	return correction;	
}








