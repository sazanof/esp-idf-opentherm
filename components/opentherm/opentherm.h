/**
* @package     Opentherm library for ESP-IDF framework
* @author:     Mikhail Sazanof
* @copyright   Copyright (C) 2024 - Sazanof.ru
* @licence     MIT
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL portENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL portEXIT_CRITICAL(&mux)

#ifndef bit
#define bit(b) (1UL << (b))
#define bitRead(value, bit) ((value >> bit) & 0x01)
#endif

// ENUMS
typedef enum OpenThermResponseStatus
{
    OT_STATUS_NONE,
    OT_STATUS_SUCCESS,
    OT_STATUS_INVALID,
    OT_STATUS_TIMEOUT
} open_therm_response_status_t;

typedef enum OpenThermMessageType // old name OpenThermRequestType; // for backwared compatibility
{
    /*  Master to Slave */
    OT_READ_DATA = 0b000,
    OT_WRITE_DATA = 0b001,
    OT_INVALID_DATA = 0b010,
    OT_RESERVED = 0b011,
    /* Slave to Master */
    OT_READ_ACK = 0b100,
    OT_WRITE_ACK = 0b101,
    OT_DATA_INVALID = 0b110,
    OT_UNKNOWN_DATA_ID = 0b111
} open_therm_message_type_t;

