/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app.h"
#include "fs.h"
#include "stdio.h"
#include "sockets.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#ifdef ENABLE_MODBUS_TCP
const osThreadAttr_t tcp_server_Task_attributes = {
  .name = "tcp_server_thread",
  .stack_size = TCP_STACK_SIZE,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t tcp_server_TaskHandle = NULL;
#endif

const osThreadAttr_t httpServerTask_attributes = {
  .name = "httpServerTask",
  .stack_size =  HTTP_SERVER_STACK_SIZE,
  .priority = (osPriority_t) osPriorityNormal1,
};

osThreadId_t httpServerTaskHandle = NULL;

const osThreadAttr_t ws_thread_attr = {
    .name = "WebSocketThread",
    .stack_size = WEBSOCKET_STACK_SIZE,
	.priority = (osPriority_t) osPriorityNormal
};

osThreadId_t ws_thread_id = NULL;

osMessageQueueId_t ws_queue;

static ts_client_socket clients_sock_arr[MAX_TCP_SOCK_CLIENTS] ;

static nmbs_t nmbs;

static nmbs_server_t nmbs_server = {.id = 0x01,.coils = {0},.regs = {0},.input_regs = {0}};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void tcp_server_thread(void* argument);
static void http_server_thread(void* argument);
static void websocket_thread(void *argument);
void generate_ws_accept(const char *key, char *output);
int parse_ws_frame(const char *data, int len, ws_frame_t *frame);
int create_ws_frame(char *buffer, int buflen, const char *payload, int payload_len, uint8_t opcode);
void send_response(int sock, const char *content_type, const char *data, int len);
void broadcast_to_ws_clients(int* clients_sock,const char *message, int len);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
	printf("Stack overflow in %s\r\n",pcTaskName);
	__NOP();
}

void vApplicationMallocFailedHook(void)
{
	printf("Low heap!\r\n");
	__NOP();
}

