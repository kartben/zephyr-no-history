#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>
#include <zephyr/timing/timing.h>
#include <zephyr/sys/printk.h>
#include <stdio.h> // For snprintf if used for float formatting

// Define a GPIO pin for testing.
#define TEST_PIN_NUM 22 // e.g., GPIO22 on &gpio1
const struct gpio_dt_spec test_gpio = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_22);

#define NUM_BENCHMARK_SAMPLES 1000 // Number of times to run each configuration

// Helper function to benchmark a specific gpio_pin_configure_dt call
static void benchmark_pin_configure_flags(gpio_flags_t flags, const char *description)
{
    uint64_t total_cycles = 0;
    timing_t ts_start, ts_end;

    zassert_true(device_is_ready(test_gpio.port), "GPIO device not ready for %s", description);

    // Warm-up call (optional, but can help stabilize cache/branch prediction)
    gpio_pin_configure_dt(&test_gpio, flags);

    for (int i = 0; i < NUM_BENCHMARK_SAMPLES; i++) {
        ts_start = timing_counter_get();
        gpio_pin_configure_dt(&test_gpio, flags);
        ts_end = timing_counter_get();
        total_cycles += timing_cycles_get(&ts_start, &ts_end);
    }
    uint64_t avg_cycles = total_cycles / NUM_BENCHMARK_SAMPLES;
    printk("Average time for gpio_pin_configure_dt (%s): %llu cycles (%llu ns)\n",
           description, avg_cycles, timing_cycles_to_ns(avg_cycles));
}

// Helper function to benchmark a specific gpio_pin_interrupt_configure_dt call
static void benchmark_interrupt_configure_flags(enum gpio_int_mode mode, enum gpio_int_trig trig, const char *description)
{
    uint64_t total_cycles = 0;
    timing_t ts_start, ts_end;

    zassert_true(device_is_ready(test_gpio.port), "GPIO device not ready for %s", description);

    // Ensure pin is configured as input for interrupt configuration to be meaningful
    gpio_pin_configure_dt(&test_gpio, GPIO_INPUT);

    // Warm-up call
    gpio_pin_interrupt_configure_dt(&test_gpio, mode | trig);

    for (int i = 0; i < NUM_BENCHMARK_SAMPLES; i++) {
        ts_start = timing_counter_get();
        gpio_pin_interrupt_configure_dt(&test_gpio, mode | trig);
        ts_end = timing_counter_get();
        total_cycles += timing_cycles_get(&ts_start, &ts_end);
    }
    uint64_t avg_cycles = total_cycles / NUM_BENCHMARK_SAMPLES;
    printk("Average time for gpio_pin_interrupt_configure_dt (%s): %llu cycles (%llu ns)\n",
           description, avg_cycles, timing_cycles_to_ns(avg_cycles));
}


static void test_gpio_config_overhead_benchmark(void)
{
    printk("Starting GPIO configuration overhead benchmark...\n");
    timing_init();
    timing_start();

    // Benchmark gpio_pin_configure_dt with various flags
    benchmark_pin_configure_flags(GPIO_INPUT, "GPIO_INPUT");
    benchmark_pin_configure_flags(GPIO_OUTPUT_LOW, "GPIO_OUTPUT_LOW");
    benchmark_pin_configure_flags(GPIO_OUTPUT_HIGH, "GPIO_OUTPUT_HIGH");
    benchmark_pin_configure_flags(GPIO_INPUT | GPIO_PULL_UP, "GPIO_INPUT | GPIO_PULL_UP");
    benchmark_pin_configure_flags(GPIO_INPUT | GPIO_PULL_DOWN, "GPIO_INPUT | GPIO_PULL_DOWN");
    // Add GPIO_OUTPUT_ACTIVE, GPIO_OUTPUT_INACTIVE if desired

    // Benchmark gpio_pin_interrupt_configure_dt
    // Ensure pin is input before these
    gpio_pin_configure_dt(&test_gpio, GPIO_INPUT);

    benchmark_interrupt_configure_flags(GPIO_INT_MODE_DISABLED, 0, "INT_MODE_DISABLED");
    benchmark_interrupt_configure_flags(GPIO_INT_MODE_EDGE, GPIO_INT_TRIG_RISING, "INT_EDGE_RISING");
    benchmark_interrupt_configure_flags(GPIO_INT_MODE_EDGE, GPIO_INT_TRIG_FALLING, "INT_EDGE_FALLING");
    benchmark_interrupt_configure_flags(GPIO_INT_MODE_EDGE, GPIO_INT_TRIG_BOTH, "INT_EDGE_BOTH");
    benchmark_interrupt_configure_flags(GPIO_INT_MODE_LEVEL, GPIO_INT_TRIG_HIGH, "INT_LEVEL_HIGH");
    benchmark_interrupt_configure_flags(GPIO_INT_MODE_LEVEL, GPIO_INT_TRIG_LOW, "INT_LEVEL_LOW");

    // Disable interrupt at the end
    gpio_pin_interrupt_configure_dt(&test_gpio, GPIO_INT_MODE_DISABLED);
    // Disconnect pin
    gpio_pin_configure_dt(&test_gpio, GPIO_DISCONNECTED);

    timing_stop();
    printk("GPIO configuration overhead benchmark finished.\n");
}

ZTEST_SUITE(gpio_bcm2711_config_overhead, NULL, NULL, NULL, NULL, NULL);

ZTEST(gpio_bcm2711_config_overhead, benchmark_config_overhead)
{
    test_gpio_config_overhead_benchmark();
}