typedef enum OpenThermMessageID
{
    MSG_ID_STATUS = 0,                                                    // flag8/flag8  Master and Slave Status flags.
    MSG_ID_T_SET = 1,                                                     // f8.8         Control Setpoint i.e.CH water temperature Setpoint(°C)
    MSG_ID_M_CONFIG_M_MEMEBER_ID_CODE = 2,                                // flag8/u8     Master Configuration Flags / Master MemberID Code
    MSG_ID_S_CONFIG_S_MEMEBER_ID_CODE = 3,                                // flag8/u8     Slave Configuration Flags / Slave MemberID Code
    MSG_ID_REMOTE_REQUEST = 4,                                            // u8/u8        Remote Request
    MSG_ID_ASF_FLAGS = 5,                                                 // flag8/u8     Application - specific fault flags and OEM fault code
    MSG_ID_RBP_FLAGS = 6,                                                 // flag8/flag8  Remote boiler parameter transfer - enable & read / write flags
    MSG_ID_COOLING_CONTROL = 7,                                           // f8.8         Cooling control signal(%)
    MSG_ID_T_SET_CH2 = 8,                                                 // f8.8         Control Setpoint for 2e CH circuit(°C)
    MSG_ID_TR_OVERRIDE = 9,                                               // f8.8         Remote override room Setpoint
    MSG_ID_TSP = 10,                                                      // u8/u8        Number of Transparent - Slave - Parameters supported by slave
    MSG_ID_TSP_INDEX_TSP_VALUE = 11,                                      // u8/u8        Index number / Value of referred - to transparent slave parameter.
    MSG_ID_FHB_SIZE = 12,                                                 // u8/u8        Size of Fault - History - Buffer supported by slave
    MSG_ID_FHB_INDEX_FHB_VALUE = 13,                                      // u8/u8        Index number / Value of referred - to fault - history buffer entry.
    MSG_ID_MAX_REL_MOD_LEVEL_SETTINGG = 14,                               // f8.8         Maximum relative modulation level setting(%)
    MSG_ID_MAX_CAPACITY_MIN_MOD_LEVEL = 15,                               // u8/u8        Maximum boiler capacity(kW) / Minimum boiler modulation level(%)
    MSG_ID_TR_SET = 16,                                                   // f8.8         Room Setpoint(°C)
    MSG_ID_REL_MOD_LEVEL = 17,                                            // f8.8         Relative Modulation Level(%)
    MSG_ID_CH_PRESSURE = 18,                                              // f8.8         Water pressure in CH circuit(bar)
    MSG_ID_DHW_FLOW_RATE = 19,                                            // f8.8         Water flow rate in DHW circuit. (litres / minute)
    MSG_ID_DAY_TIME = 20,                                                 // special/u8   Day of Week and Time of Day
    MSG_ID_DATE = 21,                                                     // u8/u8        Calendar date
    MSG_ID_YEAR = 22,                                                     // u16          Calendar year
    MSG_ID_TR_SET_CH2 = 23,                                               // f8.8         Room Setpoint for 2nd CH circuit(°C)
    MSG_ID_TR = 24,                                                       // f8.8         Room temperature(°C)
    MSG_ID_TBOILER = 25,                                                  // f8.8         Boiler flow water temperature(°C)
    MSG_ID_TDHW = 26,                                                     // f8.8         DHW temperature(°C)
    MSG_ID_TOUTSIDE = 27,                                                 // f8.8         Outside temperature(°C)
    MSG_ID_TRET = 28,                                                     // f8.8         Return water temperature(°C)
    MSG_ID_TSTORAGE = 29,                                                 // f8.8         Solar storage temperature(°C)
    MSG_ID_TCOLLECTOR = 30,                                               // f8.8         Solar collector temperature(°C)
    MSG_ID_T_FLOW_CH2 = 31,                                               // f8.8         Flow water temperature CH2 circuit(°C)
    MSG_ID_TDHW2 = 32,                                                    // f8.8         Domestic hot water temperature 2 (°C)
    MSG_ID_TEXHAUST = 33,                                                 // s16          Boiler exhaust temperature(°C)
    MSG_ID_TBOILER_HEAT_EEXCHANGER = 34,                                  // f8.8         Boiler heat exchanger temperature(°C)
    MSG_ID_BOILER_FAN_SSPEED_SETPOINT_AND_ACTIAL = 35,                    // u8/u8        Boiler fan speed Setpoint and actual value
    MSG_ID_FLAME_CURRENT = 36,                                            // f8.8         Electrical current through burner flame[μA]
    MSG_ID_TR_CH2 = 37,                                                   // f8.8         Room temperature for 2nd CH circuit(°C)
    MSG_ID_RELATIVE_HUMIDITY = 38,                                        // f8.8         Actual relative humidity as a percentage
    MSG_ID_TR_OOVERRIDE2 = 39,                                            // f8.8         Remote Override Room Setpoint 2
    MSG_ID_TDHW_SET_UBT_DHW_SET_LB = 48,                                  // s8/s8        DHW Setpoint upper & lower bounds for adjustment(°C)
    MSG_ID_MAX_TSET_UB_MAX_TSET_LB = 49,                                  // s8/s8        Max CH water Setpoint upper & lower bounds for adjustment(°C)
    MSG_ID_TDHW_SET = 56,                                                 // f8.8         DHW Setpoint(°C) (Remote parameter 1)
    MSG_ID_MAX_TSET = 57,                                                 // f8.8         Max CH water Setpoint(°C) (Remote parameters 2)
    MSG_ID_STATUS_VENTILATION_HEAT_RECOVERY = 70,                         // flag8/flag8  Master and Slave Status flags ventilation / heat - recovery
    MSG_ID_VSET = 71,                                                     // -/u8         Relative ventilation position (0-100%). 0% is the minimum set ventilation and 100% is the maximum set ventilation.
    MSG_ID_ASF_FLAGS_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY = 72,       // flag8/u8     Application-specific fault flags and OEM fault code ventilation / heat-recovery
    MSG_ID_OEM_DDIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY = 73,           // u16          An OEM-specific diagnostic/service code for ventilation / heat-recovery system
    MSG_ID_S_CONFIG_S_MEMEBER_ID_CODE_VENTILATION_HEAT_RECOVERY = 74,     // flag8/u8     Slave Configuration Flags / Slave MemberID Code ventilation / heat-recovery
    MSG_ID_OPENTHERM_VVERSION_VENTILATION_HEAT_RECOVERY = 75,             // f8.8         The implemented version of the OpenTherm Protocol Specification in the ventilation / heat-recovery system.
    MSG_ID_VENTILATION_HEAT_RECOVERY_VERSION = 76,                        // u8/u8        Ventilation / heat-recovery product version number and type
    MSG_ID_REL_VENT_LEVEL = 77,                                           // -/u8         Relative ventilation (0-100%)
    MSG_ID_RH_EXHAUST = 78,                                               // -/u8         Relative humidity exhaust air (0-100%)
    MSG_ID_CO2_EXHAUST = 79,                                              // u16          CO2 level exhaust air (0-2000 ppm)
    MSG_ID_TSI = 80,                                                      // f8.8         Supply inlet temperature (°C)
    MSG_ID_TSO = 81,                                                      // f8.8         Supply outlet temperature (°C)
    MSG_ID_TEI = 82,                                                      // f8.8         Exhaust inlet temperature (°C)
    MSG_ID_TEO = 83,                                                      // f8.8         Exhaust outlet temperature (°C)
    MSG_ID_RPM_EXHAUST = 84,                                              // u16          Exhaust fan speed in rpm
    MSG_ID_RPM_SUPPLY = 85,                                               // u16          Supply fan speed in rpm
    MSG_ID_RBP_FLAGS_VENTILATION_HEAT_RECOVERY = 86,                      // flag8/flag8  Remote ventilation / heat-recovery parameter transfer-enable & read/write flags
    MSG_ID_NOMINAL_VENTILATION_VALUE = 87,                                // u8/-         Nominal relative value for ventilation (0-100 %)
    MSG_ID_TSP_VENTILATION_HEAT_RECOVERY = 88,                            // u8/u8        Number of Transparent-Slave-Parameters supported by TSP’s ventilation / heat-recovery
    MSG_ID_TSPindexTSP_VALUE_VENTILATION_HEAT_RECOVERY = 89,              // u8/u8        Index number / Value of referred-to transparent TSP’s ventilation / heat-recovery parameter.
    MSG_ID_FHB_SIZE_VENTILATION_HEAT_RECOVERY = 90,                       // u8/u8        Size of Fault-History-Buffer supported by ventilation / heat-recovery
    MSG_ID_FHB_INDEX_FHB_VALUE_VENTILATION_HEAT_RECOVERY = 91,            // u8/u8        Index number / Value of referred-to fault-history buffer entry ventilation / heat-recovery
    MSG_ID_BRAND = 93,                                                    // u8/u8        Index number of the character in the text string ASCII character referenced by the above index number
    MSG_ID_BRAND_VERSION = 94,                                            // u8/u8        Index number of the character in the text string ASCII character referenced by the above index number
    MSG_ID_BRAND_SERIAL_NUMBER = 95,                                      // u8/u8        Index number of the character in the text string ASCII character referenced by the above index number
    MSG_ID_COOLING_OPERATION_HOURS = 96,                                  // u16          Number of hours that the slave is in Cooling Mode. Reset by zero is optional for slave
    MSG_ID_POWER_CYCLES = 97,                                             // u16          Number of Power Cycles of a slave (wake-up after Reset), Reset by zero is optional for slave
    MSG_ID_RF_SENSOR_STATUS_INFORMATION = 98,                             // special/special   For a specific RF sensor the RF strength and battery level is written
    MSG_ID_REMOTE_OVERRIDE_OOPERATING_MODE_HEATING_DHW = 99,              // special/special   Operating Mode HC1, HC2/ Operating Mode DHW
    MSG_ID_REMOTE_OVERRIDE_FUNCTION = 100,                                // flag8/-   Function of manual and program changes in master and remote room Setpoint
    MSG_ID_STATUS_SOLAR_STORAGE = 101,                                    // flag8/flag8   Master and Slave Status flags Solar Storage
    MSG_ID_ASF_FLAGS_OEMFAULT_CODE_SOLAR_STORAGE = 102,                   // flag8/u8  Application-specific fault flags and OEM fault code Solar Storage
    MSG_ID_S_CONFIG_S_MEMBER_ID_CODE_SOLAR_STORAGE = 103,                 // flag8/u8  Slave Configuration Flags / Slave MemberID Code Solar Storage
    MSG_ID_SOLAR_STORAGE_VERSION = 104,                                   // u8/u8     Solar Storage product version number and type
    MSG_ID_TSP_SOLAR_SSTORAGE = 105,                                      // u8/u8     Number of Transparent - Slave - Parameters supported by TSP’s Solar Storage
    MSG_ID_TSP_INDEX_TSP_VALUE_SOLAR_STORAGE = 106,                       // u8/u8     Index number / Value of referred - to transparent TSP’s Solar Storage parameter.
    MSG_ID_FHB_SIZE_SOLAR_STORAGE = 107,                                  // u8/u8     Size of Fault - History - Buffer supported by Solar Storage
    MSG_ID_FHB_INDEX_FHB_VALUE_SOLAR_STORAGE = 108,                       // u8/u8     Index number / Value of referred - to fault - history buffer entry Solar Storage
    MSG_ID_ELECTRICITY_PRODUCER_STARTS = 109,                             // U16     Number of start of the electricity producer.
    MSG_ID_ELECTRICITY_PRODUCER_HOURS = 110,                              // U16     Number of hours the electricity produces is in operation
    MSG_ID_ELECTRICITY_PRODUCTION = 111,                                  // U16     Current electricity production in Watt.
    MSG_ID_CUMULATIV_ELECTRICITY_PRODUCTION = 112,                        // U16     Cumulative electricity production in KWh.
    MSG_ID_UNSUCCESSFULL_BURNER_STARTS = 113,                             // u16     Number of un - successful burner starts
    MSG_ID_FLAME_SIGNAL_TOO_LOW_NUMBER = 114,                             // u16     Number of times flame signal was too low
    MSG_ID_OEM_DDIAGNOSTIC_CODE = 115,                                    // u16     OEM - specific diagnostic / service code
    MSG_ID_SUCESSFULL_BURNER_SSTARTS = 116,                               // u16     Number of succesful starts burner
    MSG_ID_CH_PUMP_STARTS = 117,                                          // u16     Number of starts CH pump
    MSG_ID_DHW_PUPM_VALVE_STARTS = 118,                                   // u16     Number of starts DHW pump / valve
    MSG_ID_DHW_BURNER_STARTS = 119,                                       // u16     Number of starts burner during DHW mode
    MSG_ID_BURNER_OPERATION_HOURS = 120,                                  // u16     Number of hours that burner is in operation(i.e.flame on)
    MSG_ID_CH_PUMP_OPERATION_HOURS = 121,                                 // u16     Number of hours that CH pump has been running
    MSG_ID_DHW_PUMP_VALVE_OPERATION_HOURS = 122,                          // u16     Number of hours that DHW pump has been running or DHW valve has been opened
    MSG_ID_DHW_BURNER_OOPERATION_HOURS = 123,                             // u16     Number of hours that burner is in operation during DHW mode
    MSG_ID_OPENTERM_VERSION_MASTER = 124,                                 // f8.8    The implemented version of the OpenTherm Protocol Specification in the master.
    MSG_ID_OPENTERM_VERSION_SLAVE = 125,                                  // f8.8    The implemented version of the OpenTherm Protocol Specification in the slave.
    MSG_ID_MASTER_VERSION = 126,                                          // u8/u8     Master product version number and type
    MSG_ID_SLAVE_VERSION = 127,                                           // u8/u8     Slave product version number and type
} open_therm_message_id_t;

