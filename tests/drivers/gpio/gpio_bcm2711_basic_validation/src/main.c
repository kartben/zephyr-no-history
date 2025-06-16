#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>

// Define GPIO pins to be used for testing.
// These should be chosen carefully to be available on RPi 4B headers
// and ideally suitable for easy loopback (jumpering).
// For example, use GPIO23 and GPIO24. User must jumper these two pins.
#define OUTPUT_PIN_NAME "GPIO_1" // Corresponds to &gpio1 in DT
#define OUTPUT_PIN_NUM  23       // Actual GPIO number (e.g., GPIO23)
#define INPUT_PIN_NAME  "GPIO_1" // Corresponds to &gpio1 in DT
#define INPUT_PIN_NUM   24       // Actual GPIO number (e.g., GPIO24)

const struct gpio_dt_spec output_gpio = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_23); // Example, adjust to actual DT binding
const struct gpio_dt_spec input_gpio = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_24);  // Example, adjust to actual DT binding
// NOTE: The above GPIO_DT_SPEC_GET_BY_NAME might need adjustment based on how gpios are named in bcm2711.dtsi's &gpio1 node
// A simpler way if the exact DT alias isn't known for tests is to get the device and use raw pin numbers:
// const struct device *gpio_dev_out;
// const struct device *gpio_dev_in;

static void test_gpio_output(void)
{
    int ret;
    // gpio_dev_out = device_get_binding(OUTPUT_PIN_NAME);
    // zassert_not_null(gpio_dev_out, "Failed to get output GPIO device");
    zassert_true(device_is_ready(output_gpio.port), "Output GPIO device not ready");

    printk("Testing GPIO %d as output\n", output_gpio.pin);

    ret = gpio_pin_configure_dt(&output_gpio, GPIO_OUTPUT_ACTIVE);
    zassert_ok(ret, "Failed to configure GPIO %d as output", output_gpio.pin);

    ret = gpio_pin_set_dt(&output_gpio, 1);
    zassert_ok(ret, "Failed to set GPIO %d high", output_gpio.pin);
    // Add a small delay for the signal to propagate if doing loopback
    k_msleep(10);
    // Verification for this part often requires external measurement (logic analyzer/multimeter)
    // or a loopback to an input pin.
    printk("Set GPIO %d to HIGH. Verify externally or via loopback.\n", output_gpio.pin);

    ret = gpio_pin_set_dt(&output_gpio, 0);
    zassert_ok(ret, "Failed to set GPIO %d low", output_gpio.pin);
    k_msleep(10);
    printk("Set GPIO %d to LOW. Verify externally or via loopback.\n", output_gpio.pin);
}

static void test_gpio_input_loopback(void)
{
    int ret;
    // gpio_dev_out = device_get_binding(OUTPUT_PIN_NAME);
    // zassert_not_null(gpio_dev_out, "Failed to get output GPIO device");
    // gpio_dev_in = device_get_binding(INPUT_PIN_NAME);
    // zassert_not_null(gpio_dev_in, "Failed to get input GPIO device");
    zassert_true(device_is_ready(output_gpio.port), "Output GPIO device not ready");
    zassert_true(device_is_ready(input_gpio.port), "Input GPIO device not ready");

    printk("Testing GPIO %d (input) with GPIO %d (output) in loopback.\n", input_gpio.pin, output_gpio.pin);
    printk("Ensure GPIO %d and GPIO %d are jumpered externally.\n", input_gpio.pin, output_gpio.pin);

    // Configure output pin
    ret = gpio_pin_configure_dt(&output_gpio, GPIO_OUTPUT_ACTIVE);
    zassert_ok(ret, "Failed to configure GPIO %d as output", output_gpio.pin);

    // Configure input pin (no pull initially)
    ret = gpio_pin_configure_dt(&input_gpio, GPIO_INPUT);
    zassert_ok(ret, "Failed to configure GPIO %d as input", input_gpio.pin);

    // Test: Output LOW, Input should be LOW
    ret = gpio_pin_set_dt(&output_gpio, 0);
    zassert_ok(ret, "Failed to set output pin low");
    k_msleep(10); // Allow time for signal to settle
    int val = gpio_pin_get_dt(&input_gpio);
    zassert_equal(val, 0, "Input pin did not read LOW when output was LOW (val=%d)", val);

    // Test: Output HIGH, Input should be HIGH
    ret = gpio_pin_set_dt(&output_gpio, 1);
    zassert_ok(ret, "Failed to set output pin high");
    k_msleep(10); // Allow time for signal to settle
    val = gpio_pin_get_dt(&input_gpio);
    zassert_equal(val, 1, "Input pin did not read HIGH when output was HIGH (val=%d)", val);

    // Test: Input with Pull-up (output pin should be high impedance or also high to not interfere)
    ret = gpio_pin_set_dt(&output_gpio, 1); // Set output high to not interfere with pull-up
    zassert_ok(ret, "Failed to set output pin high for pull-up test");
    ret = gpio_pin_configure_dt(&input_gpio, GPIO_INPUT | GPIO_PULL_UP);
    zassert_ok(ret, "Failed to configure input pin with pull-up");
    k_msleep(10);
    val = gpio_pin_get_dt(&input_gpio);
    zassert_equal(val, 1, "Input pin with pull-up did not read HIGH (val=%d)", val);

    // Test: Input with Pull-down (output pin should be high impedance or also low to not interfere)
    ret = gpio_pin_set_dt(&output_gpio, 0); // Set output low to not interfere with pull-down
    zassert_ok(ret, "Failed to set output pin low for pull-down test");
    ret = gpio_pin_configure_dt(&input_gpio, GPIO_INPUT | GPIO_PULL_DOWN);
    zassert_ok(ret, "Failed to configure input pin with pull-down");
    k_msleep(10);
    val = gpio_pin_get_dt(&input_gpio);
    zassert_equal(val, 0, "Input pin with pull-down did not read LOW (val=%d)", val);

    // Cleanup: set output low and input to disconnected
    gpio_pin_set_dt(&output_gpio, 0);
    gpio_pin_configure_dt(&input_gpio, GPIO_DISCONNECTED);

}

ZTEST_SUITE(gpio_bcm2711_basic_validation, NULL, NULL, NULL, NULL, NULL);

ZTEST(gpio_bcm2711_basic_validation, test_output_pin)
{
    test_gpio_output();
}

ZTEST(gpio_bcm2711_basic_validation, test_input_pin_loopback)
{
    test_gpio_input_loopback();
}