int _write(int file,char* ptr,int len)
{
	for(int DataIdx=0;DataIdx<len;DataIdx++)
	{
		ITM_SendChar(*ptr++);
	}
	return len;
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  MX_LWIP_Init();

  osDelay(500); //let ITM stabilize
#ifdef ENABLE_MODBUS_TCP
  tcp_server_TaskHandle = osThreadNew(tcp_server_thread, NULL, &tcp_server_Task_attributes);

  if(tcp_server_TaskHandle == NULL)
  {
	  printf("failed to create tcp_server_thread");
	  Error_Handler();
  }
#endif

  httpServerTaskHandle = osThreadNew(http_server_thread, NULL, &httpServerTask_attributes);

  if(httpServerTaskHandle == NULL)
  {
	  printf("failed to create httpServerTask\r\n");
	  Error_Handler();
  }

  ws_thread_id = osThreadNew(websocket_thread, NULL, &ws_thread_attr);

  if(ws_thread_id == NULL)
  {
	  printf("failed to create WebSocketThread\r\n");
	  Error_Handler();
  }

  ws_queue = osMessageQueueNew(4, sizeof(int), NULL);

  if(ws_queue == NULL)
  {
	  printf("failed to create WebSocket queue\r\n");
	  Error_Handler();
  }
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void websocket_thread(void *argument)
{
	printf("Start %s\n\r",osThreadGetName(osThreadGetId()));
    char frame[256];    int clients_sock[MAX_WS_CLIENTS] = {[0 ... (MAX_WS_CLIENTS-1)] =  -1};
    _Bool is_button_rised = true;

    for(;;)
    {
        int new_sock = -1;
        if (osMessageQueueGet(ws_queue, &new_sock, NULL, 0) == osOK)
        {
            for (int idx = 0; idx < MAX_WS_CLIENTS; idx++)
            {
                if (clients_sock[idx] < 0)
                {
                	clients_sock[idx] = new_sock;
                    fcntl(new_sock, F_SETFL, O_NONBLOCK);
                    printf("%s: New WebSocket client connected (%d/%d)\r\n",__func__,idx+1, MAX_WS_CLIENTS);

                    // Send initial state to new client
				   GPIO_PinState led_state = HAL_GPIO_ReadPin(LD1_GPIO_Port, LD1_Pin);
				   char init_msg[64];
				   int len = snprintf(init_msg, sizeof(init_msg),
					   "{\"type\":\"led\",\"state\":%d}",
					   (led_state == GPIO_PIN_SET) ? 1 : 0);
				   broadcast_to_ws_clients(&clients_sock[idx], init_msg, len);
				   break;
                }
                else if(idx >= MAX_WS_CLIENTS - 1 && clients_sock[idx] >= 0)
                {
                    printf("%s: WebSocket client limit reached (%d), rejecting connection...\r\n",__func__, MAX_WS_CLIENTS);
                    const char *resp = "HTTP/1.1 503 Service Unavailable\r\n"
                                    "Connection: close\r\n\r\n"
                                    "Server busy - try again later";
                    write(new_sock, resp, strlen(resp));
                    close(new_sock);
                }
            }
        }

        uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, pdMS_TO_TICKS(100));

        if(flags & 0x01)
        {
		    GPIO_PinState new_led_state  = HAL_GPIO_ReadPin(LD1_GPIO_Port, LD1_Pin);

		    char led_update[64];
			int len = snprintf(led_update, sizeof(led_update),
				"{\"type\":\"led\",\"state\":%d}",
				(new_led_state == GPIO_PIN_SET) ? 1 : 0);

			printf("%s: Broadcasting led state to all clients...\r\n",__func__);

			broadcast_to_ws_clients(clients_sock,led_update,len);
        }

        for (int idx = 0; idx < MAX_WS_CLIENTS; idx++)
        {
            if (clients_sock[idx] < 0)
            	continue;

            int bytes_read = recv(clients_sock[idx], frame, sizeof(frame), 0);

            if (bytes_read > 0)
            {
                ws_frame_t ws_frame;
                if (parse_ws_frame(frame, bytes_read, &ws_frame) > 0)
                {
                    if (ws_frame.opcode == 0x1)
                    {
                        printf("%s: Client %d: %.*s\n",__func__, idx,(int)ws_frame.payload_len, ws_frame.payload_data);
                    }
                    else if (ws_frame.opcode == 0x8)
                    {
                        printf("%s: Client %d disconnected\n",__func__, idx);
                        close(clients_sock[idx]);
                        clients_sock[idx] = -1;
                        continue;
                    }
                }
            }
            else if (bytes_read == 0)
            {
                printf("%s: Client %d disconnected\n",__func__, idx);
                close(clients_sock[idx]);
                clients_sock[idx] = -1;
                continue;
            }
            else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                printf("%s: Client %d error, disconnecting\n",__func__, idx);
                close(clients_sock[idx]);
                clients_sock[idx] = -1;
                continue;
            }

			GPIO_PinState btn_state = HAL_GPIO_ReadPin(B1_USER_GPIO_Port, B1_USER_Pin);

			if(btn_state == GPIO_PIN_SET)
			{
				if(is_button_rised == true)
				{
					char json[64];
					int len = snprintf(json, sizeof(json),"{\"type\":\"button\",\"pressed\":%d}",btn_state);

					broadcast_to_ws_clients(clients_sock,json,len);

					is_button_rised = false;
				}
			}
			else
			{
				if(is_button_rised == false)
				{
					char json[64];
					int len = snprintf(json, sizeof(json),"{\"type\":\"button\",\"pressed\":%d}",btn_state);

					broadcast_to_ws_clients(clients_sock,json,len);

					is_button_rised = true;
				}
			}
        }
        osDelay(10);
    }
}

