#include <errno.h>
#include "asf.h"
#include "main.h"
#include "stdio_serial.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "iot/http/http_client.h"
#include "MQTTClient/Wrapper/mqtt.h"

#include <stdio.h>
#include "crc32.h"
#include "nvm.h"
#include "sd_mmc_spi.h"
#include "delay.h"
#include <string.h>




volatile char mqtt_msg [64]= "{\"d\":{\"temp\":17}}\"";
volatile uint32_t temperature = 1;

#define STRING_EOL                      "\r\n"
#define STRING_HEADER                   STRING_EOL"-- IOT SMART KNOB CONTROLLER --"STRING_EOL \
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

/*HTTP DOWNLOAD RELATED DEFINES AND VARIABLES*/

uint8_t do_download_flag = false; //Flag that when true initializes a download. False to connect to MQTT broker
/** File download processing state. */
static download_state down_state = NOT_READY;
/** SD/MMC mount. */
static FATFS fatfs;
/** File pointer for file download. */
static FIL file_object;
/** Http content length. */
static uint32_t http_file_size = 0;
/** Receiving content length. */
static uint32_t received_file_size = 0;
/** File name to download. */
static char save_file_name[MAIN_MAX_FILE_NAME_LENGTH + 1] = "0:";



/** UART module for debug. */
static struct usart_module cdc_uart_module;

/** Instance of Timer module. */
struct sw_timer_module swt_module_inst;

/** Instance of HTTP client module. */
struct http_client_module http_client_module_inst;

/*MQTT RELATED DEFINES AND VARIABLES*/

/** User name of chat. */
char mqtt_user[64] = "Unit1";

/* Instance of MQTT service. */
static struct mqtt_module mqtt_inst;

/* Receive buffer of the MQTT service. */
static unsigned char mqtt_read_buffer[MAIN_MQTT_BUFFER_SIZE];
static unsigned char mqtt_send_buffer[MAIN_MQTT_BUFFER_SIZE];


/**--------------------------CUSTOM DEFINES and our VARIABLES-----------------------*/

/** All NVM address */
#define APP_START_ADDRESS ((uint32_t)0x9C00)								// Application address
#define APP_START_RESET_VEC_ADDRESS (APP_START_ADDRESS+(uint32_t)0x04)		
#define VERSION_ADDRESS ((uint32_t)0x9A00)									// Version address
#define OTAFU_ADDRESS ((uint32_t)0x9B00)									// OTAFU address	

#define VERSION 1
#define FIRMWARE 2
#define CRC 3

char* desired_url = NULL;

/** ALL GLOBAL VARIABLES */
bool otafu_flag = false;

char test_file_name[] = "0:Firmware.bin";
char ver_file_name[] = "0:Version.txt";
char crc_file_name[] = "0:Crc.txt";

FRESULT res1;

char sd_version_num[1];
uint8_t nvm_version_num;
	
enum status_code error_code;

struct nvm_config nvm_cfg;

uint8_t page_buffer[NVMCTRL_PAGE_SIZE];
uint8_t page_buffer1[NVMCTRL_PAGE_SIZE];

/**-----------------------------------------------------------------------------------------------*/

/*HTPP RELATED STATIOC FUNCTIONS*/

/**
 * \brief Initialize download state to not ready.
 */
static void init_state(void)
{
	down_state = NOT_READY;
}

/**
 * \brief Clear state parameter at download processing state.
 * \param[in] mask Check download_state.
 */
static void clear_state(download_state mask)
{
	down_state &= ~mask;
}

/**
 * \brief Add state parameter at download processing state.
 * \param[in] mask Check download_state.
 */
static void add_state(download_state mask)
{
	down_state |= mask;
}

/**
 * \brief File download processing state check.
 * \param[in] mask Check download_state.
 * \return true if this state is set, false otherwise.
 */

static inline bool is_state_set(download_state mask)
{
	return ((down_state & mask) != 0);
}

/**
 * \brief File existing check.
 * \param[in] fp The file pointer to check.
 * \param[in] file_path_name The file name to check.
 * \return true if this file name is exist, false otherwise.
 */
static bool is_exist_file(FIL *fp, const char *file_path_name)
{
	if (fp == NULL || file_path_name == NULL) {
		return false;
	}

	FRESULT ret = f_open(&file_object, (char const *)file_path_name, FA_OPEN_EXISTING);
	f_close(&file_object);
	return (ret == FR_OK);
}

/**
 * \brief Make to unique file name.
 * \param[in] fp The file pointer to check.
 * \param[out] file_path_name The file name change to uniquely and changed name is returned to this buffer.
 * \param[in] max_len Maximum file name length.
 * \return true if this file name is unique, false otherwise.
 */
