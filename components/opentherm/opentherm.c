/**
* @package     Opentherm library for ESP-IDF framework
* @author:     Mikhail Sazanof
* @copyright   Copyright (C) 2024 - Sazanof.ru
* @licence     MIT
*/

#include <stdio.h>
#include "esp_log.h"
#include "opentherm.h"
#include "rom/ets_sys.h"

static const char *TAG = "ot-example";

gpio_num_t pin_in = CONFIG_OT_IN_PIN;
gpio_num_t pin_out = CONFIG_OT_OUT_PIN;

typedef uint8_t byte;

bool esp_ot_is_slave;

void (*esp_ot_process_response_callback)(unsigned long, open_therm_response_status_t);

volatile unsigned long response;

volatile esp_ot_opentherm_status_t esp_ot_status;

volatile open_therm_response_status_t esp_ot_response_status;

volatile unsigned long esp_ot_response_timestamp;

volatile byte esp_ot_response_bit_index;

/**
 * Initialize opentherm: gpio, install isr, basic data
 *
 * @return  void 
 */
esp_err_t esp_ot_init(
    gpio_num_t _pin_in,
    gpio_num_t _pin_out,
    bool _esp_ot_is_slave,
    void (*esp_ot_process_responseCallback)(unsigned long, open_therm_response_status_t))
{

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK)
    {
        ESP_LOGE("ISR", "Error with state %s", esp_err_to_name(err));
    }

    pin_in = _pin_in;
    pin_out = _pin_out;

    // Initialize the GPIO
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin_in);
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin_out);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_isr_handler_add(pin_in, esp_ot_handle_interrupt, NULL);

    esp_ot_is_slave = _esp_ot_is_slave;

    esp_ot_process_response_callback = esp_ot_process_responseCallback;

    response = 0;

    esp_ot_response_status = OT_STATUS_NONE;

    esp_ot_response_timestamp = 0;

    gpio_intr_enable(pin_in);

    esp_ot_status = OT_READY;

    ESP_LOGI(TAG, "Initialize opentherm with in: %d out: %d", pin_in, pin_out);

    return ESP_OK;
}


/**
 * Send bit helper
 *
 * @return  void
 */
void esp_ot_send_bit(bool high)
{
    if (high)
        esp_ot_set_active_state();
    else
        esp_ot_set_idle_state();
    ets_delay_us(500);
    if (high)
        esp_ot_set_idle_state();
    else
        esp_ot_set_active_state();
    ets_delay_us(500);
}

/**
 * Request builder for boiler status
 *
 * @return  long
 */
unsigned long esp_ot_build_set_boiler_status_request(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2)
{
    unsigned int data = enableCentralHeating | (enableHotWater << 1) | (enableCooling << 2) | (enableOutsideTemperatureCompensation << 3) | (enableCentralHeating2 << 4);
    data <<= 8;
    return esp_ot_build_request(OT_READ_DATA, MSG_ID_STATUS, data);
}

/**
 * Request builder for setting up boiler temperature
 *
 * @param   float  temperature 
 *
 * @return  long  
 */
unsigned long esp_ot_build_set_boiler_temperature_request(float temperature)
{
    unsigned int data = esp_ot_temperature_to_data(temperature);
    return esp_ot_build_request(OT_WRITE_DATA, MSG_ID_T_SET, data);
}

/**
 * Request builder to get boiler temperature
 *
 * @return  long 
 */
unsigned long esp_ot_build_get_boiler_temperature_request()
{
    return esp_ot_build_request(OT_READ_DATA, MSG_ID_TBOILER, 0);
}

/**
 * [IRAM_ATTR] Check if status is ready
 *
 * @return  bool
 */
bool IRAM_ATTR esp_ot_is_ready()
{
    return esp_ot_status == OT_READY;
}

/**
 * [IRAM_ATTR] Read pin in state
 *
 * @return  int     [return description]
 */
int IRAM_ATTR esp_ot_read_state()
{
    return gpio_get_level(pin_in);
}

