#ifndef SW_PWM_H
#define SW_PWM_H

#include <stdbool.h>

typedef int sw_pwm_led;

typedef struct {
    sw_pwm_led led; // DK_LED1-DK_LED4
    int pulse; // in ms, the time the led is on
    int period; // in ms
} sw_pwm_spec;

/**
 * @brief initializes software PWM
 * @return true on success
 * @return false on failure
 */
bool sw_pwm_init();

/**
 * @brief enables PWM for the specified led.
 * @param led a valid led, i.e. `DK_LED1` - `DK_LED4`
 * @param pulse the time the led is on, in ms
 * @param period total period of time, in ms
 * @return true on success
 * @return false on failure
 */
bool sw_pwm_set(sw_pwm_led led, int pulse, int period);

/**
 * @brief enables PWM for the specified led.
 * @param spec a valid spec
 * @return true on success
 * @return false on failure
 */
bool sw_pwm_spec_set(const sw_pwm_spec *spec);

/**
 * @brief returns sw_pwm_spec for the specified led.
 * If PWM is not active, returns an invalid spec.
 * @param led a valid led, i.e. `DK_LED1` - `DK_LED4`
 * @return sw_pwm_spec 
 */
sw_pwm_spec sw_pwm_get(sw_pwm_led led);

/**
 * @brief disables PWM for the specified led.
 * The same as `sw_pwm_disable_wait(led, false)`
 * @param led a valid led, i.e. `DK_LED1` - `DK_LED4`
 * @return true on success
 * @return false on failure
 */
bool sw_pwm_disable(sw_pwm_led led);

/**
 * @brief disables PWM for the specified led
 * @param led a valid led, i.e. `DK_LED1` - `DK_LED4`
 * @param wait if true, suspends the current thread until PWM is disabled
 * @return true on success
 * @return false on failure
 */
bool sw_pwm_disable_wait(sw_pwm_led led, bool wait);

/**
 * @brief checks if the spec is valid
 * @param spec 
 * @return true if valid
 * @return false if invalid
 */
bool sw_pwm_spec_is_valid(const sw_pwm_spec *spec);

#endif // SW_PWM_H
