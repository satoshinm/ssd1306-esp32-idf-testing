/* udp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/




/*
udp_perf example

Using this example to test udp throughput performance.
esp<->esp or esp<->ap

step1:
    init wifi as AP/STA using config SSID/PASSWORD.

step2:
    create a udp server/client socket using config PORT/(IP).
    if server: wating for the first message of client.
    if client: sending a packet to server first.

step3:
    send/receive data to/from each other.
    you can see the info in serial output.
*/


#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"


/* udp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef __UDP_PERF_H__
#define __UDP_PERF_H__



#ifdef __cplusplus
extern "C" {
#endif


/*test options*/
#define EXAMPLE_ESP_WIFI_MODE_AP CONFIG_UDP_PERF_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_UDP_MODE_SERVER CONFIG_UDP_PERF_SERVER //TRUE:server FALSE:client
#define EXAMPLE_ESP_UDP_PERF_TX CONFIG_UDP_PERF_TX //TRUE:send FALSE:receive
#define EXAMPLE_PACK_BYTE_IS 97 //'a'
/*AP info and tcp_server info*/
#define EXAMPLE_DEFAULT_SSID CONFIG_UDP_PERF_WIFI_SSID
#define EXAMPLE_DEFAULT_PWD CONFIG_UDP_PERF_WIFI_PASSWORD
#define EXAMPLE_DEFAULT_PORT CONFIG_UDP_PERF_SERVER_PORT
#define EXAMPLE_DEFAULT_PKTSIZE CONFIG_UDP_PERF_PKT_SIZE
#define EXAMPLE_MAX_STA_CONN 1 //how many sta can be connected(AP mode)

#ifdef CONFIG_UDP_PERF_SERVER_IP
#define EXAMPLE_DEFAULT_SERVER_IP CONFIG_UDP_PERF_SERVER_IP
#else
#define EXAMPLE_DEFAULT_SERVER_IP "192.168.4.1"
#endif /*CONFIG_UDP_PERF_SERVER_IP*/


#define TAG "udp_perf:"

/* FreeRTOS event group to signal when we are connected to WiFi and ready to start UDP test*/
extern EventGroupHandle_t udp_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define UDP_CONNCETED_SUCCESS BIT1

extern int total_data;
extern int success_pack;


//using esp as station
void wifi_init_sta();
//using esp as softap
void wifi_init_softap();

//create a udp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_server();
//create a udp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_client();

//send or recv data task
void send_recv_data(void *pvParameters);

//get socket error code. return: error code
int get_socket_error_code(int socket);

//show socket error code. return: error code
int show_socket_error_reason(int socket);

//check connected socket. return: error code
int check_connected_socket();

//close all socket
void close_socket();





#ifdef __cplusplus
}
#endif


#endif /*#ifndef __UDP_PERF_H__*/


/* udp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


/* FreeRTOS event group to signal when we are connected to WiFi and ready to start UDP test*/
EventGroupHandle_t udp_event_group;


static int mysocket;

static struct sockaddr_in remote_addr;
static unsigned int socklen;

int total_data = 0;
int success_pack = 0;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(udp_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
    	ESP_LOGI(TAG, "event_handler:SYSTEM_EVENT_STA_GOT_IP!");
    	ESP_LOGI(TAG, "got ip:%s\n",
		ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
    	xEventGroupSetBits(udp_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
    	ESP_LOGI(TAG, "station:"MACSTR" join,AID=%d\n",
		MAC2STR(event->event_info.sta_connected.mac),
		event->event_info.sta_connected.aid);
    	xEventGroupSetBits(udp_event_group, WIFI_CONNECTED_BIT);
    	break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
    	ESP_LOGI(TAG, "station:"MACSTR"leave,AID=%d\n",
		MAC2STR(event->event_info.sta_disconnected.mac),
		event->event_info.sta_disconnected.aid);
    	xEventGroupClearBits(udp_event_group, WIFI_CONNECTED_BIT);
    	break;
    default:
        break;
    }
    return ESP_OK;
}


//wifi_init_sta
void wifi_init_sta()
{
    udp_event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_DEFAULT_SSID,
            .password = EXAMPLE_DEFAULT_PWD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s \n",
	    EXAMPLE_DEFAULT_SSID,EXAMPLE_DEFAULT_PWD);
}
//wifi_init_softap
void wifi_init_softap()
{
    udp_event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_DEFAULT_SSID,
            .ssid_len=0,
            .max_connection=EXAMPLE_MAX_STA_CONN,
            .password = EXAMPLE_DEFAULT_PWD,
            .authmode=WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_DEFAULT_PWD) ==0) {
	wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s \n",
    	    EXAMPLE_DEFAULT_SSID, EXAMPLE_DEFAULT_PWD);
}