/**
 * Set active state helper
 *
 * @return  void 
 */
void esp_ot_set_active_state()
{
    gpio_set_level(pin_out, 0);
}

/**
 * Set idle state helper
 *
 * @return  void 
 */
void esp_ot_set_idle_state()
{
    gpio_set_level(pin_out, 1);
}

/**
 * Activate boiler helper
 *
 * @return  void   
 */
void esp_ot_activate_boiler()
{
    esp_ot_set_idle_state();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
/**
 * Process response execution
 *
 * @return  void
 */
void esp_ot_process_response()
{
    if (esp_ot_process_response_callback != NULL)
    {
        // ESP_LOGI("PROCESS RESPONSE", "esp_ot_process_response, %ld, %d", response, esp_ot_response_status);
        esp_ot_process_response_callback(response, esp_ot_response_status);
    }
}

/**
 * Get message type
 *
 * @param   long message
 *
 * @return  open_therm_message_type_t 
 */
open_therm_message_type_t esp_ot_get_message_type(unsigned long message)
{
    open_therm_message_type_t msg_type = (open_therm_message_type_t)((message >> 28) & 7);
    return msg_type;
}

/**
 * Get data id
 *
 * @param   long  frame
 *
 * @return  open_therm_message_id_t
 */
open_therm_message_id_t esp_ot_get_data_id(unsigned long frame)
{
    return (open_therm_message_id_t)((frame >> 16) & 0xFF);
}

/**
 * Build a request
 *
 * @param   int   data 
 *
 * @return  long 
 */
unsigned long esp_ot_build_request(open_therm_message_type_t type, open_therm_message_id_t id, unsigned int data)
{
    unsigned long request = data;
    if (type == OT_WRITE_DATA)
    {
        request |= 1ul << 28;
    }
    request |= ((unsigned long)id) << 16;
    if (parity(request))
    {
        request |= (1ul << 31);
    }
    return request;
}

/**
 * Build response
 *
 * @param   int   data 
 *
 * @return  long 
 */
unsigned long esp_ot_build_response(open_therm_message_type_t type, open_therm_message_id_t id, unsigned int data)
{
    unsigned long response = data;
    response |= ((unsigned long)type) << 28;
    response |= ((unsigned long)id) << 16;
    if (parity(response))
        response |= (1ul << 31);
    return response;
}

/**
 * Check if request is valid
 *
 * @param   long  request 
 *
 * @return  bool 
 */
bool esp_ot_is_valid_request(unsigned long request)
{
    if (parity(request))
        return false;
    byte msgType = (request << 1) >> 29;
    return msgType == (byte)OT_READ_DATA || msgType == (byte)OT_WRITE_DATA;
}

/**
 * Check if response is valid
 *
 * @param   long  response
 *
 * @return  bool 
 */
bool esp_ot_is_valid_response(unsigned long response)
{
    if (parity(response))
        return false;
    byte msgType = (response << 1) >> 29;
    return msgType == (byte)OT_READ_ACK || msgType == (byte)OT_WRITE_ACK;
}

/**
 * Parity helper
 *
 * @param   long  frame 
 *
 * @return  bool 
 */
bool parity(unsigned long frame) // odd parity
{
    byte p = 0;
    while (frame > 0)
    {
        if (frame & 1)
            p++;
        frame = frame >> 1;
    }
    return (p & 1);
}

/**
 * Reset helper
 *
 * @return  long  
 */
unsigned long ot_reset()
{
    unsigned int data = 1 << 8;
    return esp_ot_send_request(esp_ot_build_request(OT_WRITE_DATA, MSG_ID_REMOTE_REQUEST, data));
}

/**
 * Get slave product version
 *
 * @return  long 
 */
unsigned long esp_ot_get_slave_product_version()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_SLAVE_VERSION, 0));
    return esp_ot_is_valid_response(response) ? response : 0;
}

/**
 * Get slave configuration
 *
 * @return  long 
 */
