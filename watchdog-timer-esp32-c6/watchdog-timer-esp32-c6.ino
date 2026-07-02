// reference: https://wiki.dfrobot.com/dfr0886/docs/23048 and ChatGPT

#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_task_wdt.h>

#define WDT_TIMEOUT 2 // time in seconds for the watchdog to activate
#define LED_GPIO GPIO_NUM_1
#define BUTTON_GPIO GPIO_NUM_18
gptimer_handle_t timer = NULL;
volatile bool finalCount = false;
volatile bool buttonPressed = false;

void setupGPIO()
{
    gpio_config_t io_conf = {};

    // Configure LED as output (pin 1 (D1), without pull up or pull down)
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(LED_GPIO, 0);

    // Configure button as input (pin 18 (D10) with pull up only)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
}
bool IRAM_ATTR timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    static bool state = false;
    if(buttonPressed == false && finalCount == false){
        state = !state;
        gpio_set_level(LED_GPIO, state);        
    }else{
        state = true;
        gpio_set_level(LED_GPIO, state);
        finalCount= true;
    }
    
    return false;
}
void setupTimer()
{
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 100000,      // 100 kHz
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&config, &timer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_callback,
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &cbs, NULL));

    gptimer_alarm_config_t alarm = {
        .alarm_count = 20000,         // 200 ms, since resolution_hz above is 100000 Hz, giving 10us per tick. Then 20000 * 10us = 200ms
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = true,
        },
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &alarm));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));
}

void setup() {
  // put your setup code here, to run once:
    setupGPIO();
    setupTimer();
    esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
};

    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdt_config));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
   
}

void loop() {
    // when push button on pin 18 (D10 of Xiao ESP32-C6) is pressed
    if (gpio_get_level(BUTTON_GPIO) == 0)
    {
        buttonPressed= true; // this makes the LED stop blinking and stay solid
    }
    // finalCount goes to true when the push button is pressed, effectively
    // preventing the watchdog of being reset periodically, so after some time 
    // it activates, resetting the microcontroller    
    if(finalCount == false){ 
        esp_task_wdt_reset(); // Reset watchdog timer
    }
}
