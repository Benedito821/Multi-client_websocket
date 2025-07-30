#ifndef INC_APP_H_
#define INC_APP_H_

#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nanomodbus.h"
#include "modbus_port.h"

#define MAX_TCP_SOCK_CLIENTS (MEMP_NUM_NETCONN-1) //Max possible sockets is MEMP_NUM_NETCONN-1
#define HTTP_PORT 80

typedef struct client_socket
{
	int accept_sock,
		 data_count;
	char client_data[COIL_BUF_SIZE];
	struct sockaddr_in remotehost;
	socklen_t sockaddrsize_;
	bool in_use;
}ts_client_socket;

const nmbs_t get_nmbs(void);
ts_client_socket* get_clients_arr(void);
void remotehost_struct_deep_copy(struct sockaddr_in* dest,const struct sockaddr_in* src);
#endif /* INC_APP_H_ */
