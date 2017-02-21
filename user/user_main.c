#include "mem.h"
#include "ip_addr.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "espconn.h"
#include "os_type.h"
#include "user_interface.h"
#include "user_config.h"

#define user_procTaskPrio		0
#define user_procTaskQueueLen	1
os_event_t	user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

#if 0
static volatile os_timer_t some_timer;

void some_timerfunc(void *arg)
{
	//Do blinky stuff
	if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2)
	{
		//Set GPIO2 to LOW
		gpio_output_set(0, BIT2, BIT2, 0);
	}
	else
	{
		//Set GPIO2 to HIGH
		gpio_output_set(BIT2, 0, BIT2, 0);
	}
}
#endif

//Do nothing function - the only user task we have in our queue
static void ICACHE_FLASH_ATTR
user_procTask(os_event_t *events)
{
	os_delay_us(10);
}

void ICACHE_FLASH_ATTR recv_cb(void *arg, char *data, unsigned short len)
{
	struct espconn *conn = (struct espconn *)arg;

	os_printf("Received %d bytes: %s\n", len, data);

	if (strncmp(data, "on", sizeof("on")) == 0) {
		gpio_output_set(BIT2, 0, BIT2, 0);	// GPIO2 to HIGH
	} else if (strncmp(data, "off", sizeof("off")) == 0) {
		gpio_output_set(0, BIT2, BIT2, 0);	// GPIO2 to LOW
	} else {
		os_printf("Not any?\n");
	}

	espconn_disconnect(conn);
}

void ICACHE_FLASH_ATTR discon_cb(void *arg)
{
	struct espconn *conn = (struct espconn *)arg;

	os_printf("TCP connection disconnected\n");
}

// Called whenever a new connection is made
void ICACHE_FLASH_ATTR server_connect_cb(void *arg)
{
	struct espconn *conn = (struct espconn *)arg;

	os_printf("Got connection from IP " IPSTR "\n", IP2STR(conn->proto.tcp->remote_ip));
	// Do nothing except register callback for receiving data and disconnections
	espconn_regist_recvcb(conn, recv_cb);
	espconn_regist_disconcb(conn, discon_cb);
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
	const char ssid[32] = "";
	const char psk[32] = "";
	struct station_config station_conf;
	struct espconn *server;

	/***** GPIO SETUP *****/
	// Initialize the GPIO subsystem.
	gpio_init();

	//Set GPIO2 to output mode
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

	//Set GPIO2 low
	gpio_output_set(0, BIT2, BIT2, 0);

	#if 0
	//Disarm timer
	os_timer_disarm(&some_timer);
	#endif

	uart_div_modify( 0, UART_CLK_FREQ / ( 115200 ) );

	/***** WIFI SETUP *****/
	wifi_set_opmode(STATION_MODE);
	os_memcpy(&station_conf.ssid, ssid, 32);
	os_memcpy(&station_conf.password, psk, 32);
	wifi_station_set_config(&station_conf);
	wifi_station_connect();

	/***** SERVER SETUP *****/
	server = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset(server, 0, sizeof(struct espconn));

	espconn_create(server);
	server->type = ESPCONN_TCP;
	server->state = ESPCONN_NONE;

	server->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	server->proto.tcp->local_port = 80;

	espconn_regist_connectcb(server, server_connect_cb);

	espconn_accept(server);

	espconn_regist_time(server, 15, 0);

	gpio_output_set(BIT2, 0, BIT2, 0);	// GPIO2 to HIGH

	//Start os task
	system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
