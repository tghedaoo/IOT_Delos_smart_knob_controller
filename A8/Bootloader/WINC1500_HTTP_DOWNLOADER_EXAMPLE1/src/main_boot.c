#include <errno.h>
#include "asf.h"
#include "main.h"
#include "stdio_serial.h"

#include <stdio.h>
#include "crc32.h"
#include "nvm.h"
#include "sd_mmc_spi.h"
#include "delay.h"
#include "extint.h"
#include <string.h>

#define STRING_EOL                      "\r\n"
#define STRING_HEADER                   STRING_EOL"-- BOOTLOADER --"STRING_EOL \
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL


/** SD/MMC mount. */
static FATFS fatfs;
/** File pointer for file download. */
static FIL file_object;
/** Http content length. */

/** UART module for debug. */
static struct usart_module cdc_uart_module;

/** All NVM address */
#define APP_START_ADDRESS ((uint32_t)0x9C00)								// Application address		
#define APP_START_RESET_VEC_ADDRESS 0x00009C04
#define VERSION_ADDRESS ((uint32_t)0x9A00)									// Version address
#define OTAFU_ADDRESS ((uint32_t)0x9B00)									// OTAFU address	

char test_file_name[] = "0:Firmware.bin";
char ver_file_name[] = "0:Version.txt";

FRESULT res1;

char sd_version_num[1];
uint8_t nvm_version_num;
	
enum status_code error_code;

struct nvm_config nvm_cfg;
	
crc32_t crc_mem;
crc32_t crc_mem1;
	
uint8_t page_buffer[NVMCTRL_PAGE_SIZE];
uint8_t page_buffer1[NVMCTRL_PAGE_SIZE];

bool otafu_flag;
uint8_t nvm_otafu_flag;


/**-----------------------------------------------------------------------------------------------*/

/**
 * \brief Initialize SD/MMC storage.
 */
static void init_storage(void)
{
	FRESULT res;
	Ctrl_status status;

	/* Initialize SD/MMC stack. */
	sd_mmc_init();
	while (true) {
		printf("init_storage: please plug an SD/MMC card in slot...\r\n");

		/* Wait card present and ready. */
		do {
			status = sd_mmc_test_unit_ready(0);
			if (CTRL_FAIL == status) {
				printf("init_storage: SD Card install failed.\r\n");
				printf("init_storage: try unplug and re-plug the card.\r\n");
				while (CTRL_NO_PRESENT != sd_mmc_check(0)) {
				}
			}
		} while (CTRL_GOOD != status);

		printf("init_storage: mounting SD card...\r\n");
		memset(&fatfs, 0, sizeof(FATFS));
		res = f_mount(LUN_ID_SD_MMC_0_MEM, &fatfs);
		if (FR_INVALID_DRIVE == res) {
			printf("init_storage: SD card mount failed! (res %d)\r\n", res);
			return;
		}

		printf("init_storage: SD card mount OK.\r\n");
		return;
	}
}

/**
 * \brief Configure UART console.
 */
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
}


//SETUP FOR EXTERNAL BUTTON INTERRUPT --> Bootloader request
struct extint_chan_conf config_extint_chan;
void configure_extint_channel(void)
{
   // struct extint_chan_conf config_extint_chan;
    extint_chan_get_config_defaults(&config_extint_chan);
    config_extint_chan.gpio_pin           = BUTTON_0_EIC_PIN;
    config_extint_chan.gpio_pin_mux       = BUTTON_0_EIC_MUX;
    config_extint_chan.gpio_pin_pull      = EXTINT_PULL_UP;
    config_extint_chan.detection_criteria = EXTINT_DETECT_FALLING;
    extint_chan_set_config(BUTTON_0_EIC_LINE, &config_extint_chan);
}

void extint_detection_callback(void);
void configure_extint_callbacks(void)
{
    extint_register_callback(extint_detection_callback,
            BUTTON_0_EIC_LINE,
            EXTINT_CALLBACK_TYPE_DETECT);
    extint_chan_enable_callback(BUTTON_0_EIC_LINE,
            EXTINT_CALLBACK_TYPE_DETECT);
}


volatile bool isPressed = false;
void extint_detection_callback(void)
{
	isPressed = true;
}


/***************************************************************************/

/**
* NVM CONFIGURATION
*/
void configure_nvm(void)
{
	nvm_get_config_defaults(&nvm_cfg);	
	nvm_cfg.manual_page_write = false;
	nvm_set_config(&nvm_cfg);
}