static void http_server_thread(void* argument)
{
    printf("Start %s\n\r", osThreadGetName(osThreadGetId()));
    int http_sock, max_fd;
    struct sockaddr_in http_addr;
    fd_set read_fds, master_fds;
    int client_sockets[MAX_HTTP_CLIENTS]; // Array to track client sockets
    int idx;

    for (idx = 0; idx < MAX_HTTP_CLIENTS; idx++)
    {
        client_sockets[idx] = -1;
    }

    if ((http_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("%s: Socket creation fail\r\n", __func__);
        osThreadTerminate(osThreadGetId());
    }

    http_addr.sin_family = AF_INET;
    http_addr.sin_addr.s_addr = INADDR_ANY;
    http_addr.sin_port = htons(HTTP_PORT);

    if (bind(http_sock, (struct sockaddr*)&http_addr, sizeof(http_addr)) < 0)
    {
        printf("%s: Bind error\r\n", __func__);
        close(http_sock);
        osThreadTerminate(osThreadGetId());
    }

    listen(http_sock, 16);
    fcntl(http_sock, F_SETFL, O_NONBLOCK);

    FD_ZERO(&master_fds);
    FD_SET(http_sock, &master_fds);
    max_fd = http_sock;

    for (;;)
    {
        // Copy the master set to working set
        read_fds = master_fds;

        // Wait for activity on any socket
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            printf("%s: select error\r\n",__func__);
            continue;
        }

        // Check all sockets for activity
        for (idx = 0; idx <= max_fd; idx++)
        {
            if (FD_ISSET(idx, &read_fds))
            {
                // New connection on server socket
                if (idx == http_sock)
                {
                    struct sockaddr_in http_client;
                    socklen_t http_len = sizeof(http_client);
                    int new_client = accept(http_sock, (struct sockaddr*)&http_client, &http_len);

                    if (new_client >= 0)
                    {
                        printf("%s: New connection accepted %d\r\n",__func__,new_client);

                        struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 }; //connection timeout for this socket
                        setsockopt(new_client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

                        fcntl(new_client, F_SETFL, O_NONBLOCK);

                        int j;
                        for (j = 0; j < MAX_HTTP_CLIENTS-1; j++)
                        {
                            if (client_sockets[j] < 0)
                            {
                                client_sockets[j] = new_client;
                                break;
                            }
                        }

                        printf("%s: j = %d\r\n",__func__,j);

                        if (j > (MAX_HTTP_CLIENTS-1) )
                        {
                            printf("%s: Too many connections. Closing %d...\r\n",__func__,new_client);
                            const char *resp = "HTTP/1.1 503 Service Unavailable\r\n"
                                            "Connection: close\r\n\r\n"
                                            "Server busy - try again later";
                            write(new_client, resp, strlen(resp));
                            close(new_client);
                        }
                        else
                        {
                            FD_SET(new_client, &master_fds);
                            if (new_client > max_fd)
                            {
                                max_fd = new_client;
                            }
                        }
                    }
                    else
                    {
						if (errno == EWOULDBLOCK || errno == EAGAIN)
						{
							osDelay(10);
							continue;
						}
						printf("%s: %d accept error\r\n",__func__,new_client);
                    }
                }
                else
                {
                    struct fs_file file;
                    char request[512] = {0};
                    int bytes_read = recv(idx, request, sizeof(request), 0);

                    if (bytes_read > 0)
                    {
                        request[bytes_read] = '\0';

                        int send_file(int sock, const char *path, const char *content_type)
                        {
                            printf("%s: %s requested by client %d\r\n",osThreadGetName(osThreadGetId()), path,sock);
                            if(fs_open(&file, path) == 0)
                            {
                                char headers[256];
                                int len = snprintf(headers, sizeof(headers),
                                    "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: %s\r\n"
                                    "Content-Length: %d\r\n"
                                    "Connection: close\r\n\r\n",
                                    content_type, file.len);

                                int sent = write(sock, headers, len);
                                if (sent != len)
                                {
                                    printf("%s: Failed to send complete headers to client %d\r\n",osThreadGetName(osThreadGetId()),sock);
                                    fs_close(&file);
                                    return -1;
                                }

                                size_t total_sent = 0;
                                while (total_sent < file.len)
                                {
                                    sent = write(sock, file.data + total_sent, file.len - total_sent);
                                    if (sent <= 0)
                                    {
                                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                        {
                                            osDelay(1);
                                            continue;
                                        }
                                        printf("%s: Failed to send file data to client %d\r\n",osThreadGetName(osThreadGetId()),sock);
                                        fs_close(&file);
                                        return -1;
                                    }
                                    total_sent += sent;
                                }

                                fs_close(&file);
                                printf("%s: %s sent completely (%d bytes) to client %d\n",osThreadGetName(osThreadGetId()), path, total_sent,sock);
                                return 0;
                            }
                            else
                            {
                                printf("%s: fail to open %s\r\n",osThreadGetName(osThreadGetId()), path);
                                return -1;
                            }
                        }

						int file_send_err = 0;
						if (strstr(request, "Upgrade: websocket") && strstr(request, "GET /ws"))
						{
							char *key_start = strstr(request, "Sec-WebSocket-Key: ");
							if (key_start)
							{
								char ws_key[64], accept_key[64];
								key_start += strlen("Sec-WebSocket-Key: ");
								char *key_end = strstr(key_start, "\r\n");
								if (key_end) {
									int key_len = key_end - key_start;
									strncpy(ws_key, key_start, key_len);
									ws_key[key_len] = '\0';

									generate_ws_accept(ws_key, accept_key);

									char response[256];
									int len = snprintf(response, sizeof(response),
										"HTTP/1.1 101 Switching Protocols\r\n"
										"Upgrade: websocket\r\n"
										"Connection: Upgrade\r\n"
										"Sec-WebSocket-Accept: %s\r\n\r\n",
										accept_key);
									write(idx, response, len);

									if (osMessageQueuePut(ws_queue, &idx, 0, 100) != osOK)
									{
										printf("%s: WebSocket client queue full, rejecting connection\r\n",__func__);
				                        close(idx);
				                        FD_CLR(idx, &master_fds);

				                        // Remove from client sockets array
				                        for (int j = 0; j < MAX_HTTP_CLIENTS; j++)
				                        {
				                            if (client_sockets[j] == idx)
				                            {
				                                client_sockets[j] = -1;
				                                break;
				                            }
				                        }
									}
									continue;
								}
							}
						}
						if(strstr(request, "GET /spacerockets.html") || strstr(request, "GET / "))
						{
							file_send_err = send_file(idx,"/spacerockets.html", "text/html");
						}
						else if(strstr(request, "GET /control.html"))
						{
							file_send_err = send_file(idx,"/control.html", "text/html");
						}
						else if(strstr(request, "GET /control.js"))
						{
							file_send_err = send_file(idx,"/control.js", "application/javascript");
						}
						else if(strstr(request, "GET /styles.css"))
						{
							file_send_err = send_file(idx,"/styles.css", "text/css");
						}
						else if(strstr(request, "GET /favicon.ico"))
						{
							file_send_err = send_file(idx,"/favicon.ico", "image/x-icon");
						}
						else if (strstr(request, "GET /led-state"))
						{
							GPIO_PinState led_state = HAL_GPIO_ReadPin(LD1_GPIO_Port, LD1_Pin);

							char json[32];
							int len = snprintf(json, sizeof(json), "{\"state\":%d}",
											  (led_state == GPIO_PIN_SET) ? 1 : 0);

							char response[128];
							snprintf(response, sizeof(response),
								"HTTP/1.1 200 OK\r\n"
								"Content-Type: application/json\r\n"
								"Content-Length: %d\r\n"
								"Connection: close\r\n\r\n"
								"%s",
								len, json);

							write(idx, response, strlen(response));
						}
						else if(strstr(request, "GET /img/"))
						{
							char *path_start = strstr(request, "GET /img/");
							if (path_start)
							{
								char path[64] = {0},*content_type = "image/png";

								sscanf(path_start, "GET %63s", path);

								if (strstr(path, ".jpg") || strstr(path, ".jpeg"))
								{
									content_type = "image/jpeg";
								}
								else if (strstr(path, ".png"))
								{
									content_type = "image/png";
								}

								file_send_err = send_file(idx,path, content_type);
							}
							else
								file_send_err = -2;
						}
						else if (strstr(request, "POST /led"))
						{
							HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin,(strstr(request, "\"state\":1") ? GPIO_PIN_SET : GPIO_PIN_RESET));

						    // Send success response
						    const char *response = "HTTP/1.1 200 OK\r\n"
						                          "Content-Type: application/json\r\n"
						                          "Content-Length: 0\r\n"
						                          "Connection: close\r\n\r\n";
						    write(idx, response, strlen(response));

							osThreadFlagsSet(ws_thread_id, 0x01);

						}
						else
						{
							file_send_err = send_file(idx,"/404.html", "text/html");
						}

						if(file_send_err != 0)
						{
							if(file_send_err == -1)
								printf("%s: Fatal: could not open the file\r\n",__func__);
							else if(file_send_err == -2)
								printf("%s: Fatal: wrong path\r\n",__func__);
							const char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";
							write(idx, resp, strlen(resp));
						}
                        // After handling the request, close the connection (HTTP 1.0 style)
                        close(idx);
                        FD_CLR(idx, &master_fds);

                        // Remove from client sockets array
                        for (int j = 0; j < MAX_HTTP_CLIENTS; j++)
                        {
                            if (client_sockets[j] == idx)
                            {
                                client_sockets[j] = -1;
                                break;
                            }
                        }
                    }
                    else if (bytes_read == 0)
                    {
                        // Connection closed by client
                        close(idx);
                        FD_CLR(idx, &master_fds);

                        for (int j = 0; j < MAX_HTTP_CLIENTS; j++)
                        {
                            if (client_sockets[j] == idx)
                            {
                                client_sockets[j] = -1;
                                break;
                            }
                        }
                    }
                }
            }
        }

        //Prevent CPU hogging
        osDelay(1);
    }
}

