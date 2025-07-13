#ifndef MODBUS_PORT_H
#define MODBUS_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define NMBS_TCP
// Max size of coil and register area
#define COIL_BUF_SIZE 256
#define REG_BUF_SIZE 256

#include "nanomodbus.h"
#include "stm32f7xx_hal.h"

#include "lwip/sockets.h"

typedef struct tNmbsServer {
    uint8_t id;
    uint8_t coils[COIL_BUF_SIZE];
    uint16_t regs[REG_BUF_SIZE];
} nmbs_server_t;

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* server);
nmbs_error nmbs_client_init(nmbs_t* nmbs);

#ifdef __cplusplus
}
#endif

#endif