/* 
* CHECK BOOT MODE 
*/ 
int check_boot_mode()
{
	printf("boot mode: checking if bootloader or app code is to run ....\n\r");
	
	uint32_t app_check_address;
	uint32_t *app_check_address_ptr;
	
	uint32_t otafu_check_address;
	uint8_t *otafu_check_address_ptr;
	uint32_t ver_check_address;
	uint8_t *ver_check_address_ptr;
	
	app_check_address = APP_START_ADDRESS;
	app_check_address_ptr = (uint32_t *)app_check_address;

	otafu_check_address = OTAFU_ADDRESS;
	otafu_check_address_ptr = (uint8_t *)otafu_check_address;
	
	ver_check_address = VERSION_ADDRESS;
	ver_check_address_ptr = (uint8_t *)ver_check_address;
	

	if (isPressed == true)						// Button is pressed, run bootloader
	{	
		printf("boot mode: >> Bootloader Button pressed \n\r");	
		isPressed = false;
		return 0;
	}

	if (*otafu_check_address_ptr != 0xFF)		// OTAFU requested; run bootloader
	{
		printf("boot mode: >> OTAFU \n\r");
		otafu_flag = true;
		return 0;
	}

	if (*app_check_address_ptr == 0xFFFFFFFF) 	// No application; run bootloader
	{
		printf("boot mode: >> NO APP AVAILABLE \n\r");
		return 0;
	}
	
// 	if (*ver_check_address_ptr == 0xFF)			// Even if application is present, version flag is empty
// 	{
// 		printf("boot mode: >> NO VERSION FLAG \n\r");
// 		return 0;
// 	}
	return 1;
}

///////////////////////////////////////////
/**
 * \DISABLE UART console.
 */
void disable_console()
{
	usart_disable(&cdc_uart_module);
}

/** SD card deinitialization */
void sd_deinit();
//////////////////////////////////////////

/* 
* DEINITIALIZE HARDWARE & PERIPHERALS
*/
void disable_peripherals()
{
 	printf("disable peripherals: Deinitializing peripherals \n\rJumping to app..... \n\r");

 	delay_s(2);

 	system_interrupt_disable_global();
	
 	disable_console();
 	sd_deinit();
	
}


/* 
* JUMP TO APPLICATION CODE 
*/ 
static void jump_to_app(void)
{
	disable_peripherals();
	
	/// Function pointer to application section
	void (*applicationCodeEntry)(void);
	
	/// Rebase stack pointer
	__set_MSP(*(uint32_t *) APP_START_ADDRESS);

	/// Rebase vector table
	SCB->VTOR = ((uint32_t) APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);
	
	/// Set pointer to application section
	applicationCodeEntry = (void (*)(void))(unsigned *)(*(unsigned *)(APP_START_RESET_VEC_ADDRESS));
	
	/// Jump to application
	applicationCodeEntry();
}	

