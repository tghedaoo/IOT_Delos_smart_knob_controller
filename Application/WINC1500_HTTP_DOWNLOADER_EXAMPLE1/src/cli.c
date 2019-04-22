/*
 * cli.c
 *
 * Created: 4/21/2019 11:12:36 PM
 *  Author: tghed
 */ 

#include "main.h"
#include "stdio_serial.h"
#include <stdio.h>
#include <string.h>

static char  device_name[50];

int cli(char* input)
{
	
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
		return 1;
	}
	
	///<ver_bl
	else if (strncmp(input,"ver_bl",strlen("ver_bl")) == 0)
	{
		uint8_t major = 1;
		uint8_t minor = 1;
		uint8_t patch = 1;
		printf("Bootloader Firmware version: %d.%d.%d \r\n",major,minor,patch);
		return 1;
	}
	
	///<ver_app
	else if (strncmp(input,"ver_app",strlen("ver_app")) == 0)
	{
		uint8_t major = 4;
		uint8_t minor = 5;
		uint8_t patch = 6;
		printf("Code Firmware version: %d.%d.%d \r\n",major,minor,patch);
		return 1;
	}
	
	///<mac
	else if (strncmp(input,"mac",strlen("mac")) == 0)
	{
		printf("Mac Address: F8-F0-05-F3-F9-9E\r\n");
		return 1;
	}
	
	///<IP
	else if (strncmp(input,"ip",strlen("ip")) == 0)
	{
		printf("IP Address: \r\n");
		return 1;
	}
	
	///<dev Name
	else if (strncmp(input,"devName",strlen("devName")) == 0)
	{
		printf("Name: DELOS INC.\r\n");
		return 1;
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
		printf("Device name set to %s\r\n",device_name);
		return 1;
	}
	
	///<get Device Name
	else if (strncmp(input,"getDeviceName",strlen("getDeviceName")) == 0)
	{
		printf("Device name is %s\r\n",device_name);
		return 1;
	}
	
	///<Error Condition
	else
	{
		printf("ERROR > retry\r\n");
		return 0;
	}	

}