static bool rename_to_unique(FIL *fp, char *file_path_name, uint8_t max_len)
{
	#define NUMBRING_MAX (3)
	#define ADDITION_SIZE (NUMBRING_MAX + 1) /* '-' character is added before the number. */
	uint16_t i = 1, name_len = 0, ext_len = 0, count = 0;
	char name[MAIN_MAX_FILE_NAME_LENGTH + 1] = {0};
	char ext[MAIN_MAX_FILE_EXT_LENGTH + 1] = {0};
	char numbering[NUMBRING_MAX + 1] = {0};
	char *p = NULL;
	bool valid_ext = false;

	if (file_path_name == NULL) {
		return false;
	}

	if (!is_exist_file(fp, file_path_name)) {
		return true;
	} 
	else if (strlen(file_path_name) > MAIN_MAX_FILE_NAME_LENGTH) {
		return false;
	}

	p = strrchr(file_path_name, '.');
	if (p != NULL) {
		ext_len = strlen(p);
		if (ext_len < MAIN_MAX_FILE_EXT_LENGTH) {
			valid_ext = true;
			strcpy(ext, p);
			if (strlen(file_path_name) - ext_len > MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE) {
				name_len = MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE - ext_len;
				strncpy(name, file_path_name, name_len);
			} 
			else {
				name_len = (p - file_path_name);
				strncpy(name, file_path_name, name_len);
			}
		} 
		else {
			name_len = MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE;
			strncpy(name, file_path_name, name_len);
		}
	} 
	else {
		name_len = MAIN_MAX_FILE_NAME_LENGTH - ADDITION_SIZE;
		strncpy(name, file_path_name, name_len);
	}

	name[name_len++] = '-';

	for (i = 0, count = 1; i < NUMBRING_MAX; i++) {
		count *= 10;
	}
	for (i = 1; i < count; i++) {
		sprintf(numbering, MAIN_ZERO_FMT(NUMBRING_MAX), i);
		strncpy(&name[name_len], numbering, NUMBRING_MAX);
		if (valid_ext) {
			strcpy(&name[name_len + NUMBRING_MAX], ext);
		}

		if (!is_exist_file(fp, name)) {
			memset(file_path_name, 0, max_len);
			strcpy(file_path_name, name);
			return true;
		}
	}
	return false;
}

/**
 * \brief Start file download via HTTP connection.
 */
static void start_download(void)
{
	if (!is_state_set(STORAGE_READY)) {
		printf("start_download: MMC storage not ready.\r\n");
		return;
	}

	if (!is_state_set(WIFI_CONNECTED)) {
		printf("start_download: Wi-Fi is not connected.\r\n");
		return;
	}

	if (is_state_set(GET_REQUESTED)) {
		printf("start_download: request is sent already.\r\n");
		return;
	}

	if (is_state_set(DOWNLOADING)) {
		printf("start_download: running download already.\r\n");
		return;
	}

	/* Send the HTTP request. */
	printf("start_download: sending HTTP request...\r\n");
	//http_client_send_request(&http_client_module_inst, MAIN_HTTP_FILE_URL, HTTP_METHOD_GET, NULL, NULL);
	http_client_send_request(&http_client_module_inst, desired_url, HTTP_METHOD_GET, NULL, NULL);
}

/**
 * \brief Store received packet to file.
 * \param[in] data Packet data.
 * \param[in] length Packet data length.
 */
static void store_file_packet(char *data, uint32_t length)
{
	FRESULT ret;
	if ((data == NULL) || (length < 1)) {
		printf("store_file_packet: empty data.\r\n");
		return;
	}

	if (!is_state_set(DOWNLOADING)) {
		char *cp = NULL;
		save_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
		save_file_name[1] = ':';
		//cp = (char *)(MAIN_HTTP_FILE_URL + strlen(MAIN_HTTP_FILE_URL));
		cp = (char *)(desired_url + strlen(desired_url));
		while (*cp != '/') {
			cp--;
		}
		if (strlen(cp) > 1) {
			cp++;
			strcpy(&save_file_name[2], cp);
		} else {
			printf("store_file_packet: file name is invalid. Download canceled.\r\n");
			add_state(CANCELED);
			return;
		}

		rename_to_unique(&file_object, save_file_name, MAIN_MAX_FILE_NAME_LENGTH);
		printf("store_file_packet: creating file [%s]\r\n", save_file_name);
		ret = f_open(&file_object, (char const *)save_file_name, FA_CREATE_ALWAYS | FA_WRITE);
		if (ret != FR_OK) {
			printf("store_file_packet: file creation error! ret:%d\r\n", ret);
			return;
		}

		received_file_size = 0;
		add_state(DOWNLOADING);
	}

	if (data != NULL) {
		UINT wsize = 0;
		ret = f_write(&file_object, (const void *)data, length, &wsize);
		if (ret != FR_OK) {
			f_close(&file_object);
			add_state(CANCELED);
			printf("store_file_packet: file write error, download canceled.\r\n");
			return;
		}

		received_file_size += wsize;
		printf("store_file_packet: received[%lu], file size[%lu]\r\n", (unsigned long)received_file_size, (unsigned long)http_file_size);
		if (received_file_size >= http_file_size) {
			f_close(&file_object);
			printf("store_file_packet: file downloaded successfully.\r\n");
			port_pin_set_output_level(LED_0_PIN, false);
			add_state(COMPLETED);
			return;
		}
	}
}