unsigned long ot_get_slave_configuration()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_S_CONFIG_S_MEMEBER_ID_CODE, 0));
    return esp_ot_is_valid_response(response) ? response : 0;
}

/**
 * Get slave opentherm version
 *
 * @return  float
 */
float esp_ot_get_slave_ot_version()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_OPENTERM_VERSION_SLAVE, 0));
    return esp_ot_is_valid_response(response) ? esp_ot_get_float(response) : 0;
}

/**
 * Handle interrupt helper
 *
 * @return  void
 */
void IRAM_ATTR esp_ot_handle_interrupt()
{
    // ESP_DRAM_LOGI("esp_ot_handle_interrupt", "%ld", status);
    if (esp_ot_is_ready())
    {
        if (esp_ot_is_slave && esp_ot_read_state() == 1)
        {
            esp_ot_status = OT_RESPONSE_WAITING;
        }
        else
        {
            return;
        }
    }

    unsigned long newTs = esp_timer_get_time();
    if (esp_ot_status == OT_RESPONSE_WAITING)
    {
        if (esp_ot_read_state() == 1)
        {
            // ESP_DRAM_LOGI("BIT", "Set start bit");
            esp_ot_status = OT_RESPONSE_START_BIT;
            esp_ot_response_timestamp = newTs;
        }
        else
        {
            // ESP_DRAM_LOGI("BIT", "Set OT_RESPONSE_INVALID");
            esp_ot_status = OT_RESPONSE_INVALID;
            esp_ot_response_timestamp = newTs;
        }
    }
    else if (esp_ot_status == OT_RESPONSE_START_BIT)
    {
        if ((newTs - esp_ot_response_timestamp < 750) && esp_ot_read_state() == 0)
        {
            esp_ot_status = OT_RESPONSE_RECEIVING;
            esp_ot_response_timestamp = newTs;
            esp_ot_response_bit_index = 0;
        }
        else
        {
            esp_ot_status = OT_RESPONSE_INVALID;
            esp_ot_response_timestamp = newTs;
        }
    }
    else if (esp_ot_status == OT_RESPONSE_RECEIVING)
    {
        if ((newTs - esp_ot_response_timestamp) > 750)
        {
            if (esp_ot_response_bit_index < 32)
            {
                response = (response << 1) | !esp_ot_read_state();
                esp_ot_response_timestamp = newTs;
                esp_ot_response_bit_index++;
            }
            else
            { // stop bit
                esp_ot_status = OT_RESPONSE_READY;
                esp_ot_response_timestamp = newTs;
            }
        }
    }
}

/**
 * Process function
 *
 * @return  void   
 */
void process()
{
    PORT_ENTER_CRITICAL;
    esp_ot_opentherm_status_t st = esp_ot_status;
    unsigned long ts = esp_ot_response_timestamp;
    PORT_EXIT_CRITICAL;

    if (st == OT_READY)
    {
        return;
    }
    unsigned long newTs = esp_timer_get_time();
    if (st != OT_NOT_INITIALIZED && st != OT_DELAY && (newTs - ts) > 1000000)
    {
        esp_ot_status = OT_READY;
        ESP_LOGI("SET STATUS", "set status to READY"); // here READY
        esp_ot_response_status = OT_STATUS_TIMEOUT;
        esp_ot_process_response();
    }
    else if (st == OT_RESPONSE_INVALID)
    {
        ESP_LOGE("SET STATUS", "set status to OT_RESPONSE_INVALID"); // here OT_RESPONSE_INVALID
        esp_ot_status = OT_DELAY;
        esp_ot_response_status = OT_STATUS_INVALID;
        esp_ot_process_response();
    }
    else if (st == OT_RESPONSE_READY)
    {
        esp_ot_status = OT_DELAY;
        esp_ot_response_status = (esp_ot_is_slave ? esp_ot_is_valid_request(response) : esp_ot_is_valid_response(response)) ? OT_STATUS_SUCCESS : OT_STATUS_INVALID;
        esp_ot_process_response();
    }
    else if (st == OT_DELAY)
    {
        if ((newTs - ts) > (esp_ot_is_slave ? 20000 : 100000))
        {
            esp_ot_status = OT_READY;
        }
    }
}