typedef enum OpenThermStatus
{
    OT_NOT_INITIALIZED,
    OT_READY,
    OT_DELAY,
    OT_REQUEST_SENDING,
    OT_RESPONSE_WAITING,
    OT_RESPONSE_START_BIT,
    OT_RESPONSE_RECEIVING,
    OT_RESPONSE_READY,
    OT_RESPONSE_INVALID
} esp_ot_opentherm_status_t;
// ENUMS

esp_err_t esp_ot_init(
    gpio_num_t _pin_in,
    gpio_num_t _pin_out,
    bool _esp_ot_is_slave,
    void (*esp_ot_process_responseCallback)(unsigned long, open_therm_response_status_t));

bool esp_ot_is_ready();

unsigned long esp_ot_send_request(unsigned long request);

bool esp_ot_send_response(unsigned long request);

bool esp_ot_send_request_async(unsigned long request);

unsigned long esp_ot_build_request(open_therm_message_type_t type, open_therm_message_id_t id, unsigned int data);

unsigned long esp_ot_build_response(open_therm_message_type_t type, open_therm_message_id_t id, unsigned int data);

unsigned long esp_ot_get_last_response();

open_therm_response_status_t esp_ot_get_last_response_status();

void esp_ot_handle_interrupt();

void process();

bool parity(unsigned long frame);