/**
 * \brief Callback of the HTTP client.
 *
 * \param[in]  module_inst     Module instance of HTTP client module.
 * \param[in]  type            Type of event.
 * \param[in]  data            Data structure of the event. \refer http_client_data
 */
static void http_client_callback(struct http_client_module *module_inst, int type, union http_client_data *data)
{
	switch (type) {
	case HTTP_CLIENT_CALLBACK_SOCK_CONNECTED:
		printf("http_client_callback: HTTP client socket connected.\r\n");
		break;

	case HTTP_CLIENT_CALLBACK_REQUESTED:
		printf("http_client_callback: request completed.\r\n");
		add_state(GET_REQUESTED);
		break;

	case HTTP_CLIENT_CALLBACK_RECV_RESPONSE:
		printf("http_client_callback: received response %u data size %u\r\n",
				(unsigned int)data->recv_response.response_code,
				(unsigned int)data->recv_response.content_length);
		if ((unsigned int)data->recv_response.response_code == 200) {
			http_file_size = data->recv_response.content_length;
			received_file_size = 0;
		} 
		else {
			add_state(CANCELED);
			return;
		}
		if (data->recv_response.content_length <= MAIN_BUFFER_MAX_SIZE) {
			store_file_packet(data->recv_response.content, data->recv_response.content_length);
			add_state(COMPLETED);
		}
		break;

	case HTTP_CLIENT_CALLBACK_RECV_CHUNKED_DATA:
		store_file_packet(data->recv_chunked_data.data, data->recv_chunked_data.length);
		if (data->recv_chunked_data.is_complete) {
			add_state(COMPLETED);
		}

		break;

	case HTTP_CLIENT_CALLBACK_DISCONNECTED:
		printf("http_client_callback: disconnection reason:%d\r\n", data->disconnected.reason);

		/* If disconnect reason is equal to -ECONNRESET(-104),
		 * It means the server has closed the connection (timeout).
		 * This is normal operation.
		 */
		if (data->disconnected.reason == -EAGAIN) {
			/* Server has not responded. Retry immediately. */
			if (is_state_set(DOWNLOADING)) {
				f_close(&file_object);
				clear_state(DOWNLOADING);
			}

			if (is_state_set(GET_REQUESTED)) {
				clear_state(GET_REQUESTED);
			}

			start_download();
		}

		break;
	}
}

/**
 * \brief Callback to get the data from socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg socket event type. Possible values are:
 *  - SOCKET_MSG_BIND
 *  - SOCKET_MSG_LISTEN
 *  - SOCKET_MSG_ACCEPT
 *  - SOCKET_MSG_CONNECT
 *  - SOCKET_MSG_RECV
 *  - SOCKET_MSG_SEND
 *  - SOCKET_MSG_SENDTO
 *  - SOCKET_MSG_RECVFROM
 * \param[in] pvMsg is a pointer to message structure. Existing types are:
 *  - tstrSocketBindMsg
 *  - tstrSocketListenMsg
 *  - tstrSocketAcceptMsg
 *  - tstrSocketConnectMsg
 *  - tstrSocketRecvMsg
 */
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	http_client_socket_event_handler(sock, u8Msg, pvMsg);
}

/**
 * \brief Callback for the gethostbyname function (DNS Resolution callback).
 * \param[in] pu8DomainName Domain name of the host.
 * \param[in] u32ServerIP Server IPv4 address encoded in NW byte order format. If it is Zero, then the DNS resolution failed.
 */
static void resolve_cb(uint8_t *pu8DomainName, uint32_t u32ServerIP)
{
	printf("resolve_cb: %s IP address is %d.%d.%d.%d\r\n\r\n", pu8DomainName,
			(int)IPV4_BYTE(u32ServerIP, 0), (int)IPV4_BYTE(u32ServerIP, 1),
			(int)IPV4_BYTE(u32ServerIP, 2), (int)IPV4_BYTE(u32ServerIP, 3));
	http_client_socket_resolve_handler(pu8DomainName, u32ServerIP);
}