#ifdef ENABLE_MODBUS_TCP
static void tcp_server_thread(void* argument)
{
	printf("Start %s\n\r",osThreadGetName(osThreadGetId()));
	printf("Real free heap (tcp_server_thread): %lu\r\n",(uint32_t)xPortGetFreeHeapSize());
	printf("Stack free(tcp_server_thread): %lu\r\n",(uint32_t)uxTaskGetStackHighWaterMark(NULL));
	uint16_t port = MODBUS_TCP_PORT;
	int sock;
	struct sockaddr_in address,remotehost;
	socklen_t sockaddrsize = sizeof(remotehost);

	for (uint16_t idx = 0; idx < MAX_TCP_SOCK_CLIENTS; idx++) {
		clients_sock_arr[idx] = (ts_client_socket){.accept_sock = -1,
												   .data_count = 0,
												   .sockaddrsize_ = sockaddrsize,
												   .in_use = false
												};
		memset(clients_sock_arr[idx].client_data,0,(size_t)COIL_BUF_SIZE);
	}

	nmbs_server_init(&nmbs, &nmbs_server);

	if((sock = socket(AF_INET,SOCK_STREAM,0)) >= 0)
	{
		fcntl(sock, F_SETFL, O_NONBLOCK);

		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if(bind(sock,(struct sockaddr*)&address,sizeof(address)) == 0)
		{
			listen(sock,5);

			fd_set readfds;
			int max_sock = sock;

			for(;;)
			{
				FD_ZERO(&readfds);
				FD_SET(sock,&readfds);

				for(uint8_t client_pos=0;client_pos <MAX_TCP_SOCK_CLIENTS;client_pos++ )
				{
					if(clients_sock_arr[client_pos].accept_sock > 0)
					{
						FD_SET(clients_sock_arr[client_pos].accept_sock,&readfds);
						if(clients_sock_arr[client_pos].accept_sock > max_sock)
						{
							max_sock = clients_sock_arr[client_pos].accept_sock;
						}
					}
				}

				struct timeval timeout = (struct timeval){.tv_sec=0,.tv_usec=10000};

				int activity = select(max_sock+1,&readfds,NULL,NULL,&timeout);

				if(activity < 0)
				{
					printf("select error in %s\r\n",osThreadGetName(osThreadGetId()));
					continue;
				}

				if(FD_ISSET(sock,&readfds))
				{
					int new_sock = accept(sock,(struct sockaddr*)&remotehost,(socklen_t*)&sockaddrsize);
					if (new_sock > 0)
					{
		             	printf("Real free heap (tcp_server_thread): %lu\r\n",(uint32_t)xPortGetFreeHeapSize());
						printf("Stack free(tcp_server_thread): %lu\r\n",(uint32_t)uxTaskGetStackHighWaterMark(NULL));
						int slot = -1;
						for (uint16_t idx = 0; idx < MAX_TCP_SOCK_CLIENTS; idx++)
						{
							if (clients_sock_arr[idx].accept_sock <= 0)
							{
								slot = idx;
								break;
							}
						}

						if (slot >= 0)
						{
							fcntl(new_sock, F_SETFL, O_NONBLOCK);
							clients_sock_arr[slot].accept_sock = new_sock;
							remotehost_struct_deep_copy(&clients_sock_arr[slot].remotehost,&remotehost);
						}
						else
						{
							close(new_sock);
						}
					}
				}

				for(uint8_t client_pos=0;client_pos <MAX_TCP_SOCK_CLIENTS;client_pos++ )
				{
					if(clients_sock_arr[client_pos].accept_sock > 0 && FD_ISSET(clients_sock_arr[client_pos].accept_sock,&readfds))
					{
						int ret = recv(clients_sock_arr[client_pos].accept_sock,clients_sock_arr[client_pos].client_data,20,0);
						if(ret <=0)
						{
							close(clients_sock_arr[client_pos].accept_sock);
							clients_sock_arr[client_pos].accept_sock = -1;
						}
						else
						{
							clients_sock_arr[client_pos].in_use = true;
							clients_sock_arr[client_pos].data_count = ret;
							nmbs_server_poll(&nmbs);
						}
					}
				}
			}
		 }
		else
		{
			printf("bind failed in %s\r\n",osThreadGetName(osThreadGetId()));
			close(sock);
			osThreadTerminate(osThreadGetId());
		}
	}
	else
	{
		printf("socket creation failed in %s\r\n",osThreadGetName(osThreadGetId()));
		osThreadTerminate(osThreadGetId());
	}
}
#endif