open_therm_message_type_t esp_ot_get_message_type(unsigned long message);

open_therm_message_id_t esp_ot_get_data_id(unsigned long frame);

bool esp_ot_is_valid_request(unsigned long request);

bool esp_ot_is_valid_response(unsigned long response);

int esp_ot_read_state();

void esp_ot_set_active_state();

void esp_ot_set_idle_state();

void esp_ot_activate_boiler();

void esp_ot_send_bit(bool high);

void esp_ot_process_response();

unsigned long esp_ot_build_set_boiler_status_request(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2);

unsigned long esp_ot_build_set_boiler_temperature_request(float temperature);

unsigned long esp_ot_build_get_boiler_temperature_request();

bool esp_ot_is_fault(unsigned long response);

bool esp_ot_is_central_heating_active(unsigned long response);

bool esp_ot_is_hot_water_active(unsigned long response);

bool esp_ot_is_flame_on(unsigned long response);

bool esp_ot_is_cooling_active(unsigned long response);

bool esp_ot_is_diagnostic(unsigned long response);

uint16_t esp_ot_get_uint(const unsigned long response);

float esp_ot_get_float(const unsigned long response);

unsigned int esp_ot_temperature_to_data(float temperature);

unsigned long esp_ot_set_boiler_status(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2);

bool esp_ot_set_boiler_temperature(float temperature);

float esp_ot_get_boiler_temperature();

float esp_ot_get_return_temperature();

bool esp_ot_set_dhw_setpoint(float temperature);

float esp_ot_get_dhw_temperature();

float esp_ot_get_modulation();

float esp_ot_get_pressure();

unsigned char esp_ot_get_fault();

unsigned long ot_reset();

unsigned long esp_ot_get_slave_product_version();

float esp_ot_get_slave_ot_version();