/**
 * \brief Callback to get the Wi-Fi status update.
 *
 * \param[in] u8MsgType type of Wi-Fi notification. Possible types are:
 *  - [M2M_WIFI_RESP_CURRENT_RSSI](@ref M2M_WIFI_RESP_CURRENT_RSSI)
 *  - [M2M_WIFI_RESP_CON_STATE_CHANGED](@ref M2M_WIFI_RESP_CON_STATE_CHANGED)
 *  - [M2M_WIFI_RESP_CONNTION_STATE](@ref M2M_WIFI_RESP_CONNTION_STATE)
 *  - [M2M_WIFI_RESP_SCAN_DONE](@ref M2M_WIFI_RESP_SCAN_DONE)
 *  - [M2M_WIFI_RESP_SCAN_RESULT](@ref M2M_WIFI_RESP_SCAN_RESULT)
 *  - [M2M_WIFI_REQ_WPS](@ref M2M_WIFI_REQ_WPS)
 *  - [M2M_WIFI_RESP_IP_CONFIGURED](@ref M2M_WIFI_RESP_IP_CONFIGURED)
 *  - [M2M_WIFI_RESP_IP_CONFLICT](@ref M2M_WIFI_RESP_IP_CONFLICT)
 *  - [M2M_WIFI_RESP_P2P](@ref M2M_WIFI_RESP_P2P)
 *  - [M2M_WIFI_RESP_AP](@ref M2M_WIFI_RESP_AP)
 *  - [M2M_WIFI_RESP_CLIENT_INFO](@ref M2M_WIFI_RESP_CLIENT_INFO)
 * \param[in] pvMsg A pointer to a buffer containing the notification parameters
 * (if any). It should be casted to the correct data type corresponding to the
 * notification type. Existing types are:
 *  - tstrM2mWifiStateChanged
 *  - tstrM2MWPSInfo
 *  - tstrM2MP2pResp
 *  - tstrM2MAPResp
 *  - tstrM2mScanDone
 *  - tstrM2mWifiscanResult
 */
static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
	switch (u8MsgType) {
	case M2M_WIFI_RESP_CON_STATE_CHANGED:
	{
		tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
		if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
			printf("wifi_cb: M2M_WIFI_CONNECTED\r\n");
			m2m_wifi_request_dhcp_client();
		} else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
			printf("wifi_cb: M2M_WIFI_DISCONNECTED\r\n");
			clear_state(WIFI_CONNECTED);
			if (is_state_set(DOWNLOADING)) {
				f_close(&file_object);
				clear_state(DOWNLOADING);
			}

			if (is_state_set(GET_REQUESTED)) {
				clear_state(GET_REQUESTED);
			}


			/* Disconnect from MQTT broker. */
			/* Force close the MQTT connection, because cannot send a disconnect message to the broker when network is broken. */
			mqtt_disconnect(&mqtt_inst, 1);

			m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID),
					MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);
		}

		break;
	}

	case M2M_WIFI_REQ_DHCP_CONF:
	{
		uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
		printf("wifi_cb: IP address is %u.%u.%u.%u\r\n",
				pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
		add_state(WIFI_CONNECTED);

		if(do_download_flag == 1)
		{
			start_download();
		}
		else
		{
				/* Try to connect to MQTT broker when Wi-Fi was connected. */
		if (mqtt_connect(&mqtt_inst, main_mqtt_broker))
		{
			printf("Error connecting to MQTT Broker!\r\n");
		}
		}
	}
		break;
	

	default:
		break;
	}
}

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
		add_state(STORAGE_READY);
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

/**
 * \brief Configure Timer module.
 */
static void configure_timer(void)
{
	struct sw_timer_config swt_conf;
	sw_timer_get_config_defaults(&swt_conf);

	sw_timer_init(&swt_module_inst, &swt_conf);
	sw_timer_enable(&swt_module_inst);
}

/**
 * \brief Configure HTTP client module.
 */
struct http_client_config httpc_conf;
static void configure_http_client(void)
{
	//struct http_client_config httpc_conf;
	int ret;

	http_client_get_config_defaults(&httpc_conf);

	httpc_conf.recv_buffer_size = MAIN_BUFFER_MAX_SIZE;
	httpc_conf.timer_inst = &swt_module_inst;

	ret = http_client_init(&http_client_module_inst, &httpc_conf);
	if (ret < 0) {
		printf("configure_http_client: HTTP client initialization failed! (res %d)\r\n", ret);
		while (1) {
		} /* Loop forever. */
	}

	http_client_register_callback(&http_client_module_inst, http_client_callback);
}





