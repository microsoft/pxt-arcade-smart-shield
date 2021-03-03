// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef __JDSPI_H
#define __JDSPI_H

#include "jd_protocol.h"

void jdspi_process();
void jdspi_early_init();
void jdspi_init();
void jdspi_connected();
void jdspi_disconnected();
void *jdspi_send(unsigned service_num, unsigned service_cmd, const void *data, unsigned size);
void jdspi_send_ad_data(unsigned service_num, bool *flag, const void *data, unsigned size);

extern bool jd_display_frame_start;
void jd_display_incoming(jd_packet_t *pkt);
void jd_display_process();
void jd_display_outgoing(int serviceNo);

void jd_arcade_gamepad_incoming(jd_packet_t *pkt);
void jd_arcade_gamepad_process();
void jd_arcade_gamepad_outgoing(int serviceNo);

void jd_arcade_sound_incoming(jd_packet_t *pkt);
void jd_arcade_sound_process();
void jd_arcade_sound_outgoing(int serviceNo);

#endif