//create a udp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_server()
{
    ESP_LOGI(TAG, "create_udp_server() port:%d", EXAMPLE_DEFAULT_PORT);
    mysocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (mysocket < 0) {
    	show_socket_error_reason(mysocket);
	return ESP_FAIL;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(EXAMPLE_DEFAULT_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(mysocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    	show_socket_error_reason(mysocket);
	close(mysocket);
	return ESP_FAIL;
    }
    return ESP_OK;
}

//create a udp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_client()
{
    ESP_LOGI(TAG, "create_udp_client()");
    ESP_LOGI(TAG, "connecting to %s:%d",
	    EXAMPLE_DEFAULT_SERVER_IP, EXAMPLE_DEFAULT_PORT);
    mysocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (mysocket < 0) {
    	show_socket_error_reason(mysocket);
	return ESP_FAIL;
    }
    /*for client remote_addr is also server_addr*/
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(EXAMPLE_DEFAULT_PORT);
    remote_addr.sin_addr.s_addr = inet_addr(EXAMPLE_DEFAULT_SERVER_IP);

    return ESP_OK;
}


//send or recv data task
void send_recv_data(void *pvParameters)
{
    ESP_LOGI(TAG, "task send_recv_data start!\n");
    
    int len;
    char databuff[EXAMPLE_DEFAULT_PKTSIZE + 1];

    while(1) {
    memset(databuff, 0, sizeof(databuff));
	len = recvfrom(mysocket, databuff, EXAMPLE_DEFAULT_PKTSIZE, 0, (struct sockaddr *)&remote_addr, &socklen);
	if (len > 0) {
        printf("received data: %s\n", databuff);
	    total_data += len;
	    success_pack++;
	} else {
	    if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
		show_socket_error_reason(mysocket);
	    }
	} /*if (len > 0)*/
    } /*while(1)*/
}


int get_socket_error_code(int socket)
{
    int result;
    u32_t optlen = sizeof(int);
    if(getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen) == -1) {
	ESP_LOGE(TAG, "getsockopt failed");
	return -1;
    }
    return result;
}

int show_socket_error_reason(int socket)
{
    int err = get_socket_error_code(socket);
    ESP_LOGW(TAG, "socket error %d %s", err, strerror(err));
    return err;
}

int check_connected_socket()
{
    int ret;
    ESP_LOGD(TAG, "check connect_socket");
    ret = get_socket_error_code(mysocket);
    if(ret != 0) {
    	ESP_LOGW(TAG, "socket error %d %s", ret, strerror(ret));
    }
    return ret;
}

void close_socket()
{
    close(mysocket);
}