const nmbs_t get_nmbs(void)
{
	return nmbs;
}

ts_client_socket* get_clients_arr(void)
{
	return clients_sock_arr;
}

void remotehost_struct_deep_copy(struct sockaddr_in* dest,const struct sockaddr_in* src)
{
	dest->sin_addr = src->sin_addr;
	dest->sin_family = src->sin_family;
	dest->sin_len = src->sin_len;
	dest->sin_port = src->sin_port;
	memcpy(&(dest->sin_zero),src->sin_zero,SIN_ZERO_LEN);
}

void generate_ws_accept(const char *key, char *output)
{
    char combined[64];
    const char *ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    snprintf(combined, sizeof(combined), "%s%s", key, ws_guid);

    unsigned char sha1[20];
    mbedtls_sha1((unsigned char *)combined, strlen(combined), sha1);

    mbedtls_base64_encode((unsigned char *)output, 64, NULL, sha1, 20);
}


int parse_ws_frame(const char *data, int len, ws_frame_t *frame)
{
    if (len < 2)
    	return -1;

    const uint8_t *bytes = (const uint8_t *)data;

    frame->fin = (bytes[0] & 0x80) != 0;
    frame->opcode = bytes[0] & 0x0F;
    frame->mask = (bytes[1] & 0x80) != 0;
    frame->payload_len = bytes[1] & 0x7F;

    int offset = 2;

    if (frame->payload_len == 126)
    {
        if (len < offset + 2)
        	return -1;
        frame->payload_len = (bytes[2] << 8) | bytes[3];
        offset += 2;
    }
    else if (frame->payload_len == 127)
    {
        if (len < offset + 8)
        	return -1;
        // For 64-bit length (we'll just use 32-bit for simplicity)
        frame->payload_len = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[4] << 8) | bytes[5];
        offset += 8;
    }

    if (frame->mask)
    {
        if (len < offset + 4)
        	return -1;
        memcpy(frame->masking_key, bytes + offset, 4);
        offset += 4;
    }

    if (len < offset + frame->payload_len)
    	return -1;

    frame->payload_data = (char *)(bytes + offset);
    return offset + frame->payload_len;
}


