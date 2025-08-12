# PROJECT OVERVIEW
The project implements a server that responds to modbus TCP requests from different TCP clients simultaneouslly, while also serving web pages to interact with it(using AJAX). The final purpose is to display sensors measurements and states dinamically and visually on the web page, and additionally sending them over to TCP clients for real time monitoring.
 

# MODBUS TCP with STM32F746

Credits to [@debevv](https://github.com/debevv/nanoMODBUS) for the modbus library. Since the socket implementation and the project configuration itself were not provided, I added the network layer, binded it to the modbus library and adapted it according to my need. The present library implements a TCP server that can handle multiple clients exchanging data using modbus protocol(TCP port 502). To mimic the clients I used the MBTCP Ver 1.1.4 utility available at https://ipc2u.com/upload/medialibrary/e43/e43a9e8ee8a1db808fbd9106825a9222.zip .

The library supports up to as much TCP clients as defined by:

**#define MEMP_NUM_TCP_PCB**
**#define MEMP_NUM_NETCONN**

An important note here is that during the tests I was able to connect only (MEMP_NUM_TCP_PCB -1) , (MEMP_NUM_NETCONN - 1) clients. Both constants need to be updated at the same time,otherwise there could be issues during accept() or connect(). The default number of clients is 5.

# Key configurations

In my case MPU needed to be configured properly in order for ethernet to work(see ETH and CORTEX_M7 sections in the .ioc file). Linker script should be updated properly to define the memory regions for the ethernet descriptors. There are resources on the web showing a possible configuration without the MPU.

Stack sizes for the tasks *defaultTask,tcp_server_thread,EthLink,EthIf,tcpip_thread* and maybe other critical system tasks should be defined properly for a good operation of the stack, otherwise there could be silent stack overflows preventing the ping to work. A hook function has been implemented in *freertos.c* in order to catch possible stack overflows. Some other crucial parameters are the MEM_SIZE (LwIP heap size) and configTOTAL_HEAP_SIZE (FreeRTOS heap size), with the later playing a crucial role in the amount of webpages we may have open simultaneously: when control.html is open, spacerockets.html may not show all the images properly(at the time I'm writing this, increasing configTOTAL_HEAP_SIZE deliberately leads to system instability. Increasing tasks stack sizes in *app.h* file leads to system execution instabilities, such as malloc failing, unable to fetch resources from the server,etc.). Thus, for a stable use of control.html with multiple clients or/and spacerockets.html, ENABLE_TCP_MODBUS should be undefined in *app.h*.

The main implementation of the library is located in *freertos.c* file, exported functions and types in *app.h* , modbus-related functions in *modbus_port.c/.h and nanomodbus.c/.h*. 


# MODBUS TCP CLIENTS SIMULATION

![](https://github.com/Benedito821/Server_cabinet_monitor/blob/http_server_with_ajax/modbus_client_sim.gif)

# CONTROL.html
Again, MEMP_NUM_TCP_PCB and MEMP_NUM_NETCONN play a huge role when it comes to how many media resources we want to fetch from the server, once we are using a new connection per request(not using keep-alive). Another tweak would be increasing the queue of pending connections(backlog), i.e. listen(my_socket, backlog);. On this web page we toggle the on-board LED and visually show the state of the on-board button(pressed/unpressed). The request for the button state is sent on a 100ms rate, what is too often. A webscoket would be better here.
![](https://github.com/Benedito821/Server_cabinet_monitor/blob/http_server_with_ajax/control_webpage.gif)

# SPACEROCKETS.html
Here we test the possibility of loading multiple media resources without losing any of them.
![](https://github.com/Benedito821/Server_cabinet_monitor/blob/http_server_with_ajax/spacerockets_webpage.png)