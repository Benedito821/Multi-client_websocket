#ifndef INC_APP_H_
#define INC_APP_H_

#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nanomodbus.h"
#include "modbus_port.h"

#define MAX_TCP_SOCK_CLIENTS 4U
typedef struct client_socket
{
	int accept_sock,
		 data_count;
	char client_data[COIL_BUF_SIZE];
	struct sockaddr_in remotehost;
	socklen_t sockaddrsize_;
	bool in_use;
}ts_client_socket;

ts_client_socket get_client_socket01(void);

#endif /* INC_APP_H_ */
