#pragma once
#include <stdio.h>
#include <inttypes.h>

typedef struct
{
    int min;
    int max;
} esp_ot_min_max_t;

typedef struct
{
    unsigned short int kw;
    int min_modulation;
} esp_ot_cap_mod_t;

typedef struct
{
    bool is_service_request;
    bool can_reset;
    bool is_low_water_press;
    bool is_gas_flame_fault;
    bool is_air_press_fault;
    bool is_water_over_temp;
    char fault_code;
    uint16_t diag_code;

} esp_ot_asf_flags_t;

// bit: description [ clear/0, set/1]
//  0: DHW present [ dhw not present, dhw is present ]
//  1: Control type [ modulating, on/off ]
//  2: Cooling config [ cooling not supported, cooling supported]
//  3: DHW config [instantaneous or not-specified, storage tank]
//  4: Master low-off&pump control function [allowed,not allowed]
//  5: CH2 present [CH2 not present, CH2 present]
typedef struct
{
    bool dhw_present;
    unsigned char control_type;
    bool cooling_supported;
    bool dhw_config; // 0 - double-circuit 1 - water boiler
    bool pump_control_allowed;
    bool ch2_present;

} esp_ot_slave_config_t;