int create_ws_frame(char *buffer, int buflen, const char *payload, int payload_len, uint8_t opcode)
{
    if (buflen < payload_len + 10)
    	return -1;

    int offset = 0;
    buffer[offset++] = 0x80 | (opcode & 0x0F); // FIN + opcode

    if (payload_len <= 125)
    {
        buffer[offset++] = payload_len;
    }
    else if (payload_len <= 65535)
    {
        buffer[offset++] = 126;
        buffer[offset++] = (payload_len >> 8) & 0xFF;
        buffer[offset++] = payload_len & 0xFF;
    }
    else
    {
        buffer[offset++] = 127;
        // For simplicity, we'll assume payload_len fits in 32 bits
        buffer[offset++] = 0;
        buffer[offset++] = 0;
        buffer[offset++] = 0;
        buffer[offset++] = 0;
        buffer[offset++] = (payload_len >> 24) & 0xFF;
        buffer[offset++] = (payload_len >> 16) & 0xFF;
        buffer[offset++] = (payload_len >> 8) & 0xFF;
        buffer[offset++] = payload_len & 0xFF;
    }

    memcpy(buffer + offset, payload, payload_len);
    return offset + payload_len;
}

void broadcast_to_ws_clients(int* clients, const char *message, int len)
{
    char ws_frame[128];
    int frame_len = create_ws_frame(ws_frame, sizeof(ws_frame), message, len, 0x01);

    for (int idx = 0; idx < MAX_WS_CLIENTS; idx++)
    {
        if (clients[idx] >= 0)
        {
            if (write(clients[idx], ws_frame, frame_len) < 0)
            {
                printf("Failed to send to client %d, closing\n", idx);
                close(clients[idx]);
                clients[idx] = -1;
            }
        }
    }
}

void send_response(int sock, const char *content_type, const char *data, int len)
{
    char headers[128];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        content_type,
        len
    );

    write(sock, headers, headers_len);
    write(sock, data, len);
}
/* USER CODE END Application */
