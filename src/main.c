#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dk_buttons_and_leds.h>

#include "sw_pwm.h"

LOG_MODULE_REGISTER(main);

#define BLINKING_THREAD_STACK_SIZE 500
#define BLINKING_THREAD_PRIORITY 5

#define CTRL_LED DK_LED2
#define CTRL_LED_STEP 2
#define CTRL_LED_PERIOD 10

typedef struct {
    sw_pwm_led led;
    int period;
    int update_delay;

    K_THREAD_STACK_MEMBER(thread_stack_area, BLINKING_THREAD_STACK_SIZE);
    struct k_thread thread_data;
    k_tid_t thread_id;
} blinking_led_spec;

static void blinking_thread(void *, void *, void *);

static void start_blinking_thread(blinking_led_spec *spec) {
    spec->thread_id = k_thread_create(&spec->thread_data,
        spec->thread_stack_area,
        K_THREAD_STACK_SIZEOF(spec->thread_stack_area),
        blinking_thread,
        spec, NULL, NULL,
        BLINKING_THREAD_PRIORITY, 0, K_NO_WAIT);
}

// <CTRL_LED functions>

static void change_brightness(int delta) {
    sw_pwm_spec spec = sw_pwm_get(CTRL_LED);
    spec.pulse = MIN(MAX(spec.pulse + delta, 0), spec.period);
    sw_pwm_spec_set(&spec);
}

static void btn_handler(uint32_t button_state, uint32_t has_changed) {
    #define is_btn_pressed(__btn_mask) \
        (has_changed & __btn_mask && button_state & __btn_mask)
    
    if (is_btn_pressed(DK_BTN1_MSK))
        change_brightness(CTRL_LED_STEP);

    if (is_btn_pressed(DK_BTN2_MSK))
        change_brightness(-CTRL_LED_STEP);

    if (is_btn_pressed(DK_BTN3_MSK)) {
        sw_pwm_disable_wait(CTRL_LED, true);
        dk_set_led_off(CTRL_LED);
    }

    if (is_btn_pressed(DK_BTN4_MSK))
        sw_pwm_set(CTRL_LED, 1, CTRL_LED_PERIOD);

    #undef is_btn_pressed
}

// </CTRL_LED functions>

static blinking_led_spec g_blinking_spec1 = {
    .led = DK_LED1,
    .period = 5,
    .update_delay = 25
};

static blinking_led_spec g_blinking_spec2 = {
    .led = DK_LED4,
    .period = 8,
    .update_delay = 50
};

int main() {
    if (dk_leds_init() != 0) {
        LOG_ERR("LEDs initialization failed");
        return -1;
    }

    if (dk_buttons_init(btn_handler) != 0) {
        LOG_ERR("Buttons initialization failed");
        return -1;
    }

    sw_pwm_init();
    sw_pwm_set(CTRL_LED, 1, CTRL_LED_PERIOD);
    
    start_blinking_thread(&g_blinking_spec1);
    start_blinking_thread(&g_blinking_spec2);

    return 0;
}

void blinking_thread(void *p1, void *p2, void *p3) {
    blinking_led_spec *spec = p1;
    int counter = 0;

    while (1) {
        ++counter;

        int x = counter % (spec->period * 2);
        int pulse = MIN(2 * spec->period - x, x);
        
        sw_pwm_set(spec->led, pulse, spec->period);
        k_msleep(spec->update_delay);
    }
}
