#include "sw_pwm.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(sw_pwm);

#define SW_PWM_THREAD_STACK_SIZE 500
#define SW_PWM_THREAD_PRIORITY 5

// the first argument is sw_pwm_details*
static void sw_pwm_thread(void *, void *, void *);

#define MAX_LEDS 4
#define LED_INVALID -1

typedef enum {
    SW_PWM_STATE_OFF,
    SW_PWM_STATE_ON
} sw_pwm_state;

typedef struct {
    sw_pwm_spec spec;
    sw_pwm_state state;

    K_THREAD_STACK_MEMBER(thread_stack_area, SW_PWM_THREAD_STACK_SIZE);
    struct k_thread thread_data;
    k_tid_t thread_id;

    struct k_mutex spec_mutex;

    bool is_running;
} sw_pwm_details;

static sw_pwm_details g_leds[MAX_LEDS];

static bool is_led_valid(sw_pwm_led led) {
    switch (led) {
        case DK_LED1:
        case DK_LED2:
        case DK_LED3:
        case DK_LED4:
            return true;
        default:
            return false;
    }
}

static void details_reset(sw_pwm_details *details) {
    details->spec.led = LED_INVALID;
    details->thread_id = NULL;
    details->is_running = false;
}

static void details_init(sw_pwm_details *details) {
    details_reset(details);
    k_mutex_init(&details->spec_mutex);
}

static void details_lock_spec_mutex(sw_pwm_details *details) {
    k_mutex_lock(&details->spec_mutex, K_FOREVER);
}

static void details_unlock_spec_mutex(sw_pwm_details *details) {
    k_mutex_unlock(&details->spec_mutex);
}

static bool is_details_valid(const sw_pwm_details *details) {
    return details->thread_id != NULL;
}

static sw_pwm_details *get_details_for(sw_pwm_led led) {
    for (int i = 0; i < MAX_LEDS; ++i) {
        if (g_leds[i].spec.led == led) {
            return &g_leds[i];
        }
    }

    return NULL;
}

static void sw_pwm_start_thread(sw_pwm_details *details) {
    details->thread_id = k_thread_create(&details->thread_data,
        details->thread_stack_area,
        K_THREAD_STACK_SIZEOF(details->thread_stack_area),
        sw_pwm_thread,
        details, NULL, NULL,
        SW_PWM_THREAD_PRIORITY, 0, K_NO_WAIT);
}

static bool sw_pwm_start(const sw_pwm_spec *spec) {
    sw_pwm_details *details = get_details_for(spec->led);

    // update spec
    if (details) {
        details_lock_spec_mutex(details);
        details->spec = *spec;
        details_unlock_spec_mutex(details);

        LOG_INF("Spec for LED %i was updated", spec->led);

        return true;
    }

    // otherwise enable PWM for the specified led
    for (int i = 0; i < MAX_LEDS; ++i) {
        details = &g_leds[i];

        if (!is_details_valid(details)) {
            details->spec = *spec;
            details->state = SW_PWM_STATE_ON;
            details->is_running = true;

            sw_pwm_start_thread(details);

            LOG_INF("PWM was enabled for LED %i", spec->led);

            return true;
        }
    }

    LOG_ERR("PWM cannot be enabled for LED %i: out of slots", spec->led);

    return false;
}

bool sw_pwm_init() {
    LOG_INF("init");

    for (int i = 0; i < MAX_LEDS; ++i)
        details_init(&g_leds[i]);
    
    return true;
}

bool sw_pwm_set(sw_pwm_led led, int pulse, int period) {
    sw_pwm_spec spec = {
        .led = led,
        .pulse = pulse,
        .period = period
    };
    
    return sw_pwm_spec_set(&spec);
}

bool sw_pwm_spec_set(const sw_pwm_spec *spec) {
    if (!sw_pwm_spec_is_valid(spec)) {
        LOG_ERR("Invalid spec");
        return false;
    }

    return sw_pwm_start(spec);
}

sw_pwm_spec sw_pwm_get(sw_pwm_led led) {
    sw_pwm_details *details = get_details_for(led);

    if (!details)
        return (sw_pwm_spec) {.led = LED_INVALID};

    details_lock_spec_mutex(details);
    sw_pwm_spec spec = details->spec;
    details_unlock_spec_mutex(details);
    
    return spec;
}

bool sw_pwm_disable(sw_pwm_led led) {
    return sw_pwm_disable_wait(led, false);
}

bool sw_pwm_disable_wait(sw_pwm_led led, bool wait) {
    sw_pwm_details *details = get_details_for(led);

    if (details) {
        sw_pwm_led led = details->spec.led;
        k_tid_t thread_id = details->thread_id;
        details->is_running = false;

        if (wait) {
            k_thread_join(thread_id, K_MSEC(0));
            LOG_INF("PWM for LED %i was disabled", led);
        } else {
            LOG_INF("PWM for LED %i will be disabled", led);
        }
    }

    return details;
}

bool sw_pwm_spec_is_valid(const sw_pwm_spec *spec) {
    return is_led_valid(spec->led) &&
           spec->period > 0 &&
           spec->pulse >= 0 &&
           spec->period >= spec->pulse;
}

void sw_pwm_thread(void *p1, void *p2, void *p3) {
    int64_t now;
    sw_pwm_details *details = p1;

    while (details->is_running) {
        now = k_uptime_get();

        details_lock_spec_mutex(details);

        int curr_pulse_width = details->spec.pulse;

        if (details->state == SW_PWM_STATE_OFF)
            curr_pulse_width = details->spec.period - curr_pulse_width;

        if (dk_set_led(details->spec.led, details->state) != 0) {
            LOG_ERR("State of the LED %i cannot be changed", details->spec.led);
        }

        details_unlock_spec_mutex(details);

        k_msleep(curr_pulse_width);

        // change state
        details->state = !details->state;
    }

    details_reset(details);
}
