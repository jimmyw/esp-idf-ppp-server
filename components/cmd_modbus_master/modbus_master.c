/*
 * SPDX-FileCopyrightText: 2016-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// FreeModbus Master Example ESP32

#include <string.h>
#include <sys/queue.h>
#include "esp_log.h"
#include "esp_system.h"
//#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_console.h"
//#include "nvs_flash.h"
//#include "esp_netif.h"
//#include "mdns.h"
//#include "protocol_examples_common.h"

#include "modbus_params.h"  // for modbus parameters structures
#include "mbcontroller.h"
#include "sdkconfig.h"

#undef CONFIG_MB_MDNS_IP_RESOLVER

#define MB_TCP_PORT                     (CONFIG_FMB_TCP_PORT_DEFAULT)   // TCP port used by example

// The number of parameters that intended to be used in the particular control process
#define MASTER_MAX_CIDS num_device_parameters

// Number of reading of parameters from slave
#define MASTER_MAX_RETRY                (30)

// Timeout to update cid over Modbus
#define UPDATE_CIDS_TIMEOUT_MS          (500)
#define UPDATE_CIDS_TIMEOUT_TICS        (UPDATE_CIDS_TIMEOUT_MS / portTICK_PERIOD_MS)

// Timeout between polls
#define POLL_TIMEOUT_MS                 (1)
#define POLL_TIMEOUT_TICS               (POLL_TIMEOUT_MS / portTICK_PERIOD_MS)
#define MB_MDNS_PORT                    (502)

// The macro to get offset for parameter in the appropriate structure
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) + 1))
#define COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_t, field) + 1))
#define DISCR_OFFSET(field) ((uint16_t)(offsetof(discrete_reg_params_t, field) + 1))
#define STR(fieldname) ((const char*)( fieldname ))

// Options can be used as bit masks or parameter limits
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

#define MB_ID_BYTE0(id) ((uint8_t)(id))
#define MB_ID_BYTE1(id) ((uint8_t)(((uint16_t)(id) >> 8) & 0xFF))
#define MB_ID_BYTE2(id) ((uint8_t)(((uint32_t)(id) >> 16) & 0xFF))
#define MB_ID_BYTE3(id) ((uint8_t)(((uint32_t)(id) >> 24) & 0xFF))

#define MB_ID2STR(id) MB_ID_BYTE0(id), MB_ID_BYTE1(id), MB_ID_BYTE2(id), MB_ID_BYTE3(id)

#if CONFIG_FMB_CONTROLLER_SLAVE_ID_SUPPORT
#define MB_DEVICE_ID (uint32_t)CONFIG_FMB_CONTROLLER_SLAVE_ID
#else
#define MB_DEVICE_ID (uint32_t)0x00112233
#endif

#define MB_MDNS_INSTANCE(pref) pref"mb_master_tcp"
static const char *TAG = "MASTER_TEST";

// Enumeration of modbus device addresses accessed by master device
// Each address in the table is a index of TCP slave ip address in mb_communication_info_t::tcp_ip_addr table
enum {
    MB_DEVICE_ADDR1 = 1, // Slave address 1
    MB_DEVICE_ADDR2 = 200,
    MB_DEVICE_ADDR3 = 35
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
    CID_INP_DATA_0 = 0,
    CID_HOLD_DATA_0,
    CID_INP_DATA_1,
    CID_HOLD_DATA_1,
    CID_INP_DATA_2,
    CID_HOLD_DATA_2,
    CID_HOLD_TEST_REG,
    CID_RELAY_P1,
    CID_RELAY_P2,
    CID_COUNT
};

// Example Data (Object) Dictionary for Modbus parameters:
// The CID field in the table must be unique.
// Modbus Slave Addr field defines slave address of the device with correspond parameter.
// Modbus Reg Type - Type of Modbus register area (Holding register, Input Register and such).
// Reg Start field defines the start Modbus register number and Reg Size defines the number of registers for the characteristic accordingly.
// The Instance Offset defines offset in the appropriate parameter structure that will be used as instance to save parameter value.
// Data Type, Data Size specify type of the characteristic and its data size.
// Parameter Options field specifies the options that can be used to process parameter value (limits or masks).
// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    { CID_INP_DATA_0, STR("Data_channel_0"), STR("Volts"), MB_DEVICE_ADDR1, MB_PARAM_INPUT, 0, 2,
                    INPUT_OFFSET(input_data0), PARAM_TYPE_FLOAT, 4, OPTS( -10, 10, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_0, STR("Humidity_1"), STR("%rH"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, 2,
            HOLD_OFFSET(holding_data0), PARAM_TYPE_FLOAT, 4, OPTS( 0, 100, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_INP_DATA_1, STR("Temperature_1"), STR("C"), MB_DEVICE_ADDR1, MB_PARAM_INPUT, 2, 2,
            INPUT_OFFSET(input_data1), PARAM_TYPE_FLOAT, 4, OPTS( -40, 100, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_1, STR("Humidity_2"), STR("%rH"), MB_DEVICE_ADDR2, MB_PARAM_HOLDING, 2, 2,
            HOLD_OFFSET(holding_data1), PARAM_TYPE_FLOAT, 4, OPTS( 0, 100, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_INP_DATA_2, STR("Temperature_2"), STR("C"), MB_DEVICE_ADDR2, MB_PARAM_INPUT, 4, 2,
            INPUT_OFFSET(input_data2), PARAM_TYPE_FLOAT, 4, OPTS( -40, 100, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_2, STR("Humidity_3"), STR("%rH"), MB_DEVICE_ADDR3, MB_PARAM_HOLDING, 4, 2,
            HOLD_OFFSET(holding_data2), PARAM_TYPE_FLOAT, 4, OPTS( 0, 100, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_TEST_REG, STR("Test_regs"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 8, 100,
            HOLD_OFFSET(test_regs), PARAM_TYPE_ASCII, 200, OPTS( 0, 100, 1 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_RELAY_P1, STR("RelayP1"), STR("on/off"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 8,
            COIL_OFFSET(coils_port0), PARAM_TYPE_U16, 1, OPTS( BIT1, 0, 0 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_RELAY_P2, STR("RelayP2"), STR("on/off"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 8, 8,
            COIL_OFFSET(coils_port1), PARAM_TYPE_U16, 1, OPTS( BIT0, 0, 0 ), PAR_PERMS_READ_WRITE_TRIGGER }
};

// Calculate number of parameters in the table
const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));

// This table represents slave IP addresses that correspond to the short address field of the slave in device_parameters structure
// Modbus TCP stack shall use these addresses to be able to connect and read parameters from slave
char* slave_ip_address_table[] = {
    "10.10.0.2",     // Address corresponds to MB_DEVICE_ADDR1 and set to predefined value by user
    "10.10.0.2",     // Corresponds to characteristic MB_DEVICE_ADDR2
    "10.10.0.2",     // Corresponds to characteristic MB_DEVICE_ADDR3
    NULL              // End of table condition (must be included)
};

const size_t ip_table_sz = (size_t)(sizeof(slave_ip_address_table) / sizeof(slave_ip_address_table[0]));


// The function to get pointer to parameter storage (instance) according to parameter description table
static void* master_get_param_data(const mb_parameter_descriptor_t* param_descriptor)
{
    assert(param_descriptor != NULL);
    void* instance_ptr = NULL;
    if (param_descriptor->param_offset != 0) {
       switch(param_descriptor->mb_param_type)
       {
           case MB_PARAM_HOLDING:
               instance_ptr = ((void*)&holding_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_INPUT:
               instance_ptr = ((void*)&input_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_COIL:
               instance_ptr = ((void*)&coil_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_DISCRETE:
               instance_ptr = ((void*)&discrete_reg_params + param_descriptor->param_offset - 1);
               break;
           default:
               instance_ptr = NULL;
               break;
       }
    } else {
        ESP_LOGE(TAG, "Wrong parameter offset for CID #%d", param_descriptor->cid);
        assert(instance_ptr != NULL);
    }
    return instance_ptr;
}

// User operation function to read slave values and check alarm
static void master_operation_func(void *arg)
{
    esp_err_t err = ESP_OK;
    float value = 0;
    bool alarm_state = false;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(TAG, "Start modbus test...");

    for(uint16_t retry = 0; retry <= MASTER_MAX_RETRY && (!alarm_state); retry++) {
        // Read all found characteristics from slave(s)
        for (uint16_t cid = 0; (err != ESP_ERR_NOT_FOUND) && cid < MASTER_MAX_CIDS; cid++)
        {
            // Get data from parameters description table
            // and use this information to fill the characteristics description table
            // and having all required fields in just one table
            err = mbc_master_get_cid_info(cid, &param_descriptor);
            if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                if ((param_descriptor->param_type == PARAM_TYPE_ASCII) &&
                        (param_descriptor->cid == CID_HOLD_TEST_REG)) {
                   // Check for long array of registers of type PARAM_TYPE_ASCII
                    err = mbc_master_get_parameter(cid, (char*)param_descriptor->param_key,
                                                                            (uint8_t*)temp_data_ptr, &type);
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "Characteristic #%d %s (%s) value = (0x%08x) read successful.",
                                                 param_descriptor->cid,
                                                 (char*)param_descriptor->param_key,
                                                 (char*)param_descriptor->param_units,
                                                 *(uint32_t*)temp_data_ptr);
                        // Initialize data of test array and write to slave
                        if (*(uint32_t*)temp_data_ptr != 0xAAAAAAAA) {
                            memset((void*)temp_data_ptr, 0xAA, param_descriptor->param_size);
                            *(uint32_t*)temp_data_ptr = 0xAAAAAAAA;
                            err = mbc_master_set_parameter(cid, (char*)param_descriptor->param_key,
                                                              (uint8_t*)temp_data_ptr, &type);
                            if (err == ESP_OK) {
                                ESP_LOGI(TAG, "Characteristic #%d %s (%s) value = (0x%08x), write successful.",
                                                            param_descriptor->cid,
                                                            (char*)param_descriptor->param_key,
                                                            (char*)param_descriptor->param_units,
                                                            *(uint32_t*)temp_data_ptr);
                            } else {
                                ESP_LOGE(TAG, "Characteristic #%d (%s) write fail, err = 0x%x (%s).",
                                                        param_descriptor->cid,
                                                        (char*)param_descriptor->param_key,
                                                        (int)err,
                                                        (char*)esp_err_to_name(err));
                            }
                        }
                    } else {
                        ESP_LOGE(TAG, "Characteristic #%d (%s) read fail, err = 0x%x (%s).",
                                                param_descriptor->cid,
                                                (char*)param_descriptor->param_key,
                                                (int)err,
                                                (char*)esp_err_to_name(err));
                    }
                } else {
                    err = mbc_master_get_parameter(cid, (char*)param_descriptor->param_key,
                                                        (uint8_t*)temp_data_ptr, &type);
                    if (err == ESP_OK) {
                        if ((param_descriptor->mb_param_type == MB_PARAM_HOLDING) ||
                            (param_descriptor->mb_param_type == MB_PARAM_INPUT)) {
                            value = *(float*)temp_data_ptr;
                            ESP_LOGI(TAG, "Characteristic #%d %s (%s) value = %f (0x%x) read successful.",
                                            param_descriptor->cid,
                                            (char*)param_descriptor->param_key,
                                            (char*)param_descriptor->param_units,
                                            value,
                                            *(uint32_t*)temp_data_ptr);
                            if (((value > param_descriptor->param_opts.max) ||
                                (value < param_descriptor->param_opts.min))) {
                                    alarm_state = true;
                                    break;
                            }
                        } else {
                            uint8_t state = *(uint8_t*)temp_data_ptr;
                            const char* rw_str = (state & param_descriptor->param_opts.opt1) ? "ON" : "OFF";
                            ESP_LOGI(TAG, "Characteristic #%d %s (%s) value = %s (0x%x) read successful.",
                                            param_descriptor->cid,
                                            (char*)param_descriptor->param_key,
                                            (char*)param_descriptor->param_units,
                                            (const char*)rw_str,
                                            *(uint8_t*)temp_data_ptr);
                            if (state & param_descriptor->param_opts.opt1) {
                                alarm_state = true;
                                break;
                            }
                        }
                    } else {
                        ESP_LOGE(TAG, "Characteristic #%d (%s) read fail, err = 0x%x (%s).",
                                            param_descriptor->cid,
                                            (char*)param_descriptor->param_key,
                                            (int)err,
                                            (char*)esp_err_to_name(err));
                    }
                }
                vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls
            }
        }
        vTaskDelay(UPDATE_CIDS_TIMEOUT_TICS);
    }

    if (alarm_state) {
        ESP_LOGI(TAG, "Alarm triggered by cid #%d.",
                                        param_descriptor->cid);
    } else {
        ESP_LOGE(TAG, "Alarm is not triggered after %d retries.",
                                        MASTER_MAX_RETRY);
    }

}


// Modbus master initialization
static esp_err_t master_init(mb_communication_info_t* comm_info)
{
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init_tcp(&master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE,
                                TAG,
                                "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            TAG,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);

    err = mbc_master_setup((void*)comm_info);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            TAG,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);

    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                TAG,
                                "mb controller set descriptor fail, returns(0x%x).",
                                (uint32_t)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            TAG,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);
    vTaskDelay(5);
    return err;
}


static int do_modbus_master_connect_cmd(int argc, char **argv)
{


    mb_communication_info_t comm_info = { 0 };
    comm_info.ip_port = MB_TCP_PORT;
    comm_info.ip_addr_type = MB_IPV4;
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr = (void*)slave_ip_address_table;
    //comm_info.ip_netif_ptr = (void*)get_example_netif();
    comm_info.ip_netif_ptr = (void*)0x1; // Random fake non-null address, unused.

    ESP_ERROR_CHECK(master_init(&comm_info)); 
    return 0;
}
static int do_modbus_master_poll_cmd(int argc, char **argv)
{
    master_operation_func(NULL);
    return 0;
}

void register_modbus_master()
{
    const esp_console_cmd_t modbus_master_connect_cmd = {
        .command = "modbus_master_connect",
        .help = "start modbus master",
        .hint = NULL,
        .func = &do_modbus_master_connect_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&modbus_master_connect_cmd));
    const esp_console_cmd_t modbus_master_poll_cmd = {
        .command = "modbus_master_poll",
        .help = "poll modbus",
        .hint = NULL,
        .func = &do_modbus_master_poll_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&modbus_master_poll_cmd));
}