//this task establish a UDP connection and receive data from UDP
static void udp_conn(void *pvParameters)
{
    ESP_LOGI(TAG, "task udp_conn start.");
    /*wating for connecting to AP*/
    xEventGroupWaitBits(udp_event_group, WIFI_CONNECTED_BIT,false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "sta has connected to ap.");
    
    /*create udp socket*/
    int socket_ret;
    
    ESP_LOGI(TAG, "create udp server after 3s...");
    vTaskDelay(3000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "create_udp_server.");
    socket_ret=create_udp_server();
    if(socket_ret == ESP_FAIL) {
	ESP_LOGI(TAG, "create udp socket error,stop.");
	vTaskDelete(NULL);
    }
    
    /*create a task to tx/rx data*/
    TaskHandle_t tx_rx_task;
    xTaskCreate(&send_recv_data, "send_recv_data", 4096, NULL, 4, &tx_rx_task);

    /*waiting udp connected success*/
    xEventGroupWaitBits(udp_event_group, UDP_CONNCETED_SUCCESS,false, true, portMAX_DELAY);
    int bps;
    while (1) {
	total_data = 0;
	vTaskDelay(3000 / portTICK_RATE_MS);//every 3s
	bps = total_data / 3;

	if (total_data <= 0) {
	    int err_ret = check_connected_socket();
	    if (err_ret == -1) {  //-1 reason: low level netif error
		ESP_LOGW(TAG, "udp send & recv stop.\n");
		break;
	    }
	}

#if EXAMPLE_ESP_UDP_PERF_TX
	ESP_LOGI(TAG, "udp send %d byte per sec! total pack: %d \n", bps, success_pack);
#else
	ESP_LOGI(TAG, "udp recv %d byte per sec! total pack: %d \n", bps, success_pack);
#endif /*EXAMPLE_ESP_UDP_PERF_TX*/
    }
    close_socket();
    vTaskDelete(tx_rx_task);
    vTaskDelete(NULL);
}



/**
 * Copyright (c) 2017 Tara Keeling
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <driver/spi_master.h>
#include <driver/i2c.h>
#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "ssd1306.h"
#include "font.h"

#include "iface_esp32_i2c.h"
#include "iface_esp32_spi.h"
#include "iface_virtual.h"


int64_t GetMillis( void ) {
    return esp_timer_get_time( ) / 1000;
}

void FBShiftLeft( struct SSD1306_Device* DeviceHandle, uint8_t* ShiftIn, uint8_t* ShiftOut ) {
    uint8_t* Framebuffer = NULL;
    int Width = 0;
    int Height = 0;
    int y = 0;
    int x = 0;

    NullCheck( DeviceHandle, return );

    Framebuffer = DeviceHandle->Framebuffer;
    Width = DeviceHandle->Width;
    Height = DeviceHandle->Height;

    /* Clear out the first and last rows */
    for ( y = 0; y < ( Width / 8 ); y++ ) {
        /* Copy the column to be destroyed out if a buffer was passed in to hold it */
        if ( ShiftOut != NULL ) {
            ShiftOut[ y ] = Framebuffer[ y * Width ];
        }

        Framebuffer[ y * Width ] = 0;

        /* If the caller passes a buffer of pixels it wants shifted in, use that instead of clearing it */
        Framebuffer[ ( y * Width ) + ( Width - 1 ) ] = ( ShiftIn != NULL ) ? ShiftIn[ y ] : 0;
    }

    /* Shift every column of pixels one column to the left */
    for ( x = 0; x < ( Width - 1 ); x++ ) {
        for ( y = 0; y < ( Height / 8 ); y++ ) {
            Framebuffer[ x + ( y * Width ) ] = Framebuffer[ 1 + x + ( y * Width ) ]; 
        }
    }
}

void DrawPixelInColumn( uint8_t* Column, int y, bool Color ) {
    uint32_t Pixel = ( y & 0x07 );
    uint32_t Page = ( y >> 3 );

    NullCheck( Column, return );

    Column[ Page ] = ( Color == true ) ? Column[ Page ] | BIT( Pixel ) : Column[ Page ] & ~BIT( Pixel );
}

const int RSTPin = 17;
const int DCPin = 19;
const int CSPin = 5;
//const int SCLPin = -1; // i2c
//const int SDAPin = -1; // i2c

struct SSD1306_Device Dev_SPI;
//struct SSD1306_Device Dev_I2C;
struct SSD1306_Device Dev_Span;

