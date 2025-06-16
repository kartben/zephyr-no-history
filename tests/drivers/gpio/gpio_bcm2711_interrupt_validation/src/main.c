#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>
#include <zephyr/sys/util.h> // For BIT macro

// Define GPIO pins to be used for testing.
// User must jumper these two pins externally.
#define OUTPUT_PIN_NUM  23 // Actual GPIO number (e.g., GPIO23 on gpio1)
#define INPUT_PIN_NUM   24 // Actual GPIO number (e.g., GPIO24 on gpio1)

// GPIO DT Specs - these need to map to actual DT definitions for rpi_4b
// For simplicity in this example, we'll assume &gpio1 is the node label
// and gpios_23, gpios_24 are the names within that node. This MUST be verified.
const struct gpio_dt_spec output_gpio = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_23);
const struct gpio_dt_spec input_gpio  = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_24);

static struct gpio_callback int_callback_data;
static volatile uint32_t callback_count = 0;
static volatile gpio_port_pins_t last_triggered_pins = 0;

// Generic callback function
static void gpio_test_callback(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    // It's good practice to do minimal work in a callback
    // For testing, we'll record the pin and increment a counter
    if (pins & BIT(input_gpio.pin)) { // Check if it's our target pin
        callback_count++;
        last_triggered_pins = pins;
    }
    printk("Callback executed for pins: 0x%x, target pin %d callback_count: %u\n", pins, input_gpio.pin, callback_count);
}

// Helper to setup output and input pins
static void setup_pins(void)
{
    int ret;
    zassert_true(device_is_ready(output_gpio.port), "Output GPIO device not ready");
    zassert_true(device_is_ready(input_gpio.port), "Input GPIO device not ready");

    ret = gpio_pin_configure_dt(&output_gpio, GPIO_OUTPUT_INACTIVE); // Start output low
    zassert_ok(ret, "Failed to configure output pin");

    // Input pin initially configured as plain input, specific interrupt config will follow
    ret = gpio_pin_configure_dt(&input_gpio, GPIO_INPUT);
    zassert_ok(ret, "Failed to configure input pin");

    // Initialize and add callback
    gpio_init_callback(&int_callback_data, gpio_test_callback, BIT(input_gpio.pin));
    ret = gpio_add_callback(input_gpio.port, &int_callback_data);
    zassert_ok(ret, "Failed to add GPIO callback");
}

static void reset_callback_state(void)
{
    callback_count = 0;
    last_triggered_pins = 0;
}

static void teardown_pins(void)
{
    gpio_remove_callback(input_gpio.port, &int_callback_data);
    gpio_pin_configure_dt(&input_gpio, GPIO_DISCONNECTED);
    gpio_pin_configure_dt(&output_gpio, GPIO_DISCONNECTED);
}


static void test_interrupt_edge_rising(void)
{
    printk("Testing EDGE_RISING interrupt on GPIO %d (triggered by GPIO %d)\n", input_gpio.pin, output_gpio.pin);
    reset_callback_state();

    int ret = gpio_pin_interrupt_configure_dt(&input_gpio, GPIO_INT_EDGE_RISING);
    zassert_ok(ret, "Failed to configure edge rising interrupt");

    // Trigger: LOW -> HIGH
    gpio_pin_set_dt(&output_gpio, 0);
    k_busy_wait(1000); // Ensure output is low (microseconds)
    gpio_pin_set_dt(&output_gpio, 1);
    k_busy_wait(1000); // Allow time for interrupt processing

    zassert_equal(callback_count, 1, "Callback count mismatch for rising edge (expected 1, got %u)", callback_count);
    zassert_true((last_triggered_pins & BIT(input_gpio.pin)), "Triggered pin mismatch");

    // Ensure no trigger on LOW or HIGH->LOW
    reset_callback_state();
    gpio_pin_set_dt(&output_gpio, 0); // H -> L
    k_busy_wait(1000);
    gpio_pin_set_dt(&output_gpio, 0); // L -> L
    k_busy_wait(1000);
    zassert_equal(callback_count, 0, "Callback triggered unexpectedly (count: %u)", callback_count);
}

static void test_interrupt_edge_falling(void)
{
    printk("Testing EDGE_FALLING interrupt on GPIO %d (triggered by GPIO %d)\n", input_gpio.pin, output_gpio.pin);
    reset_callback_state();

    int ret = gpio_pin_interrupt_configure_dt(&input_gpio, GPIO_INT_EDGE_FALLING);
    zassert_ok(ret, "Failed to configure edge falling interrupt");

    // Ensure initial state is high before falling edge
    gpio_pin_set_dt(&output_gpio, 1);
    k_busy_wait(1000);

    // Trigger: HIGH -> LOW
    gpio_pin_set_dt(&output_gpio, 0);
    k_busy_wait(1000);

    zassert_equal(callback_count, 1, "Callback count mismatch for falling edge (expected 1, got %u)", callback_count);
    zassert_true((last_triggered_pins & BIT(input_gpio.pin)), "Triggered pin mismatch");

    // Ensure no trigger on HIGH or LOW->HIGH
    reset_callback_state();
    gpio_pin_set_dt(&output_gpio, 1); // L -> H
    k_busy_wait(1000);
    gpio_pin_set_dt(&output_gpio, 1); // H -> H
    k_busy_wait(1000);
    zassert_equal(callback_count, 0, "Callback triggered unexpectedly (count: %u)", callback_count);
}