/**
 * Send request async
 *
 * @param   long  request 
 *
 * @return  bool   
 */
bool esp_ot_send_request_async(unsigned long request)
{
    PORT_ENTER_CRITICAL;
    const bool ready = esp_ot_is_ready();
    PORT_EXIT_CRITICAL;
    if (!ready)
    {
        return false;
    }
    PORT_ENTER_CRITICAL;
    esp_ot_status = OT_REQUEST_SENDING;
    response = 0;
    esp_ot_response_status = OT_STATUS_NONE;
    PORT_EXIT_CRITICAL;

    // vTaskSuspendAll();

    esp_ot_send_bit(1); // start bit
    for (int i = 31; i >= 0; i--)
    {
        esp_ot_send_bit(bitRead(request, i));
    }
    esp_ot_send_bit(1); // stop bit
    esp_ot_set_idle_state();

    esp_ot_response_timestamp = esp_timer_get_time();
    esp_ot_status = OT_RESPONSE_WAITING;

    // xTaskResumeAll();

    return true;
}

/**
 * Send request
 *
 * @param   long  request
 *
 * @return  long 
 */
unsigned long esp_ot_send_request(unsigned long request)
{
    if (!esp_ot_send_request_async(request))
    {
        return 0;
    }
    // ESP_LOGI("STATUS", "esp_ot_send_request with status %d", status); // here WAITING
    while (!esp_ot_is_ready())
    {
        process();
        vPortYield();
    }
    return response;
}

/**
 * Check if response fault
 *
 * @param   long  response
 *
 * @return  bool  
 */
bool esp_ot_is_fault(unsigned long response)
{
    return response & 0x1;
}

/**
 * Check if central heating is active
 *
 * @param   long  response 
 *
 * @return  bool        
 */
bool esp_ot_is_central_heating_active(unsigned long response)
{
    return response & 0x2;
}

/**
 * Check if hot water is active
 *
 * @param   long  response 
 *
 * @return  bool 
 */
bool esp_ot_is_hot_water_active(unsigned long response)
{
    return response & 0x4;
}

/**
 * Check if flame is on
 *
 * @param   long  response 
 *
 * @return  bool   
 */
bool esp_ot_is_flame_on(unsigned long response)
{
    return response & 0x8;
}

/**
 * Check if cooling is active
 *
 * @param   long  response  [response description]
 *
 * @return  bool            [return description]
 */
bool esp_ot_is_cooling_active(unsigned long response)
{
    return response & 0x10;
}

/**
 * Check if response has diagnostic
 *
 * @param   long  response  [response description]
 *
 * @return  bool            [return description]
 */
bool esp_ot_is_diagnostic(unsigned long response)
{
    return response & 0x40;
}

/**
 * Get uint value
 *
 * @param   long      response 
 *
 * @return  uint16_t         
 */
uint16_t esp_ot_get_uint(const unsigned long response)
{
    const uint16_t u88 = response & 0xffff;
    return u88;
}

/**
 * Get float value
 *
 * @param   long   response 
 *
 * @return  float       
 */
float esp_ot_get_float(const unsigned long response)
{
    const uint16_t u88 = esp_ot_get_uint(response);
    const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
    return f;
}

/**
 * Get data from temperature
 *
 * @param   float  temperature 
 *
 * @return  int        
 */
unsigned int esp_ot_temperature_to_data(float temperature)
{
    if (temperature < 0)
    {
        temperature = 0;
    }

    if (temperature > 100)
    {
        temperature = 100;
    }

    unsigned int data = (unsigned int)(temperature * 256);
    return data;
}

/**
 * Sets bioler status
 *
 * @param bool  enableCentralHeating  enable central heating or not
 * @param bool  enableHotWater
 * @param bool  enableCooling
 * @param bool  enableOutsideTemperatureCompensation
 * @param bool  enableCentralHeating2
 * 
 * @return long boiler status
 */
