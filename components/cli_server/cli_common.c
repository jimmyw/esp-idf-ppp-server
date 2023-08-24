#include <inttypes.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

int run_multiple_commands(char *command_line, bool pre_print_command)
{
    char *saveptr = NULL;
    char *token = strtok_r(command_line, ";", &saveptr);
    while (token) {
        int ret;
        // Skip leading spaces.
        while (*token == ' ')
            token++;

        if (pre_print_command)
            printf("esp32> %s\n", token);

        esp_err_t err = esp_console_run(token, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command '%s'\n", token);
            return err;
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command '%s' returned non-zero error code: 0x%x (%s)\n", token, ret, esp_err_to_name(ret));
            return err;
        } else if (err != ESP_OK) {
            printf("Command '%s' Internal error: %s\n", token, esp_err_to_name(err));
            return err;
        } else {
            printf("Command '%s' executed successfully\n", token);
        }
        token = strtok_r(NULL, ";", &saveptr);
    }
    return 0;
}