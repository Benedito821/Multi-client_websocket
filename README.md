# MODBUS TCP with STM32F746

Credits to [@debevv](https://github.com/debevv/nanoMODBUS) for the modbus library. Since the socket implementation and the project configuration itself were not provided, I added the network layer, binded it to the modbus library and adapted it according to my need. The present library implements a TCP server that can handle multiple clients exchanging data using modbus protocol(TCP port 502). To mimic the clients I used the MBTCP Ver 1.1.4 utility available at https://ipc2u.com/upload/medialibrary/e43/e43a9e8ee8a1db808fbd9106825a9222.zip .

The library supports up to as much TCP clients as defined by:

**#define MEMP_NUM_TCP_PCB**
**#define MEMP_NUM_NETCONN**

An important note here is that during the tests I was able to connect only (MEMP_NUM_TCP_PCB -1) , (MEMP_NUM_NETCONN - 1) clients. Both constants need to be updated at the same time,otherwise there could be issues during accept() or connect(). The default number of clients is 5.

# Key configurations

In my case MPU needed to be configured properly in order for ethernet to work(see ETH and CORTEX_M7 sections in the .ioc file). Linker script should be updated properly to define the memory regions for the ethernet descriptors. There are resources on the web showing a possible configuration without the MPU.

Stack sizes for the tasks *defaultTask,tcp_server_thread,EthLink,EthIf,tcpip_thread* and maybe other critical system tasks should be defined properly for a good operation of the stack, otherwise there could be silent stack overflows preventing the ping to work. A hook function has been implemented in *freertos.c* in order to catch possible stack overflows.

The main implementation of the library is located in *freertos.c* file, exported functions and types in *app.h* , modbus-related functions in *modbus_port.c/.h and nanomodbus.c/.h*. 