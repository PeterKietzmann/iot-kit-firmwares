/*
 * Copyright (C) 2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v3. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdlib.h>
#include <string.h>
#include <coap.h>

#include "board.h"
#include "window.h"

#define APPLICATION_NAME "WINDOW Node"

extern servo_t servo;
static char window_post = 0;

extern void _send_coap_post(uint8_t* uri_path, uint8_t *data);

#define MAX_RESPONSE_LEN 500
static uint8_t response[MAX_RESPONSE_LEN] = { 0 };

static int handle_get_well_known_core(coap_rw_buffer_t *scratch,
                                      const coap_packet_t *inpkt,
                                      coap_packet_t *outpkt,
                                      uint8_t id_hi, uint8_t id_lo);

static int handle_get_name(coap_rw_buffer_t *scratch,
                           const coap_packet_t *inpkt,
                           coap_packet_t *outpkt,
                           uint8_t id_hi, uint8_t id_lo);

static int handle_get_os(coap_rw_buffer_t *scratch,
                         const coap_packet_t *inpkt,
                         coap_packet_t *outpkt,
                         uint8_t id_hi, uint8_t id_lo);

static int handle_get_board(coap_rw_buffer_t *scratch,
                            const coap_packet_t *inpkt,
                            coap_packet_t *outpkt,
                            uint8_t id_hi, uint8_t id_lo);

static int handle_get_mcu(coap_rw_buffer_t *scratch,
                          const coap_packet_t *inpkt,
                          coap_packet_t *outpkt,
                          uint8_t id_hi, uint8_t id_lo);

static int handle_get_window(coap_rw_buffer_t *scratch,
                          const coap_packet_t *inpkt,
                          coap_packet_t *outpkt,
                          uint8_t id_hi, uint8_t id_lo);

static int handle_put_window(coap_rw_buffer_t *scratch,
                          const coap_packet_t *inpkt,
                          coap_packet_t *outpkt,
                          uint8_t id_hi, uint8_t id_lo);

static const coap_endpoint_path_t path_well_known_core =
{ 2, { ".well-known", "core" } };

static const coap_endpoint_path_t path_name =
        { 1, { "name" } };

static const coap_endpoint_path_t path_os =
        { 1, { "os" } };

static const coap_endpoint_path_t path_board =
{ 1, { "board" } };

static const coap_endpoint_path_t path_mcu =
{ 1, { "mcu" } };

static const coap_endpoint_path_t path_window =
{ 1, { "led" } };

const coap_endpoint_t endpoints[] =
{
    { COAP_METHOD_GET,	handle_get_well_known_core,
      &path_well_known_core, "ct=40" },
    { COAP_METHOD_GET,	handle_get_name,
      &path_name,	   "ct=0"  },
    { COAP_METHOD_GET,	handle_get_os,
      &path_os,  	   "ct=0"  },
    { COAP_METHOD_GET,	handle_get_board,
      &path_board,	   "ct=0"  },
    { COAP_METHOD_GET,	handle_get_mcu,
      &path_mcu,	   "ct=0"  },
    { COAP_METHOD_GET,	handle_get_window,
      &path_window,	   "ct=0"  },
    { COAP_METHOD_PUT,	handle_put_window,
      &path_window,	   "ct=0"  },
    /* marks the end of the endpoints array: */
    { (coap_method_t)0, NULL, NULL, NULL }
};

static int handle_get_well_known_core(coap_rw_buffer_t *scratch,
                                      const coap_packet_t *inpkt, coap_packet_t *outpkt,
                                      uint8_t id_hi, uint8_t id_lo)
{
    char *rsp = (char *)response;
    /* resetting the content of response message */
    memset(response, 0, sizeof(response));
    size_t len = sizeof(response);
    const coap_endpoint_t *ep = endpoints;
    int i;
    
    len--; // Null-terminated string
    
    while (NULL != ep->handler) {
        if (NULL == ep->core_attr) {
            ep++;
            continue;
        }
        
        if (0 < strlen(rsp)) {
            strncat(rsp, ",", len);
            len--;
        }
        
        strncat(rsp, "<", len);
        len--;
        
        for (i = 0; i < ep->path->count; i++) {
            strncat(rsp, "/", len);
            len--;
            
            strncat(rsp, ep->path->elems[i], len);
            len -= strlen(ep->path->elems[i]);
        }
        
        strncat(rsp, ">;", len);
        len -= 2;
        
        strncat(rsp, ep->core_attr, len);
        len -= strlen(ep->core_attr);
        
        ep++;
    }
    
    return coap_make_response(scratch, outpkt, (const uint8_t *)rsp,
                              strlen(rsp), id_hi, id_lo, &inpkt->tok,
                              COAP_RSPCODE_CONTENT,
                              COAP_CONTENTTYPE_APPLICATION_LINKFORMAT);
}

