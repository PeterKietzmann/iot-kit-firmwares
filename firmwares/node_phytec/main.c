/*
 * Copyright (C) 2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v3. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <inttypes.h>
#include <coap.h>

#include "msg.h"
#include "thread.h"
#include "xtimer.h"
#include "hdc1000.h"
#include "hdc1000_params.h"
#include "tcs37727.h"
#include "tcs37727_params.h"
#include "mpl3115a2.h"
#include "board.h"
#include "net/af.h"
#include "net/gnrc/ipv6.h"
#include "net/conn/udp.h"

#ifndef BROKER_ADDR
#define BROKER_ADDR           "2001:660:3207:102::4"
#endif

#define BROKER_PORT           (5683)

#define INTERVAL              (30000000U)    /* set interval to 30 seconds */
#define SENSORS_INTERVAL      (1000000U)     /* set sensor updates * 3 */

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
static uint16_t pkt_id = 0;
static uint8_t response[512] = { 0 };

static hdc1000_t th_dev;
static tcs37727_t light_dev;

void microcoap_server_loop(void);

/* import "ifconfig" shell command, used for printing addresses */
extern int _netif_config(int argc, char **argv);

void _read_temperature(int16_t * temperature)
{
    *temperature = 666; // todo
}

void _read_illuminance(uint32_t * illuminance)
{
    *illuminance = 777; // todo
}

void _read_humidity(uint16_t * humidity)
{
    *humidity = 888; // todo
}

void _send_coap_post(uint8_t* uri_path, uint8_t *data)
{
    /* format destination address from string */
    ipv6_addr_t dst_addr;
    if (ipv6_addr_from_str(&dst_addr, broker_addr) == NULL) {
        printf("Error: address not valid '%s'\n", broker_addr);
        return;
    }

    pkt_id ++;
    req_hdr.id[0] = (uint8_t)(pkt_id >> 8);
    req_hdr.id[1] = (uint8_t)(pkt_id << 8 / 255);

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

    int16_t temp, hum;


    for(;;) {
        //memset(response, 0, 512);

        hdc1000_read(&th_dev, &temp, &hum);

        tcs37727_data_t light_data;
        tcs37727_read(&light_dev, &light_data);

        size_t p = 0;
        p += sprintf((char*)&response[p], "illuminance:");
        p += sprintf((char*)&response[p], "%ilx",
                     (int)light_data.lux);
        response[p] = '\0';

        printf("%s\n", response);
        _send_coap_post((uint8_t*)"server", response);

        //memset(response, 0, 512);

        xtimer_usleep(SENSORS_INTERVAL);

        /* only send temperature update when changed */
        p = 0;
        p += sprintf((char*)&response[p], "temperature:");
        p += sprintf((char*)&response[p], "%d.%d°C",
                     temp / 100, (temp % 100) /10);
        response[p] = '\0';

        printf("%s\n", response);
        _send_coap_post((uint8_t*)"server", response);

        //memset(response, 0, 512);
        xtimer_usleep(SENSORS_INTERVAL);

        p = 0;
        p += sprintf((char*)&response[p], "humidity:");
        p += sprintf((char*)&response[p], "%u.%02u%%",
                     (unsigned int)(hum / 100),
                     (unsigned int)(hum % 100));
        response[p] = '\0';
        printf("%s\n", response);
        _send_coap_post((uint8_t*)"server", response);

        /* wait */
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

    /* initialize sensors */
    if (hdc1000_init(&th_dev, &hdc1000_params[0]) == HDC1000_OK) {
        puts("[OK]\n");
    }
    else {
        puts("[Failed]");
        return 1;
    }
    if (tcs37727_init(&light_dev, &tcs37727_params[0]) == TCS37727_OK) {
        puts("[OK]\n");
    }
    else {
        puts("[Failed]");
        return -1;
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
                                    NULL, "Sensors thread");
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
