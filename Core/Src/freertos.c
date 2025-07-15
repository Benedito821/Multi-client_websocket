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
osThreadId_t tcp_server_TaskHandle;

const osThreadAttr_t tcp_server_Task_attributes = {
  .name = "tcp_server_thread",
  .stack_size = 5*1024,
  .priority = (osPriority_t) osPriorityNormal,
};

ts_client_socket clients_sock_arr[MAX_TCP_SOCK_CLIENTS] ;

static nmbs_t nmbs;

static nmbs_server_t nmbs_server = {
        .id = 0x01,
        .coils =
                {
					0,
                },
        .regs =
                {
					0,
                },
		.input_regs =
					{
						0,
					},
};
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
					perror("select error");
					continue;
				}

				if(FD_ISSET(sock,&readfds))
				{
					int new_sock = accept(sock,(struct sockaddr*)&remotehost,(socklen_t*)&sockaddrsize);
					if (new_sock > 0)
					{
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
			perror("bind failed");
			close(sock);
			return;
		}
	}
	else
	{
		perror("socket creation failed");
		return;
	}
}

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
/* USER CODE END Application */

