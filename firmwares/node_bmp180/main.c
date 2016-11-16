/*
 * Copyright (C) 2016 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       CoAP example server application (using microcoap)
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @}
 */

#include <stdio.h>
#include <coap.h>
#include "msg.h"
#include "thread.h"
#include "xtimer.h"
#include "bmp180.h"
#include "board.h"
#include "net/af.h"
#include "net/gnrc/ipv6.h"
#include "net/conn/udp.h"

#ifndef BROKER_ADDR
#define BROKER_ADDR           "2001:660:3207:102::4"
#endif

#define BROKER_PORT           (5683)

#define INTERVAL              (30000000U)    /* set interval to 30 seconds */
#define SENSORS_INTERVAL      (5000000U)     /* set temperature updates interval to 5 seconds */

#define MAIN_QUEUE_SIZE       (8)
#define BEACONING_QUEUE_SIZE  (8)
#define SENSORS_QUEUE_SIZE    (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static msg_t _beaconing_msg_queue[BEACONING_QUEUE_SIZE];
static char beaconing_stack[THREAD_STACKSIZE_DEFAULT];

static msg_t _sensors_msg_queue[SENSORS_QUEUE_SIZE];
static char sensors_stack[THREAD_STACKSIZE_DEFAULT];

static coap_header_t req_hdr = {
    .ver  = 1,
    .t    = COAP_TYPE_NONCON,
    .tkl  = 0,
    .code = COAP_METHOD_POST,
    .id   = {5, 57}            // is equivalent to 1337 when converted to uint16_t
};

/* broker  */
static const char * broker_addr = BROKER_ADDR;
static uint8_t pkt_id = 0;
static uint8_t response[512] = { 0 };

/* BMP180 sensor */
#define I2C_DEVICE (0)
static bmp180_t bmp180_dev;
static int32_t temperature = 0;
static int32_t pressure = 0;

void microcoap_server_loop(void);

/* import "ifconfig" shell command, used for printing addresses */
extern int _netif_config(int argc, char **argv);

void _read_temperature(int32_t * temperature)
{
    bmp180_read_temperature(&bmp180_dev, temperature);
}

void _read_pressure(int32_t * pressure)
{
    bmp180_read_pressure(&bmp180_dev, pressure);
}

void _send_coap_post(uint8_t* uri_path, uint8_t *data)
{
    /* format destination address from string */
    ipv6_addr_t dst_addr;
    if (ipv6_addr_from_str(&dst_addr, broker_addr) == NULL) {
        printf("Error: address not valid '%s'\n", broker_addr);
        return;
    }

    pkt_id = (pkt_id + 1) % 1024;
    req_hdr.id[1] = pkt_id;

    uint8_t  snd_buf[128];
    size_t   req_pkt_sz;

    coap_buffer_t payload = {
        .p   = data,
        .len = strlen((char*)data)
    };

    coap_packet_t req_pkt;
    req_pkt.hdr  = req_hdr;
    req_pkt.tok  = (coap_buffer_t) { 0 };
    req_pkt.numopts = 1;
    req_pkt.opts[0].num = COAP_OPTION_URI_PATH;
    req_pkt.opts[0].buf.p = uri_path;
    req_pkt.opts[0].buf.len = strlen((char*)uri_path);
    req_pkt.payload = payload;

    req_pkt_sz = sizeof(req_pkt);

    if (coap_build(snd_buf, &req_pkt_sz, &req_pkt) != 0) {
        printf("CoAP build failed :(\n");
        return;
    }

    conn_udp_sendto(snd_buf, req_pkt_sz, NULL, 0,
                    &dst_addr, sizeof(dst_addr),
                    AF_INET6, 1234, BROKER_PORT);
}

void *sensors_thread(void *args)
{
    msg_init_queue(_sensors_msg_queue, SENSORS_QUEUE_SIZE);
    int32_t tmp_temperature, tmp_pressure;

    for(;;) {
        bmp180_read_temperature(&bmp180_dev, &tmp_temperature);
        /* only send temperature update when changed */
        size_t p = 0;
        if (tmp_temperature != temperature) {
            p += sprintf((char*)&response[p], "temperature:");
            p += sprintf((char*)&response[p],
                         "%.1f°C", (double)temperature/10.0);
            response[p] = '\0';
            _send_coap_post((uint8_t*)"server", response);
            temperature = tmp_temperature;
        }

        bmp180_read_pressure(&bmp180_dev, &tmp_pressure);
        if (tmp_pressure != pressure) {
            p = 0;
            p += sprintf((char*)&response[p], "pressure:");
            p += sprintf((char*)&response[p], "%.2fhPa", (double)pressure/100.0);
            response[p] = '\0';
            _send_coap_post((uint8_t*)"server", response);
            pressure = tmp_pressure;
        }

        /* wait 5 seconds */
        xtimer_usleep(SENSORS_INTERVAL);
    }

    return NULL;
}

void *beaconing_thread(void *args)
{
    msg_init_queue(_beaconing_msg_queue, BEACONING_QUEUE_SIZE);

    for(;;) {
        _send_coap_post((uint8_t*)"alive", (uint8_t*)"Alive");
        /* wait 30 seconds */
        xtimer_usleep(INTERVAL);
    }
    return NULL;
}


int main(void)
{
    puts("RIOT microcoap example application");

    /* microcoap_server uses conn which uses gnrc which needs a msg queue */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    puts("Waiting for address autoconfiguration...");
    xtimer_sleep(3);

    /* print network addresses */
    puts("Configured network interfaces:");
    _netif_config(0, NULL);

    /* Initialize the BMP180 sensor */
    printf("+------------Initializing BMP180 sensor ------------+\n");
    uint8_t result = bmp180_init(&bmp180_dev, I2C_DEVICE, BMP180_ULTRALOWPOWER);
    if (result == -1) {
        puts("[Error] The given i2c is not enabled");
    }
    else if (result == -2) {
        puts("[Error] The sensor did not answer correctly on the given address");
    }
    else {
        printf("Initialization successful\n\n");
    }

    /* create the beaconning thread that will send periodic messages to
       the broker */
    int beacon_pid = thread_create(beaconing_stack, sizeof(beaconing_stack),
                                   THREAD_PRIORITY_MAIN - 1,
                                   THREAD_CREATE_STACKTEST, beaconing_thread,
                                   NULL, "Beaconing thread");
    if (beacon_pid == -EINVAL || beacon_pid == -EOVERFLOW) {
        puts("Error: failed to create beaconing thread, exiting\n");
    }
    else {
        puts("Successfuly created beaconing thread !\n");
    }

    /* create the sensors thread that will send periodic updates to
       the server */
    int sensors_pid = thread_create(sensors_stack, sizeof(sensors_stack),
                                    THREAD_PRIORITY_MAIN - 1,
                                    THREAD_CREATE_STACKTEST, sensors_thread,
                                    NULL, "IMU thread");
    if (sensors_pid == -EINVAL || sensors_pid == -EOVERFLOW) {
        puts("Error: failed to create sensors thread, exiting\n");
    }
    else {
        puts("Successfuly created sensors thread !\n");
    }

    /* start coap server loop */
    microcoap_server_loop();

    /* should be never reached */
    return 0;
}
