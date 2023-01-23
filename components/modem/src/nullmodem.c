// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_modem_dce_service.h"
#include "nullmodem.h"


static const char *DCE_TAG = "nullmodem";

/**
 * @brief Macro defined for error checking
 *
 */
#define DCE_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(DCE_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)

/**
 * @brief Handle response from AT+QPOWD=1
 */
static esp_err_t nullmodem_handle_power_down(modem_dce_t *dce, const char *line)
{
    return ESP_OK;
}

/**
 * @brief Set Working Mode
 *
 * @param dce Modem DCE object
 * @param mode woking mode
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t nullmodem_set_working_mode(modem_dce_t *dce, modem_mode_t mode)
{
    dce->mode = MODEM_PPP_MODE;
    return ESP_OK;
}

/**
 * @brief Power down
 *
 * @param nullmodem_dce nullmodem object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t nullmodem_power_down(modem_dce_t *dce)
{
    return ESP_OK;
}

/**
 * @brief Deinitialize nullmodem object
 *
 * @param dce Modem DCE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on fail
 */
static esp_err_t nullmodem_deinit(modem_dce_t *dce)
{
    esp_modem_dce_t *esp_modem_dce = __containerof(dce, esp_modem_dce_t, parent);
    if (dce->dte) {
        dce->dte->dce = NULL;
    }
    free(esp_modem_dce);
    return ESP_OK;
}

esp_err_t nullmodem_dce_sync(modem_dce_t *dce)
{
    return ESP_OK;

}

esp_err_t nullmodem_dce_echo(modem_dce_t *dce, bool on)
{
    return ESP_OK;
}

esp_err_t nullmodem_dce_store_profile(modem_dce_t *dce)
{
    return ESP_OK;
}

esp_err_t nullmodem_dce_set_flow_ctrl(modem_dce_t *dce, modem_flow_ctrl_t flow_ctrl)
{
    return ESP_OK;
}

esp_err_t nullmodem_dce_define_pdp_context(modem_dce_t *dce, uint32_t cid, const char *type, const char *apn)
{
    return ESP_OK;
}


esp_err_t nullmodem_dce_get_signal_quality(modem_dce_t *dce, uint32_t *rssi, uint32_t *ber)
{
    return ESP_OK;
}

esp_err_t nullmodem_dce_get_battery_status(modem_dce_t *dce, uint32_t *bcs, uint32_t *bcl, uint32_t *voltage)
{
    return ESP_OK;
}

esp_err_t nullmodem_dce_get_operator_name(modem_dce_t *dce)
{
    return ESP_OK;
}

modem_dce_t *nullmodem_init(modem_dte_t *dte)
{
    DCE_CHECK(dte, "DCE should bind with a DTE", err);
    /* malloc memory for esp_modem_dce object */
    esp_modem_dce_t *esp_modem_dce = calloc(1, sizeof(esp_modem_dce_t));
    DCE_CHECK(esp_modem_dce, "calloc nullmodem_dce failed", err);
    /* Bind DTE with DCE */
    esp_modem_dce->parent.dte = dte;
    dte->dce = &(esp_modem_dce->parent);
    /* Bind methods */
    esp_modem_dce->parent.handle_line = NULL;
    esp_modem_dce->parent.sync = nullmodem_dce_sync;
    esp_modem_dce->parent.echo_mode = nullmodem_dce_echo;
    esp_modem_dce->parent.store_profile = nullmodem_dce_store_profile;
    esp_modem_dce->parent.set_flow_ctrl = nullmodem_dce_set_flow_ctrl;
    esp_modem_dce->parent.define_pdp_context = nullmodem_dce_define_pdp_context;
    esp_modem_dce->parent.hang_up = esp_modem_dce_hang_up;
    esp_modem_dce->parent.get_signal_quality = nullmodem_dce_get_signal_quality;
    esp_modem_dce->parent.get_battery_status = nullmodem_dce_get_battery_status;
    esp_modem_dce->parent.get_operator_name = nullmodem_dce_get_operator_name;
    esp_modem_dce->parent.set_working_mode = nullmodem_set_working_mode;
    esp_modem_dce->parent.power_down = nullmodem_power_down;
    esp_modem_dce->parent.deinit = nullmodem_deinit;
    return &(esp_modem_dce->parent);
err:
    return NULL;
}
