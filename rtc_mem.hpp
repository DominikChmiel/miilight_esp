#pragma once
#include <coredecls.h>

#include "config.hpp"

namespace rtcMem {
#define MEM_VERSION 5
typedef struct {
    // Header
    uint32_t crc32;
    uint8_t  version;

    // Wifi network information
    uint8_t  channel;
    uint8_t  bssid[6];
    uint32_t ip_addr;
    uint32_t gateway_addr;
    uint32_t netmask;
    uint32_t dns_addr;

    bool     is_on;
    uint32_t counter;

} rtcData;

static_assert(sizeof(rtcData) < 512, "Size of RTC Memory exceeded");

extern rtcData gRTC;

auto is_valid() -> bool;

auto read() -> bool;

auto write() -> bool;
}   // namespace rtcMem
