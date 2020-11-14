#include "esp32_rmt_common.h"
#include "esp32_rmt_remotes.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"

#include "driver/rmt.h"
#include "driver/periph_ctrl.h"
#include "soc/rmt_reg.h"

#include "freertos/ringbuf.h"

#define TX_PIN_SSSS 13
const char* RAW_TAG = "RAW";

/*
 * SEND
 */

#if SEND_RAW

void raw_tx_init(gpio_num_t gpio_num, unsigned int frequency)
{
	rmt_config_t rmt_tx;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = gpio_num;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = RMT_CARRIER_DUTY;
    rmt_tx.tx_config.carrier_freq_hz = frequency;
    rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
    rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = RMT_MODE_TX;
	
	rmt_tx_init(&rmt_tx);
}

void rmtlib_raw_build_items(rmt_item32_t* item, int data[], int data_len)
{
    for(int i=0; i < data_len; i+=2) {
        // I don't know why I need to divide by two each value oO
        rmt_fill_item_level(item, data[i] >> 1, data[i+1] >> 1);
        item++;
    }
}

void rmtlib_raw_send(unsigned int frequency, int data[], int data_len)
{
	vTaskDelay(10);
	raw_tx_init(TX_PIN_SSSS, frequency);
    int item_num = data_len/2;
    size_t item_size = (sizeof(rmt_item32_t) * item_num * RMT_TX_DATA_NUM);
    rmt_item32_t* item = (rmt_item32_t*) malloc(item_size);
    memset((void*) item, 0, item_size);

    rmtlib_raw_build_items(item, data, data_len);

	esp_log_level_set(RAW_TAG, ESP_LOG_INFO);
	ESP_LOGI(RAW_TAG, "RMT TX DATA");
    
	rmt_write_items(RMT_TX_CHANNEL, item, item_num * RMT_TX_DATA_NUM, true);
	rmt_wait_tx_done(RMT_TX_CHANNEL, 150);
	free(item);
	rmt_driver_uninstall(RMT_TX_CHANNEL);
}

#endif