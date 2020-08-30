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
#define SONY_BITS              	12
#define SONY_HEADER_HIGH_US    	2400
#define SONY_HEADER_LOW_US     	600
#define SONY_BIT_ONE_HIGH_US    1200
#define SONY_BIT_ONE_LOW_US     600
#define SONY_BIT_ZERO_HIGH_US   600
#define SONY_BIT_ZERO_LOW_US    600
//
#define SONY_DATA_ITEM_NUM   	13  /*!< code item number: 3 bits header + 12bit data */
#define SONY_BIT_MARGIN			60  /* deviation from signal timing */

const char* SONY_TAG = "SONY";

/*
 * SEND
 */

#if SEND_SONY

void sony_fill_item_header(rmt_item32_t* item)
{
    rmt_fill_item_level(item, SONY_HEADER_HIGH_US, SONY_HEADER_LOW_US);
}

void sony_fill_item_bit_one(rmt_item32_t* item)
{
    rmt_fill_item_level(item, SONY_BIT_ONE_HIGH_US, SONY_BIT_ONE_LOW_US);
}

void sony_fill_item_bit_zero(rmt_item32_t* item)
{
    rmt_fill_item_level(item, SONY_BIT_ZERO_HIGH_US, SONY_BIT_ZERO_LOW_US);
}

void sony_build_items(rmt_item32_t* item, uint32_t cmd_data)
{
  sony_fill_item_header(item++);
  
  // parse from left to right 32 bits (0x80000000)
  uint32_t mask = 0x01;
  mask <<= SONY_BITS - 1;

  for (int j = 0; j < SONY_BITS; j++) {
    if (cmd_data & mask) {
      sony_fill_item_bit_one(item);
    } else {
      sony_fill_item_bit_zero(item);
    }
    item++;
    mask >>= 1;
  }
}

void sony_tx_init(gpio_num_t gpio_num)
{
	rmt_config_t rmt_tx;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = gpio_num;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = RMT_CARRIER_DUTY;
    rmt_tx.tx_config.carrier_freq_hz = RMT_CARRIER_FREQ_38;
    rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
    rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = RMT_MODE_TX;
	
	rmt_tx_init(&rmt_tx);
}

void rmtlib_sony_send(unsigned long data)
{
	vTaskDelay(10);
	sony_tx_init(TX_PIN_SSSS);
	
	esp_log_level_set(SONY_TAG, ESP_LOG_INFO);
	ESP_LOGI(SONY_TAG, "RMT TX DATA");
	
	size_t size = (sizeof(rmt_item32_t) * SONY_DATA_ITEM_NUM * RMT_TX_DATA_NUM);
	rmt_item32_t* item = (rmt_item32_t*) malloc(size);
	memset((void*) item, 0, size);
	
	sony_build_items(item, data);

	int item_num = SONY_DATA_ITEM_NUM * RMT_TX_DATA_NUM;
	rmt_write_items(RMT_TX_CHANNEL, item, item_num, true);
	rmt_wait_tx_done(RMT_TX_CHANNEL, 150);
	free(item);
	rmt_driver_uninstall(RMT_TX_CHANNEL);
}

#endif