/*MQTT RELATED STATIC FUNCTIONS*/

/** Prototype for MQTT subscribe Callback */
void SubscribeHandler(MessageData *msgData);

/**
 * \brief Callback to get the Socket event.
 *
 * \param[in] Socket descriptor.
 * \param[in] msg_type type of Socket notification. Possible types are:
 *  - [SOCKET_MSG_CONNECT](@ref SOCKET_MSG_CONNECT)
 *  - [SOCKET_MSG_BIND](@ref SOCKET_MSG_BIND)
 *  - [SOCKET_MSG_LISTEN](@ref SOCKET_MSG_LISTEN)
 *  - [SOCKET_MSG_ACCEPT](@ref SOCKET_MSG_ACCEPT)
 *  - [SOCKET_MSG_RECV](@ref SOCKET_MSG_RECV)
 *  - [SOCKET_MSG_SEND](@ref SOCKET_MSG_SEND)
 *  - [SOCKET_MSG_SENDTO](@ref SOCKET_MSG_SENDTO)
 *  - [SOCKET_MSG_RECVFROM](@ref SOCKET_MSG_RECVFROM)
 * \param[in] msg_data A structure contains notification informations.
 */
static void socket_event_handler(SOCKET sock, uint8_t msg_type, void *msg_data)
{
	mqtt_socket_event_handler(sock, msg_type, msg_data);
}


/**
 * \brief Callback of gethostbyname function.
 *
 * \param[in] doamin_name Domain name.
 * \param[in] server_ip IP of server.
 */
static void socket_resolve_handler(uint8_t *doamin_name, uint32_t server_ip)
{
	mqtt_socket_resolve_handler(doamin_name, server_ip);
}


/**
 * \brief Callback to receive the subscribed Message.
 *
 * \param[in] msgData Data to be received.
 */

void SubscribeHandler(MessageData *msgData)
{
	/* You received publish message which you had subscribed. */
	/* Print Topic and message */
	printf("\r\n %.*s",msgData->topicName->lenstring.len,msgData->topicName->lenstring.data);
	printf(" >> ");
	printf("%.*s",msgData->message->payloadlen,(char *)msgData->message->payload);	

	//Handle LedData message
	if(strncmp((char *) msgData->topicName->lenstring.data, LED_TOPIC, msgData->message->payloadlen) == 0)
	{
		if(strncmp((char *)msgData->message->payload, LED_TOPIC_LED_OFF, msgData->message->payloadlen) == 0)
		{
			port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
		} 
		else if (strncmp((char *)msgData->message->payload, LED_TOPIC_LED_ON, msgData->message->payloadlen) == 0)
		{
			port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		}
	}
}


/**
 * \brief Callback to get the MQTT status update.
 *
 * \param[in] conn_id instance id of connection which is being used.
 * \param[in] type type of MQTT notification. Possible types are:
 *  - [MQTT_CALLBACK_SOCK_CONNECTED](@ref MQTT_CALLBACK_SOCK_CONNECTED)
 *  - [MQTT_CALLBACK_CONNECTED](@ref MQTT_CALLBACK_CONNECTED)
 *  - [MQTT_CALLBACK_PUBLISHED](@ref MQTT_CALLBACK_PUBLISHED)
 *  - [MQTT_CALLBACK_SUBSCRIBED](@ref MQTT_CALLBACK_SUBSCRIBED)
 *  - [MQTT_CALLBACK_UNSUBSCRIBED](@ref MQTT_CALLBACK_UNSUBSCRIBED)
 *  - [MQTT_CALLBACK_DISCONNECTED](@ref MQTT_CALLBACK_DISCONNECTED)
 *  - [MQTT_CALLBACK_RECV_PUBLISH](@ref MQTT_CALLBACK_RECV_PUBLISH)
 * \param[in] data A structure contains notification informations. @ref mqtt_data
 */
