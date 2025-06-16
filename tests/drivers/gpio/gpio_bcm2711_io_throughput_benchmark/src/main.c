#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>
#include <zephyr/timing/timing.h> // For timing_xxx functions
#include <zephyr/sys/printk.h>

// Define a GPIO pin for testing.
#define TEST_PIN_NUM 22 // e.g., GPIO22 on &gpio1
const struct gpio_dt_spec test_gpio = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_22);

#define BENCHMARK_DURATION_MS 1000 // Run each benchmark for 1 second

static void test_gpio_output_toggle_throughput(void)
{
    int ret;
    uint32_t toggle_count = 0;
    timing_t t_start, t_end;
    uint64_t duration_cycles;
    uint64_t duration_ns;

    printk("Starting GPIO output toggle throughput benchmark on pin %d...\n", test_gpio.pin);
    zassert_true(device_is_ready(test_gpio.port), "GPIO device not ready");

    ret = gpio_pin_configure_dt(&test_gpio, GPIO_OUTPUT_LOW);
    zassert_ok(ret, "Failed to configure pin as output");

    // Lock scheduler to prevent preemption during the tight loop
    k_sched_lock();

    t_start = timing_counter_get();

    // Loop for a fixed duration
    // Note: k_uptime_get() inside the loop adds overhead.
    // A better approach is to run for a fixed number of iterations and measure time,
    // or use timing_elapsed_us(&t_start) if precise.
    // For simplicity here, we'll use a fixed number of iterations and measure total time.
    // For a true "for duration" test, one would loop checking k_uptime_get() or similar.
    // Let's refine to fixed iterations for better measurement of raw toggle speed.
    #define TOGGLE_ITERATIONS 1000000

    for (uint32_t i = 0; i < TOGGLE_ITERATIONS; i++) {
        // Using gpio_pin_set_dt twice for a toggle to be comparable to drivers
        // that might not have a native toggle function, or to measure raw set speed.
        // gpio_pin_toggle_dt(&test_gpio) would be more direct if available and desired.
        gpio_pin_set_dt(&test_gpio, 1);
        gpio_pin_set_dt(&test_gpio, 0);
        toggle_count++; // Counts pairs of set, so one full toggle cycle
    }

    t_end = timing_counter_get();
    k_sched_unlock();

    duration_cycles = timing_cycles_get(&t_start, &t_end);
    duration_ns = timing_cycles_to_ns(duration_cycles);

    if (duration_ns > 0) {
        uint64_t toggles_per_sec = ((uint64_t)toggle_count * 1000000000ULL) / duration_ns;
        printk("Achieved %u toggles in %llu ns.\n", toggle_count, duration_ns);
        printk("GPIO output toggle rate: %llu toggles/sec (approx %llu KHz square wave)\n",
               toggles_per_sec, toggles_per_sec / 2000); // Each toggle is half a wave period
    } else {
        printk("Duration was too short to measure accurately.\n");
    }
    zassert_true(duration_ns > 0, "Benchmark duration was zero");

    // Cleanup
    gpio_pin_configure_dt(&test_gpio, GPIO_DISCONNECTED);
}


static void test_gpio_input_read_throughput(void)
{
    int ret;
    volatile int pin_val; // Make volatile to prevent optimization
    uint32_t read_count = 0;
    timing_t t_start, t_end;
    uint64_t duration_cycles;
    uint64_t duration_ns;

    printk("Starting GPIO input read throughput benchmark on pin %d...\n", test_gpio.pin);
    zassert_true(device_is_ready(test_gpio.port), "GPIO device not ready");

    ret = gpio_pin_configure_dt(&test_gpio, GPIO_INPUT); // Pulls don't matter much for read speed itself
    zassert_ok(ret, "Failed to configure pin as input");

    k_sched_lock();
    t_start = timing_counter_get();

    #define READ_ITERATIONS 2000000 // More iterations for faster operation

    for (uint32_t i = 0; i < READ_ITERATIONS; i++) {
        pin_val = gpio_pin_get_dt(&test_gpio);
        read_count++;
    }

    t_end = timing_counter_get();
    k_sched_unlock();

    (void)pin_val; // Suppress unused variable warning

    duration_cycles = timing_cycles_get(&t_start, &t_end);
    duration_ns = timing_cycles_to_ns(duration_cycles);

    if (duration_ns > 0) {
        uint64_t reads_per_sec = ((uint64_t)read_count * 1000000000ULL) / duration_ns;
        printk("Performed %u reads in %llu ns.\n", read_count, duration_ns);
        printk("GPIO input read rate: %llu reads/sec\n", reads_per_sec);
    } else {
        printk("Duration was too short to measure accurately.\n");
    }
    zassert_true(duration_ns > 0, "Benchmark duration was zero");

    // Cleanup
    gpio_pin_configure_dt(&test_gpio, GPIO_DISCONNECTED);
}

static void test_suite_setup_handler(void *data) {
    ARG_UNUSED(data);
    timing_init();
    timing_start(); // Start a common timing session for the suite
}

static void test_suite_teardown_handler(void *data) {
    ARG_UNUSED(data);
    timing_stop();
}

ZTEST_SUITE(gpio_bcm2711_io_throughput, NULL, test_suite_setup_handler, NULL, NULL, test_suite_teardown_handler);

ZTEST(gpio_bcm2711_io_throughput, benchmark_output_toggle)
{
    test_gpio_output_toggle_throughput();
}

ZTEST(gpio_bcm2711_io_throughput, benchmark_input_read)
{
    test_gpio_input_read_throughput();
}
