/*
 * cli.c
 *
 * Created: 4/21/2019 11:12:36 PM
 *  Author: tghed
 */ 

#include "main.h"
#include "stdio_serial.h"
#include "MQTTClient/Wrapper/mqtt.h"
#include <stdio.h>
#include <string.h>

#define CLI_BUF_SIZE 25
#define DEV_BUF_SIZE 25

static char  device_name[DEV_BUF_SIZE];

void clear_buffer (uint8_t* buffer);
void read_to_buffer (uint8_t* buffer);


int cli(volatile char* mqtt_msg)
{
	uint8_t input[CLI_BUF_SIZE];

	clear_buffer(input);			
	read_to_buffer(input);				
	
	/* Comparing the input with the available set of commands */
	
	///<help
	if (strncmp(input,"help",strlen("help")) == 0)
	{
		printf("\r\nAvailable Commands:\r\n");
		printf("ver_bl - bootloader version\r\n");
		printf("ver_app - Application code version\r\n");
		printf("mac - mac address\r\n");
		printf("ip - ip address\r\n");
		printf("devName - develor name\r\n");
		printf("setDeviceName <string name> - Device Name set\r\n");
		printf("getDeviceName - Get Device Name\r\n");
		
		clear_buffer(input);
		
		return 0;
	}
	
	///<ver_bl
	else if (strncmp(input,"ver_bl",strlen("ver_bl")) == 0)
	{
		uint8_t major = 1;
		uint8_t minor = 1;
		uint8_t patch = 1;
		printf("\r\nBootloader Firmware version: %d.%d.%d \r\n",major,minor,patch);
		return 0;
	}
	
	///<ver_app
	else if (strncmp(input,"ver_app",strlen("ver_app")) == 0)
	{
		uint8_t major = 4;
		uint8_t minor = 5;
		uint8_t patch = 6;
		printf("\r\nCode Firmware version: %d.%d.%d \r\n",major,minor,patch);
		return 0;
	}
	
	///<mac
	else if (strncmp(input,"mac",strlen("mac")) == 0)
	{
		printf("\r\nMac Address: F8-F0-05-F3-F9-9E\r\n");
		return 0;
	}
	
	///<IP
	else if (strncmp(input,"ip",strlen("ip")) == 0)
	{
		printf("\r\nIP Address: 0.0.0.0 \r\n");
		return 0;
	}
	
	///<dev Name
	else if (strncmp(input,"devName",strlen("devName")) == 0)
	{
		printf("\r\nName : DELOS INC.\r\n");
		return 0;
	}
	
	///<set Device Name
	else if (strncmp(input,"setDeviceName",strlen("setDeviceName")) == 0)
	{
		int iter1 = 14;
		int iter2;
		//Reading string after space
		while(input[iter1] != '\r')
		{
			if(input[iter1] != ' ')
			{
				iter2 = 0;
				while(input[iter1] != '\r')
				{
					device_name[iter2] = input[iter1];
					iter1++;
					iter2++;
				}
				break;
			}
			else
			{
				iter1++;
			}
		}
		printf("\r\nDevice name set to %s\r\n",device_name);
		return 0;
	}
	
	///<get Device Name
	else if (strncmp(input,"getDeviceName",strlen("getDeviceName")) == 0)
	{
		printf("\r\nDevice name is %s\r\n",device_name);
		return 0;
	}
	
	///< temperatute Data publishing 
	else if (strncmp(input,"tempdata",strlen("tempdata")) == 0)
	{
		int temp;
		printf("\nEnter Temperature:");
		scanf("%d",temp);
		snprintf(mqtt_msg, 63, "{\"d\":{\"temp\":%d}}", temp);
		mqtt_publish(&mqtt_inst, TEMP_TOPIC, mqtt_msg, strlen(mqtt_msg), 2, 0);
		return 0;
	}
	
	else if (strncmp(input,"exit",strlen("exit")) == 0)
	{
		printf("\r\nClosing cli ..... \n\r");
		return 1;
	}
	
	///<Error Condition
	else
	{
		printf("\n\rERROR > retry\r\n");
		return 0;
	}	

}

/** 
* Clears the stdin buffer for receiving new command
*/
void clear_buffer (uint8_t* buffer)
{
	for(int i = 0 ; i < CLI_BUF_SIZE; i++)
		buffer[i] = 0;	
}

/**
* Reads char by char from the stdin serial with backspace handling
*/
void read_to_buffer (uint8_t* buffer)
{
	int i = 0;
	
	while(i < CLI_BUF_SIZE)
	{
		buffer[i] = getchar();
		if(buffer[i] == 13)
		{
			break;
		}

		// PLease change putty terminal settings for backspace in terminal option (^H)
		// Backspace condition
		if(buffer[i] == 8)
		{	
			
			if(i != 0)
			{
				// Terminal display handling
				putchar(buffer[i]); // This guy moves the cursor in the terminal (buffer of i contains \b)
				putchar(' ');		// This guy prints a space	
				putchar(buffer[i]); // This guy again goes back to where it should be
				
				//Our buffer handling
				buffer[i] = 0;
				buffer[i-1] = 0;
				
				i--;	
			}
			else
			{
				putchar(buffer[i]);	
				buffer[i] = 0;
			}				
					
			continue;
		}
		
		putchar(buffer[i]);
		i++;
	}
}	

