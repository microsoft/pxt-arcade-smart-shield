#ifndef __JDSPI_H
#define __JDSPI_H

#include "jdprotocol.h"
#include "jdarcade.h"

void jdspi_process();
void jdspi_early_init();
void jdspi_init();
void jdspi_connected();
void jdspi_disconnected();
void jdspi_send(uint32_t service, uint32_t cmd, uint32_t size);

extern bool jd_display_frame_start;
void jd_display_incoming(jd_packet_t *pkt);
void jd_display_process();
void jd_display_outgoing(int serviceNo, void *dst);

void jd_arcade_controls_incoming(jd_packet_t *pkt);
void jd_arcade_controls_process();
void jd_arcade_controls_outgoing(int serviceNo, void *dst);

#endif