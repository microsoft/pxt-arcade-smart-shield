#ifndef JD_USER_CONFIG_H
#define JD_USER_CONFIG_H

//#include "board.h"
#include "dmesg.h"

#define JD_LOG DMESG

#ifdef PSCREEN
#define PIN_LOG1 PA_12
#else
#define PIN_LOG1 PA_9
#endif

#ifdef BB_V1
#define PIN_LED PC_6
#define PIN_LOG0 PC_14
#elif defined(PROTO_V2)
#define PIN_LED PB_14
#define PIN_LOG0 PB_12
#undef PIN_LOG1
#define PIN_LOG1 PB_13
#else
#define PIN_LED PC_6
#define PIN_LOG0 PA_10
#endif


#endif