static void mqtt_callback(struct mqtt_module *module_inst, int type, union mqtt_data *data)
{
	switch (type) {
	case MQTT_CALLBACK_SOCK_CONNECTED:
	{
		/*
		 * If connecting to broker server is complete successfully, Start sending CONNECT message of MQTT.
		 * Or else retry to connect to broker server.
		 */
		if (data->sock_connected.result >= 0) {
			printf("\r\nConnecting to Broker...");
			if(0 != mqtt_connect_broker(module_inst, 1, CLOUDMQTT_USER_ID, CLOUDMQTT_USER_PASSWORD, CLOUDMQTT_USER_ID, NULL, NULL, 0, 0, 0))
			{
				printf("MQTT  Error - NOT Connected to broker\r\n");
			}
			else
			{
				printf("MQTT Connected to broker\r\n");
			}
		} else {
			printf("Connect fail to server(%s)! retry it automatically.\r\n", main_mqtt_broker);
			mqtt_connect(module_inst, main_mqtt_broker); /* Retry that. */
		}
	}
	break;

	case MQTT_CALLBACK_CONNECTED:
		if (data->connected.result == MQTT_CONN_RESULT_ACCEPT) {
			/* Subscribe chat topic. */
			mqtt_subscribe(module_inst, TEMPERATURE_TOPIC, 2, SubscribeHandler);
			mqtt_subscribe(module_inst, LED_TOPIC, 2, SubscribeHandler);
			/* Enable USART receiving callback. */
			
			printf("MQTT Connected\r\n");
		} else {
			/* Cannot connect for some reason. */
			printf("MQTT broker decline your access! error code %d\r\n", data->connected.result);
		}

		break;

	case MQTT_CALLBACK_DISCONNECTED:
		/* Stop timer and USART callback. */
		printf("MQTT disconnected\r\n");
		usart_disable_callback(&cdc_uart_module, USART_CALLBACK_BUFFER_RECEIVED);
		break;
	}
}



/**
 * \brief Configure MQTT service.
 */
struct mqtt_config mqtt_conf;
static void configure_mqtt(void)
{
	//struct mqtt_config mqtt_conf;
	int result;

	mqtt_get_config_defaults(&mqtt_conf);
	/* To use the MQTT service, it is necessary to always set the buffer and the timer. */
	mqtt_conf.read_buffer = mqtt_read_buffer;
	mqtt_conf.read_buffer_size = MAIN_MQTT_BUFFER_SIZE;
	mqtt_conf.send_buffer = mqtt_send_buffer;
	mqtt_conf.send_buffer_size = MAIN_MQTT_BUFFER_SIZE;
	mqtt_conf.port = CLOUDMQTT_PORT;
	mqtt_conf.keep_alive = 6000;
	
	result = mqtt_init(&mqtt_inst, &mqtt_conf);
	if (result < 0) {
		printf("MQTT initialization failed. Error code is (%d)\r\n", result);
		while (1) {
		}
	}

	result = mqtt_register_callback(&mqtt_inst, mqtt_callback);
	if (result < 0) {
		printf("MQTT register callback failed. Error code is (%d)\r\n", result);
		while (1) {
		}
	}
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
	//Publish some data after a button press and release. Note: just an example! This is not the most elegant way of doing this!
	//temperature++;
	//if (temperature > 40) temperature = 1;
	//snprintf(mqtt_msg, 63, "{\"d\":{\"temp\":%d}}", temperature);	
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

////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 
* OTA NEW FIRMWARE AND METADATA DOWNLOAD > This should be in App code
* This is to be done when SW0 and OTAFU button on cloud is pressed (Firmware update request buttons)
* Services Firmware Update request
*/


// MAIN HTTP INTERACTION FOR DOWNLOAD
int otafu_download_operation(int file_type)
{
	if (file_type == VERSION)
	{
		desired_url = "https://www.seas.upenn.edu/~tghedaoo/Version.txt";	
	}
	else if (file_type == FIRMWARE)
	{
		desired_url = "https://www.seas.upenn.edu/~tghedaoo/Firmware.bin";	
	}
	else if (file_type == CRC)
	{
		desired_url = "https://www.seas.upenn.edu/~tghedaoo/Crc.txt";	
	}
	

	//DOWNLOAD A FILE
	do_download_flag = true;

	// Initialize socket module.
	socketInit();
	// Register socket callback function. 
	registerSocketCallback(socket_cb, resolve_cb);

	//Connect to router. 
	//printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
	//m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

	while (!(is_state_set(COMPLETED) || is_state_set(CANCELED))) {
		// Handle pending events from network controller. 
		m2m_wifi_handle_events(NULL);
		// Checks the timer timeout. 
		sw_timer_task(&swt_module_inst);
	}
	
	clear_state(COMPLETED);
	clear_state(CANCELED);
	
	//Disable socket for HTTP Transfer
	socketDeinit();
}


// Version Check > If actually the firmware is to be downloaded or not
int otafu_version_check()
{
	char sd_download_version_num[1];
	uint8_t nvm_curr_version_num;

	printf("otafu_version_check: Checking for new Version ...... \n\r");
	
	// 1. Erase Version.txt file from SD card
	ver_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	FRESULT ret = f_unlink((char const *)ver_file_name); 
	
	// 2. Download new Version.txt into SD card
	otafu_download_operation(VERSION);
	
	// 3. Read Version from text file (New)
	ver_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	FRESULT res_ver = f_open(&file_object,(char const *)ver_file_name,FA_READ);
	f_gets(sd_download_version_num,&file_object.fsize,&file_object);
	f_close(&file_object);	
	
	uint8_t sd_version_num_int = atoi(sd_download_version_num);		
	
	// 4. Read Version from NVM (Old)
	do
	{
		error_code = nvm_read_buffer(VERSION_ADDRESS,&nvm_curr_version_num,1);			
	} while (error_code == STATUS_BUSY);
	
	// 5. If New Version > Old Version -> return version number
	if ((nvm_curr_version_num == 255) || (nvm_curr_version_num < sd_version_num_int))
	{
		printf("otafu_version_check: Version Different, Writing new code ..... \n\r");
		return sd_version_num_int;
	}
	else
	{
		printf("otafu_version_check: >> Version Same \n\r");
		return 0;
	}
}


// Firmware Download > Download the new firmware file and also its crc for successful download verification
int otafu_firmware_download()
{
	int crc_on_server = 0;
	int crc_downloaded_in_sd_card = 0;
	printf("otafu_firmware_download: Downloading new firmware ...... \n\r");
	
	// 1. Erase Firmware.bin file from SD card
	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	FRESULT ret_firm = f_unlink((char const *)test_file_name);
	
	// 2. Erase crc_new.txt file from SD card
	crc_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	FRESULT ret_crc = f_unlink((char const *)crc_file_name);
	
	// 4. Download Firmware.bin into SD card
	otafu_download_operation(FIRMWARE);
	
	// 3. Download Crc.txt into SD card
	//otafu_download_operation(CRC);
	
	// 5. crc check on Firmware.bin file in the SD card
	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	res1 = f_open(&file_object,(char const*)test_file_name,FA_READ);
	if (res1 != FR_OK)
	{
		printf("sd operation: >> Opening a file failed\n\r");
		return 0;
	}
	printf("sd operation: >> File open success\n\r");
	
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
		for(uint16_t j=0;j<num_pages;j++)
		{
			f_read(&file_object,page_buffer,NVMCTRL_PAGE_SIZE,&bytes_read);
			if((j==(num_pages-1)) && off_set!=0)
			{
				crc32_recalculate(page_buffer,off_set,&crc_downloaded_in_sd_card);
			}
			else
			{
				crc32_recalculate(page_buffer,NVMCTRL_PAGE_SIZE,&crc_downloaded_in_sd_card);
			}
		}
	}
	f_close(&file_object);
	printf("CRC_DOWN = %u\n\r", (uint32_t*)crc_downloaded_in_sd_card);
	
	// 6. crc on Crc.txt 
	// This should come from CRC file as of now the download is happening correctly
	crc_on_server = 1836692497;
	
	// 7. Compare crc with crc from the
	if (crc_on_server == crc_downloaded_in_sd_card)	
		return 1; 
	else
		return 0;
}