static void test_interrupt_edge_both(void)
{
    printk("Testing EDGE_BOTH interrupt on GPIO %d (triggered by GPIO %d)\n", input_gpio.pin, output_gpio.pin);
    reset_callback_state();

    int ret = gpio_pin_interrupt_configure_dt(&input_gpio, GPIO_INT_EDGE_BOTH);
    zassert_ok(ret, "Failed to configure edge both interrupt");

    // Trigger: LOW -> HIGH
    gpio_pin_set_dt(&output_gpio, 0);
    k_busy_wait(1000);
    gpio_pin_set_dt(&output_gpio, 1);
    k_busy_wait(1000);
    zassert_equal(callback_count, 1, "Callback count mismatch for rising edge (expected 1, got %u)", callback_count);
    zassert_true((last_triggered_pins & BIT(input_gpio.pin)), "Triggered pin mismatch on rising");

    reset_callback_state();
    // Trigger: HIGH -> LOW
    gpio_pin_set_dt(&output_gpio, 0);
    k_busy_wait(1000);
    zassert_equal(callback_count, 1, "Callback count mismatch for falling edge (expected 1, got %u)", callback_count);
    zassert_true((last_triggered_pins & BIT(input_gpio.pin)), "Triggered pin mismatch on falling");
}

// Level interrupts are trickier to test without potential multiple triggers
// The busy wait might not be enough, a short sleep might be better
// but for level, the interrupt will keep firing as long as the level is active.
// The BCM2711 hardware might automatically convert level to edge for GPEDS,
// or the Zephyr API abstracts this. This needs checking against BCM2711 datasheet.
// For now, we assume GPEDS reflects event detection.
static void test_interrupt_level_high(void)
{
    printk("Testing LEVEL_HIGH interrupt on GPIO %d (triggered by GPIO %d)\n", input_gpio.pin, output_gpio.pin);
    reset_callback_state();

    // Pull-down to ensure it's low before triggering high level
    int ret = gpio_pin_configure_dt(&input_gpio, GPIO_INPUT | GPIO_PULL_DOWN);
    zassert_ok(ret, "Failed to configure input pin with pull-down for level high test");
    ret = gpio_pin_interrupt_configure_dt(&input_gpio, GPIO_INT_LEVEL_HIGH);
    zassert_ok(ret, "Failed to configure level high interrupt");

    gpio_pin_set_dt(&output_gpio, 0); // Ensure low
    k_msleep(1);

    reset_callback_state(); // Reset after any spurious triggers during setup

    gpio_pin_set_dt(&output_gpio, 1); // Set HIGH
    k_msleep(5); // Wait for interrupt, allow potential re-trigger if truly level

    // For BCM2711, GPEDS is edge sensitive. Level interrupts are emulated by continuously checking GPLEV
    // if GPHEN/GPLEN are set. The Zephyr API for BCM2711 seems to use GPEDS and clears it.
    // So, we might only get one callback if the level is held.
    // If the driver truly supports level and re-fires, callback_count might be > 1.
    // This test assumes the Zephyr API provides a single callback event per configuration.
    zassert_true(callback_count >= 1, "Callback not triggered for level high (count: %u)", callback_count);
    zassert_true((last_triggered_pins & BIT(input_gpio.pin)), "Triggered pin mismatch");

    // Clear interrupt by setting output low
    gpio_pin_set_dt(&output_gpio, 0);
    k_msleep(5);
    // Further callbacks should ideally not occur if the driver handles level correctly
    // by not re-firing after the condition is cleared.
}

// Similar caveats for LEVEL_LOW as for LEVEL_HIGH
static void test_interrupt_level_low(void)
{
    printk("Testing LEVEL_LOW interrupt on GPIO %d (triggered by GPIO %d)\n", input_gpio.pin, output_gpio.pin);
    reset_callback_state();

    // Pull-up to ensure it's high before triggering low level
    int ret = gpio_pin_configure_dt(&input_gpio, GPIO_INPUT | GPIO_PULL_UP);
    zassert_ok(ret, "Failed to configure input pin with pull-up for level low test");
    ret = gpio_pin_interrupt_configure_dt(&input_gpio, GPIO_INT_LEVEL_LOW);
    zassert_ok(ret, "Failed to configure level low interrupt");

    gpio_pin_set_dt(&output_gpio, 1); // Ensure high
    k_msleep(1);

    reset_callback_state(); // Reset after any spurious triggers during setup

    gpio_pin_set_dt(&output_gpio, 0); // Set LOW
    k_msleep(5);

    zassert_true(callback_count >= 1, "Callback not triggered for level low (count: %u)", callback_count);
    zassert_true((last_triggered_pins & BIT(input_gpio.pin)), "Triggered pin mismatch");

    // Clear interrupt by setting output high
    gpio_pin_set_dt(&output_gpio, 1);
    k_msleep(5);
}


ZTEST_SUITE(gpio_bcm2711_interrupt_validation, NULL, setup_pins, NULL, NULL, teardown_pins);

ZTEST(gpio_bcm2711_interrupt_validation, test_edge_rising)
{
    test_interrupt_edge_rising();
}

ZTEST(gpio_bcm2711_interrupt_validation, test_edge_falling)
{
    test_interrupt_edge_falling();
}

ZTEST(gpio_bcm2711_interrupt_validation, test_edge_both)
{
    test_interrupt_edge_both();
}

ZTEST(gpio_bcm2711_interrupt_validation, test_level_high)
{
    test_interrupt_level_high();
}

ZTEST(gpio_bcm2711_interrupt_validation, test_level_low)
{
    test_interrupt_level_low();
}