void ShiftTask( void* Param ) {
    static uint8_t In[ 8 ];
    static uint8_t Out[ 8 ];
    int64_t Start = 0;
    int64_t End = 0;
    int Delay = 0;

    while ( true ) {
        Start = GetMillis( );
            FBShiftLeft( &Dev_Span, In, Out );
            memcpy( In, Out, sizeof( Out ) );

            //Virt_DeviceBlit( &Dev_Span, &Dev_I2C, MakeRect( 0, 127, 0, 63 ), MakeRect( 0, 127, 0, 63 ) );
            Virt_DeviceBlit( &Dev_Span, &Dev_SPI, MakeRect( 128, 255, 0, 63 ), MakeRect( 0, 127, 0, 63 ) );   

            //SSD1306_Update( &Dev_I2C );
            SSD1306_Update( &Dev_SPI );
        End = GetMillis( );

        /* Sync to 30FPS */
        Delay = 33 - ( int ) ( End - Start );

        if ( Delay <= 0 ) {
            /* More dogs for the watch dog god */
            Delay= 3;
        }

        vTaskDelay( pdMS_TO_TICKS( Delay ) );
    }
}

void app_main( void ) {
    //bool Screen0 = false;
    bool Screen1 = false;

    printf("Initializing...\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    wifi_init_sta();
    xTaskCreate(&udp_conn, "udp_conn", 4096, NULL, 5, NULL);

    printf( "Ready...\n" );

    /*
    if ( ESP32_InitI2CMaster( SDAPin, SCLPin ) ) {
        printf( "i2c master initialized.\n" );

        if ( SSD1306_Init_I2C( &Dev_I2C, 128, 64, 0x3C, 0, ESP32_WriteCommand_I2C, ESP32_WriteData_I2C, NULL ) == 1 ) {
            printf( "i2c display initialized.\n" );
            Screen0 = true;
            
            //SSD1306_SetFont( &Dev_I2C, &Font_Comic_Neue_25x28 );
            //FontDrawAnchoredString( &Dev_I2C, "Smile!", TextAnchor_Center, true );

            //SSD1306_Update( &Dev_I2C );
        }
    }
    */

    if ( ESP32_InitSPIMaster( DCPin ) ) {
        printf( "SPI Master Init OK.\n" );

        if ( ESP32_AddDevice_SPI( &Dev_SPI, 128, 64, CSPin, RSTPin ) == 1 ) {
            printf( "SSD1306 Init OK.\n" );
            Screen1 = true;      

            //SSD1306_SetFont( &Dev_SPI, &Font_Comic_Neue_25x28 );
            //FontDrawAnchoredString( &Dev_SPI, "Okay.", TextAnchor_Center, true );

            //SSD1306_Update( &Dev_SPI );
        }
    }

    //if ( Screen0 == true && Screen1 == true ) {
    if ( Screen1 == true ) {
        if ( Virt_DeviceInit( &Dev_Span, 256, 64 ) == 1 ) {
            printf( "Span created!\n" );

            SSD1306_SetHFlip( &Dev_SPI, true );
            SSD1306_SetVFlip( &Dev_SPI, true );
            SSD1306_SetFont( &Dev_Span, &Font_Liberation_Sans_15x16 );
            FontDrawAnchoredString( &Dev_Span, "Hello ESP32", TextAnchor_Center, true );

            //Virt_DeviceBlit( &Dev_Span, &Dev_I2C, MakeRect( 0, 127, 0, 63 ), MakeRect( 0, 127, 0, 63 ) );
            Virt_DeviceBlit( &Dev_Span, &Dev_SPI, MakeRect( 128, 255, 0, 63 ), MakeRect( 0, 127, 0, 63 ) );

            //SSD1306_Update( &Dev_I2C );
            SSD1306_Update( &Dev_SPI );

            xTaskCreate( ShiftTask, "ShiftTask", 4096, NULL, 3, NULL );
        }
    }
}