/** 
************************ MAIN OTAFU LOGIC *****************************
*/ 
int otafu_download()
{
	int new_ver_num = 0;
	char user_reponse;
	int download_attempts = 0;

	printf("otafu_download: Downloading update version ..... \n\r");
 	
	/* 
 	if( (new_ver_num = otafu_version_check()) == 0)
 	{
 		printf("otafu_download: >> Resuming application\n\r");
 		return 0;
 	}
	*/
 	
 	printf("otafu_download: New Firmware Available, version: %d \n\r",new_ver_num);
 	
 	USER_INPUT:
 	
 	printf("Would you like to download the new version: (y/n) > ");
 	scanf("%c",&user_reponse);
 	printf("%c\n\r",user_reponse);
	 
 	if (user_reponse == 'n')
 	{
 		printf("otafu_download: >> Resuming application\n\r");
 		return 0;
 	}
 	else if (user_reponse == 'y')
 	{
 		for (download_attempts = 0;download_attempts < 3;download_attempts++)
 		{
 			if (otafu_firmware_download() == 0)
 				printf("otafu_download: attempt [%d] > Downloading failed, trying again \n\r",download_attempts);
 			else
 				return 1;
 		}	
 		printf("otafu_download: >> Downloading failed even after multiple attempts\n\r");
 		printf("otafu_download: >> Resuming application for now\n\r");
 		return 0;
 	}
	else
	{
		printf("invalid response, please try again\n\r");
		goto USER_INPUT;
	}
}