unsigned long esp_ot_set_boiler_status(
    bool enableCentralHeating,
    bool enableHotWater,
    bool enableCooling,
    bool enableOutsideTemperatureCompensation,
    bool enableCentralHeating2)
{
    return esp_ot_send_request(esp_ot_build_set_boiler_status_request(enableCentralHeating, enableHotWater, enableCooling, enableOutsideTemperatureCompensation, enableCentralHeating2));
}

/**
 * Set boler temperature
 *
 * @param   float temperature target temperature setpoint
 *
 * @return  bool
 */
bool esp_ot_set_boiler_temperature(float temperature)
{
    unsigned long response = esp_ot_send_request(esp_ot_build_set_boiler_temperature_request(temperature));
    return esp_ot_is_valid_response(response);
}

/**
 * Get current boiler temperature
 *
 * @return  float   target temperature
 */
float esp_ot_get_boiler_temperature()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_get_boiler_temperature_request());
    return esp_ot_is_valid_response(response) ? esp_ot_get_float(response) : 0;
}

/**
 * Get return temperature data
 *
 * @return  float 
 */
float esp_ot_get_return_temperature()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_TRET, 0));
    return esp_ot_is_valid_response(response) ? esp_ot_get_float(response) : 0;
}


/**
 * Set hot water setpoint
 *
 * @param   float  temperature
 *
 * @return  bool  
 */
bool esp_ot_set_dhw_setpoint(float temperature)
{
    unsigned int data = esp_ot_temperature_to_data(temperature);
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_WRITE_DATA, MSG_ID_TDHW_SET, data));
    return esp_ot_is_valid_response(response);
}

/**
 * Get hot water temperature
 *
 * @return  float 
 */
float esp_ot_get_dhw_temperature()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_TDHW, 0));
    return esp_ot_is_valid_response(response) ? esp_ot_get_float(response) : 0;
}

/**
 * Get modulation
 *
 * @return  float  
 */
float esp_ot_get_modulation()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_REL_MOD_LEVEL, 0));
    return esp_ot_is_valid_response(response) ? esp_ot_get_float(response) : 0;
}

/**
 * Get pressure
 *
 * @return  float 
 */
float esp_ot_get_pressure()
{
    unsigned long response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_CH_PRESSURE, 0));
    return esp_ot_is_valid_response(response) ? esp_ot_get_float(response) : 0;
}

/**
 * Is boiler status fault, get ASF flags
 *
 * @return  char    [return description]
 */
unsigned char esp_ot_get_fault()
{
    return ((esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_ASF_FLAGS, 0)) >> 8) & 0xff);
}

/**
 * Get last response status
 *
 * @return  open_therm_response_status_t
 */
open_therm_response_status_t esp_ot_get_last_response_status()
{
    return esp_ot_response_status;
}


/**
 * Send response
 *
 * @param   long  request  
 *
 * @return  bool         
 */
bool esp_ot_send_response(unsigned long request)
{
    PORT_ENTER_CRITICAL;
    const bool ready = esp_ot_is_ready();

    if (!ready)
    {
        PORT_EXIT_CRITICAL;
        return false;
    }

    esp_ot_status = OT_REQUEST_SENDING;
    response = 0;
    esp_ot_response_status = OT_STATUS_NONE;

    // vTaskSuspendAll();

    PORT_EXIT_CRITICAL;

    esp_ot_send_bit(1); // start bit
    for (int i = 31; i >= 0; i--)
    {
        esp_ot_send_bit(bitRead(request, i));
    }
    esp_ot_send_bit(1); // stop bit
    esp_ot_set_idle_state();
    esp_ot_status = OT_READY;
    // xTaskResumeAll();

    return true;
}


/**
 * Get last response
 *
 * @return  long  
 */
unsigned long esp_ot_get_last_response()
{
    return response;
}