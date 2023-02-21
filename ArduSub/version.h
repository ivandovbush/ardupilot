#pragma once

#ifndef FORCE_VERSION_H_INCLUDE
#error version.h should never be included directly. You probably want to include AP_Common/AP_FWVersion.h
#endif

#include "ap_version.h"

#define THISFIRMWARE "ArduSub V4.1.1 BETA4"

// the following line is parsed by the autotest scripts
#define FIRMWARE_VERSION 4,1,1,FIRMWARE_VERSION_TYPE_BETA+4

#define FW_MAJOR 4
#define FW_MINOR 1
#define FW_PATCH 1
#define FW_TYPE FIRMWARE_VERSION_TYPE_BETA

#include <AP_Common/AP_FWVersionDefine.h>