// OTAFU trigger check > MQTT request
void otafu()
{
	if(otafu_download() == 0)   
	{
		return;
	}
	else
	{
		printf(">> New Firmware Downloaded \n\r Device Reseting .... \n\r");
		
		// 1. write otafu_flag in nvm
		uint8_t otaflag = 1;
		
		do
		{
			error_code = nvm_erase_row(OTAFU_ADDRESS);
		} while (error_code == STATUS_BUSY);
		
		do
		{
			error_code = nvm_write_buffer(OTAFU_ADDRESS,&otaflag,1);
		} while (error_code == STATUS_BUSY);
				
		// 2. jump to bootloader // Software reset
		NVIC_SystemReset();		
	}			
}


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
///* ...... MAIN ........ *
////////////////////////////////////////////////////////////////////////////

int main(void)
{
	char input_buffer[50];
	uint8_t response = 0;
	
	/** INITIALIZATING THE BOARD AND PERIPHERALS */
	tstrWifiInitParam param;
	int8_t ret;
	init_state();
	
	system_init();						/* Initialize the board. */	
	configure_console();				/* Initialize the UART console. */
	
	printf(STRING_HEADER);
	printf("\r\nmain: Initializing Board and peripherals for application...... \r\n\r\n");
	
	configure_timer();					/* Initialize the Timer. */	
	configure_http_client();			/* Initialize the HTTP client service. */
	configure_mqtt();					/* Initialize the MQTT service. */
	nm_bsp_init();						/* Initialize the BSP. */
	
	delay_init();						/* Initialize delay */
	
	init_storage();							/* Initialize SD/MMC storage. */
	
	configure_extint_channel();				/*Initialize BUTTON 0 as an external interrupt*/
	configure_extint_callbacks();

	configure_nvm();						/*Initialize NVM */

	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));		// Initialize Wi-Fi parameters structure. 

	param.pfAppWifiCb = wifi_cb;									// Initialize Wi-Fi driver with data and status callbacks. 
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) 
	{
		printf("main: m2m_wifi_init call error! (res %d)\r\n", ret);
		while (1);
	}

	if (SysTick_Config(system_cpu_clock_get_hz() / 1000)) 
	{
		puts("ERR>> Systick configuration error\r\n");
		while (1);
	}
	
	printf("\n\rmain: >> Board and peripherals initialized\n\r");
	
	/** INITIALIZATION COMPLETE */	

	delay_s(2);
	
	//Connect to router. 
	printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
	m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

	while(1)
	{
	
	
		// CLI  
		/* If SW0 is pressed */
		if(isPressed == true) 
		{
			while(response != 1)
			{
				printf("enter command \n\r type 'help' for the command list\n\r> ");
				scanf("%s",input_buffer);
				response = cli(input_buffer);
			}		
		}
		
		//OTAFU 
		///< add MQTT condition also
		//if (MQTT == true)	
		//{
			otafu();
		//}	
	}
	
	


	///////////////////////////////////////////////////////////////////////////////////////////
	/*
	//DOWNLOAD A FILE
	do_download_flag = true;

	// Initialize socket module. 
	socketInit();
	// Register socket callback function. 
	registerSocketCallback(socket_cb, resolve_cb);

	// Connect to router. 
	printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
	m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

	while (!(is_state_set(COMPLETED) || is_state_set(CANCELED))) {
		// Handle pending events from network controller. 
		m2m_wifi_handle_events(NULL);
		// Checks the timer timeout. 
		sw_timer_task(&swt_module_inst);
	}
	printf("main: please unplug the SD/MMC card.\r\n");
	printf("main: done.\r\n");

	//Disable socket for HTTP Transfer
	socketDeinit();
	
	delay_s(1);
	
	//CONNECT TO MQTT BROKER
	do_download_flag = false;    // Flag false indicating that mqtt broker to be contacted 
	
	//Re-enable socket for MQTT Transfer
	socketInit();
	registerSocketCallback(socket_event_handler, socket_resolve_handler);

		// Connect to router. 
	
	if (mqtt_connect(&mqtt_inst, main_mqtt_broker))
	{
		printf("Error connecting to MQTT Broker!\r\n");
	}

	while (1) {

		//Handle pending events from network controller.
		m2m_wifi_handle_events(NULL);
		sw_timer_task(&swt_module_inst);

		if(isPressed)
		{
			//Publish updated temperature data
			mqtt_publish(&mqtt_inst, TEMPERATURE_TOPIC, mqtt_msg, strlen(mqtt_msg), 2, 0);
			isPressed = false;
		}

		//Handle MQTT messages
			if(mqtt_inst.isConnected)
			mqtt_yield(&mqtt_inst, 100);

	}
	
	*/
	return 0;
	
}
