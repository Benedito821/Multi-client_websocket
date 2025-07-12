#ifndef INC_APP_H_
#define INC_APP_H_

#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nanomodbus.h"
#include "modbus_port.h"

typedef struct client_socket
{
	struct sockaddr_in remotehost;
	socklen_t sockaddrsize;
	int accept_sock;
}ts_client_socket;

ts_client_socket get_client_socket01(void);

#endif /* INC_APP_H_ */