/* 
* ALL SD CARD OPERATIONS 
*/ 
int sd_card_to_nvm_copy()
{	
	printf("sd_card_to_nvm_copy: Reading card ..... \n\r");
	
	/************* Check for Firmware version on SD Card ***************/
	
	ver_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	res1 = f_open(&file_object,(char const *)ver_file_name,FA_READ);
	f_gets(sd_version_num,&file_object.fsize,&file_object);
	f_close(&file_object);	
	
	uint8_t sd_version_num1 = atoi(sd_version_num);		
	
	do
	{
		error_code = nvm_read_buffer(VERSION_ADDRESS,&nvm_version_num,1);			
	} while (error_code == STATUS_BUSY);
		
	printf("SD_VER = %u\n\r", (uint8_t)sd_version_num1);
	printf("NVM_VER = %u\n\r", (uint8_t)nvm_version_num);

	if ((nvm_version_num == 255) || (nvm_version_num < sd_version_num1))
	{
		printf("sd_card_to_nvm_copy: Version Different, Writing new code ..... \n\r");
	}
	else
	{
		printf("sd_card_to_nvm_copy: >> Version Same \n\r");
		jump_to_app();
	}
	
	/*---------------------------------------Version Check Complete ----------------------------*/	
	
	///////////////////////////////////////////////////////////////////
		
	/**************** Open Firmware File ******************/
	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	res1 = f_open(&file_object,(char const*)test_file_name,FA_READ);
	if (res1 != FR_OK)
		{
			printf("sd operation: >> Opening a file failed\n\r");
			return 1;
		}
	printf("sd operation: >> File open success\n\r");
	
	
	/**************** Read one Page at a time, Erase NVM and write to NVM ******************/
	
	printf("sd operation: initiating firmware write to nvm ....... \n\r");

	uint32_t bytes_read = 0;
	uint32_t num_pages=0;
	uint32_t off_set=0;
	uint32_t fw_size= f_size(&file_object);
	uint32_t rem = fw_size%NVMCTRL_PAGE_SIZE;
	if(rem!=0)
	{
		num_pages = (fw_size/NVMCTRL_PAGE_SIZE)+1;
		off_set = fw_size - ((num_pages-1) * NVMCTRL_PAGE_SIZE);
	}
	else
	{
		num_pages = (fw_size/NVMCTRL_PAGE_SIZE);
		off_set = 0;
	}
		
	if (fw_size != 0)
	{
		uint32_t current_page = 0;
		uint32_t curr_address = 0;
		uint16_t rows_clear = fw_size / NVMCTRL_ROW_SIZE;
		uint16_t i;
			
		//Clear NVM
		/** -------------- NVM clearing ----------------------------- */
		//printf("sd operation: erasing nvm location ....... \n\r");
		for (i = 0; i <= rows_clear; i++)
		{
			do
			{
				error_code = nvm_erase_row((APP_START_ADDRESS) + (NVMCTRL_ROW_SIZE * i));	
			} while (error_code == STATUS_BUSY);
		}
			
		//Write to NVM
		/** -------------- NVM writing ----------------------------- */
		//printf("sd operation: writing firmware to nvm ....... \n\r");
		for(uint16_t j=0;j<num_pages;j++)
		{
				f_read(&file_object,page_buffer,NVMCTRL_PAGE_SIZE,&bytes_read);
				if((j==(num_pages-1)) && off_set!=0)
				{
					crc32_recalculate(page_buffer,off_set,&crc_mem);
				}
				else
				{
					crc32_recalculate(page_buffer,NVMCTRL_PAGE_SIZE,&crc_mem);
				}
				
				do
				{
					error_code = nvm_write_buffer(APP_START_ADDRESS+(j*NVMCTRL_PAGE_SIZE),page_buffer,bytes_read);
					
				} while (error_code == STATUS_BUSY);
		}

		// Read for CRC calculation
		/** -------------- CRC NVM calculation ----------------------------- */
		//printf("sd operation: calculating nvm firmware crc ....... \n\r");
		for(uint16_t k=0;k<num_pages;k++)
		{
				do
				{
					error_code = nvm_read_buffer(APP_START_ADDRESS+(k*NVMCTRL_PAGE_SIZE),page_buffer1,NVMCTRL_PAGE_SIZE);	
				
				} while (error_code == STATUS_BUSY);
				
				if((k==(num_pages-1)) && off_set!=0)
				{
					crc32_recalculate(page_buffer1,off_set,&crc_mem1);
				}
				else
				{
					crc32_recalculate(page_buffer1,NVMCTRL_PAGE_SIZE,&crc_mem1);
				}			
		}
	}
	f_close(&file_object);
		
	
	/** -------------- CRC Verification ----------------------------- */
	//printf("sd operation: verfying crc of sd card firmware and nvm firmware ....... \n\r");
	//printf("CRC_MEM = %u\n\r", (uint32_t*)crc_mem);
	//printf("CRC_NVM = %u\n\r", (uint32_t*)crc_mem1);

	if(crc_mem == crc_mem1)
	{
		do
		{
			error_code = nvm_erase_row(VERSION_ADDRESS);
		} while (error_code == STATUS_BUSY);		
		
		do
		{
			error_code = nvm_write_buffer(VERSION_ADDRESS,&sd_version_num1,1);
		} while (error_code == STATUS_BUSY);
		
		printf("sd operation: >> NEW FIRMWARE VERSION UPDATED \n\r");		
		printf("sd operation: >> NEW FIRMWARE WRITTEN SUCCESSFULLY \n\r");
		
		jump_to_app();
	}
	else
	{
		printf("sd operation: >> NEW FIRMWARE WRITE FAILED\n\r");
		return 1;
	}
}
/////////////////////////////////////////////////////////////////////////////
///* ...... MAIN ........ *
////////////////////////////////////////////////////////////////////////////

int main(void)
{
	/** INITIALIZATING THE BOARD AND PERIPHERALS */

	system_init();						/* Initialize the board. */	
	configure_console();				/* Initialize the UART console. */
	
	printf(STRING_HEADER);
	delay_init();
	
	printf("\r\nmain: Initializing Board and peripherals ...... \r\n\r\n");
	
	init_storage();							/* Initialize SD/MMC storage. */
	
	configure_extint_channel();				/*Initialize BUTTON 0 as an external interrupt*/
	configure_extint_callbacks();

	configure_nvm();						/*Initialize NVM */
	
	system_interrupt_enable_global();		

	printf("\n\rmain: >> Board and peripherals initialized\n\r");
	
	/** INITIALIZATION COMPLETE */	

	
	//////////////////////////////////////////////////////////////////////////////////
	/** ----------------BOOTLAODER CODE---------------------*/
	//jump_to_app(); // test
	
	while(1)
	{
		printf("main: Selecting Bootmode (Press button to force boot) \n\r");
		delay_s(2);
		if (check_boot_mode() == 1)
		{
			printf("main: >> APP present \n\r");
			jump_to_app();
		}
		
		// OTAFU request check 
		if (otafu_flag == true)
		{
			printf("main: OTA Firmware detected ..... \n\r");
			nvm_otafu_flag = 255;
			
			do
			{
				error_code = nvm_erase_row(OTAFU_ADDRESS);
			} while (error_code == STATUS_BUSY);
			
			do
			{
				error_code = nvm_write_buffer(OTAFU_ADDRESS,&nvm_otafu_flag,1);
			} while (error_code == STATUS_BUSY);
			
			otafu_flag = false;
		}

		// SD card operation
		if(sd_card_to_nvm_copy() != 1)		
			jump_to_app();
	}	
	return 0;	
}