static int handle_get_name(coap_rw_buffer_t *scratch,
                           const coap_packet_t *inpkt,
                           coap_packet_t *outpkt,
                           uint8_t id_hi, uint8_t id_lo)
{
    const char *app_name = APPLICATION_NAME;
    size_t len = strlen(APPLICATION_NAME);

    memcpy(response, app_name, len);

    return coap_make_response(scratch, outpkt, (const uint8_t *)response, len,
                              id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT,
                              COAP_CONTENTTYPE_TEXT_PLAIN);
}

static int handle_get_os(coap_rw_buffer_t *scratch,
                         const coap_packet_t *inpkt,
                         coap_packet_t *outpkt,
                         uint8_t id_hi, uint8_t id_lo)
{
    const char *os = "riot";
    size_t len = strlen("riot");

    memcpy(response, os, len);

    return coap_make_response(scratch, outpkt, (const uint8_t *)response, len,
                              id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT,
                              COAP_CONTENTTYPE_TEXT_PLAIN);
}

static int handle_get_board(coap_rw_buffer_t *scratch,
                            const coap_packet_t *inpkt,
                            coap_packet_t *outpkt,
                            uint8_t id_hi, uint8_t id_lo)
{
    const char *riot_name = RIOT_BOARD;
    int len = strlen(RIOT_BOARD);
    
    memcpy(response, riot_name, len);
    
    return coap_make_response(scratch, outpkt, (const uint8_t *)response, len,
                              id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT,
                              COAP_CONTENTTYPE_TEXT_PLAIN);
}

static int handle_get_mcu(coap_rw_buffer_t *scratch,
                          const coap_packet_t *inpkt,
                          coap_packet_t *outpkt,
                          uint8_t id_hi, uint8_t id_lo)
{
    const char *riot_mcu = RIOT_MCU;
    int len = strlen(RIOT_MCU);
    
    memcpy(response, riot_mcu, len);
    
    return coap_make_response(scratch, outpkt, (const uint8_t *)response, len,
                              id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT,
                              COAP_CONTENTTYPE_TEXT_PLAIN);
}

static int handle_get_window(coap_rw_buffer_t *scratch,
                          const coap_packet_t *inpkt,
                          coap_packet_t *outpkt,
                          uint8_t id_hi, uint8_t id_lo)
{
    char window_status[1];
    sprintf(window_status, "%d", window_post);
    memcpy(response, window_status, 1);

    printf("handle_get_window");
    
    return coap_make_response(scratch, outpkt, (const uint8_t *)response, 1,
                              id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT,
                              COAP_CONTENTTYPE_TEXT_PLAIN);
}

static int handle_put_window(coap_rw_buffer_t *scratch,
                          const coap_packet_t *inpkt,
                          coap_packet_t *outpkt,
                          uint8_t id_hi, uint8_t id_lo)
{   
    coap_responsecode_t resp = COAP_RSPCODE_CHANGED;

    /* Check input data is valid */
    uint8_t val = strtol((char*)inpkt->payload.p, NULL, 10);
    if ( (inpkt->payload.len == 1) && (val == 1) ) {
        openWindow(&servo);
        window_post = 1;
        printf("Window open: %i\n", (int)val);

    }
    else if ( (inpkt->payload.len == 1) && (val == 0) ){
        closeWindow(&servo);
        window_post = 0;
        printf("Window close: %i\n", (int)val);

    }
    else {
        resp = COAP_RSPCODE_BAD_REQUEST;
    }
    
    /* Reply to server */
    int result = coap_make_response(scratch, outpkt, NULL, 0,
                                    id_hi, id_lo,
                                    &inpkt->tok, resp,
                                    COAP_CONTENTTYPE_TEXT_PLAIN);
    
    /* Send post notification to server */
    char window_status[5];
    sprintf(window_status, "led:%d", window_post);
    _send_coap_post((uint8_t*)"server", (uint8_t*)window_status);
    
    return result;
}
