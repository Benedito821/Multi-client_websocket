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
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SOCK_DATA_BUFF_LEN 150
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t tcp_server_TaskHandle;

const osThreadAttr_t tcp_server_Task_attributes = {
  .name = "tcp_server_thread",
  .stack_size = 1024,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t client_socket_TaskHandle;

typedef struct client_socket
{
	struct sockaddr_in remotehost;
	socklen_t sockaddrsize;
	int accept_sock;
}ts_client_socket;

ts_client_socket client_socket01;

volatile size_t stack_control_var = 0;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t client_socket_Task_attributes = {
  .name = "client_socket_thread",
  .stack_size = 2*1024,
  .priority = (osPriority_t) osPriorityNormal,
};

char out_buffer[SOCK_DATA_BUFF_LEN] = {0};
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void tcp_server_thread(void* argument);
static void client_socket_thread(void* argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
	__NOP();
	/* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
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
  /* add threads, ... */
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
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */
  tcp_server_TaskHandle = osThreadNew(tcp_server_thread, NULL, &tcp_server_Task_attributes);
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
    HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void tcp_server_thread(void* argument)
{
	uint16_t port = 502;
	int sock,accept_sock;
//	sock = -1;
//	accept_sock=-1;
	struct sockaddr_in address,remotehost;
	socklen_t sockaddrsize;
	if((sock = socket(AF_INET,SOCK_STREAM,0)) >= 0)
	{
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		if(bind(sock,(struct sockaddr*)&address,sizeof(address)) == 0)
		{
			listen(sock,5);
			for(;;)
			{
				accept_sock = accept(sock,(struct sockaddr*)&remotehost,(socklen_t*)&sockaddrsize);
				if(accept_sock >= 0)
				{
					client_socket01.accept_sock = accept_sock;
					//client_socket01.remotehost = (struct sockaddr_in)remotehost;
					client_socket01.remotehost.sin_addr = remotehost.sin_addr;
					client_socket01.remotehost.sin_family = remotehost.sin_family;
					client_socket01.remotehost.sin_len = remotehost.sin_len;
					client_socket01.remotehost.sin_port = remotehost.sin_port;
					memcpy(&(client_socket01.remotehost.sin_zero),&(remotehost.sin_zero),8);

					client_socket01.sockaddrsize = sockaddrsize;
					client_socket_TaskHandle = osThreadNew(client_socket_thread, (void*)&client_socket01, &client_socket_Task_attributes);
				}
			}
		}
		else
		{
			close(sock);
			return;
		}
	}
}

static void client_socket_thread(void* argument)
{
	int ret,accept_sock;
	struct sockaddr_in remotehost;
	socklen_t sockaddrsize;
	ts_client_socket* arg_client_socket;
	arg_client_socket = (ts_client_socket*)argument;
	remotehost = arg_client_socket->remotehost;
	sockaddrsize = arg_client_socket->sockaddrsize;
	accept_sock = arg_client_socket->accept_sock;
	for(;;)
	{
		ret = recvfrom(accept_sock,out_buffer,SOCK_DATA_BUFF_LEN,0,(struct sockaddr*)&remotehost,&sockaddrsize);
		if(ret > 0)
		{
			if(strcmp(out_buffer,"-c") == 0)
			{
				strcpy(out_buffer,"Bye Bye from server!");
				sendto(accept_sock,out_buffer,strlen(out_buffer),0,(struct sockaddr*)&remotehost,sockaddrsize);
				close(accept_sock);
				memset(out_buffer,0,(size_t)SOCK_DATA_BUFF_LEN);
				osThreadExit();
			}
			stack_control_var = xPortGetMinimumEverFreeHeapSize();
			memset(out_buffer,0,(size_t)SOCK_DATA_BUFF_LEN);
		}


	}
}

/* USER CODE END Application */

