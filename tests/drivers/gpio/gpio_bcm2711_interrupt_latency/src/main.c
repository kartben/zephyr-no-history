#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>
#include <zephyr/timing/timing.h> // For timing_t, timing_counter_t, etc.
#include <zephyr/sys/util.h>     // For BIT()

// Define GPIO pins for loopback. User must jumper these externally.
// Ensure these are on the same GPIO port if possible, though not strictly necessary.
#define TRIGGER_PIN_NUM 23 // e.g., GPIO23 on &gpio1
#define ECHO_PIN_NUM    24 // e.g., GPIO24 on &gpio1

const struct gpio_dt_spec trigger_gpio = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_23);
const struct gpio_dt_spec echo_gpio    = GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_24);

static struct gpio_callback echo_callback_data;
static K_SEM_DEFINE(isr_occurred_sem, 0, 1); // Semaphore to signal ISR execution

// Variables to store timing data
static timing_t ts_start;
static timing_t ts_end;
static uint64_t total_latency_cycles = 0;
static uint32_t num_samples = 0;
#define NUM_LATENCY_SAMPLES 100

// Callback for the echo pin interrupt
static void echo_pin_callback(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    if (pins & BIT(echo_gpio.pin)) {
        ts_end = timing_counter_get();
        k_sem_give(&isr_occurred_sem);
    }
}

static void setup_latency_test_pins(void)
{
    int ret;
    zassert_true(device_is_ready(trigger_gpio.port), "Trigger GPIO device not ready");
    zassert_true(device_is_ready(echo_gpio.port), "Echo GPIO device not ready");

    // Configure trigger pin as output, initially low
    ret = gpio_pin_configure_dt(&trigger_gpio, GPIO_OUTPUT_LOW);
    zassert_ok(ret, "Failed to configure trigger_gpio");

    // Configure echo pin as input with rising edge interrupt
    ret = gpio_pin_configure_dt(&echo_gpio, GPIO_INPUT); // No pull, rely on trigger
    zassert_ok(ret, "Failed to configure echo_gpio as input");

    ret = gpio_pin_interrupt_configure_dt(&echo_gpio, GPIO_INT_EDGE_RISING);
    zassert_ok(ret, "Failed to configure interrupt on echo_gpio");

    gpio_init_callback(&echo_callback_data, echo_pin_callback, BIT(echo_gpio.pin));
    ret = gpio_add_callback(echo_gpio.port, &echo_callback_data);
    zassert_ok(ret, "Failed to add GPIO callback for echo_gpio");

    k_sem_reset(&isr_occurred_sem);
    total_latency_cycles = 0;
    num_samples = 0;
}

static void teardown_latency_test_pins(void)
{
    gpio_remove_callback(echo_gpio.port, &echo_callback_data);
    gpio_pin_interrupt_configure_dt(&echo_gpio, GPIO_INT_MODE_DISABLED);
    gpio_pin_configure_dt(&echo_gpio, GPIO_DISCONNECTED);
    gpio_pin_configure_dt(&trigger_gpio, GPIO_DISCONNECTED);
}


static void test_gpio_interrupt_latency(void)
{
    printk("Starting GPIO interrupt latency test...\n");
    printk("Ensure GPIO %d (trigger) and GPIO %d (echo) are jumpered externally.\n", trigger_gpio.pin, echo_gpio.pin);

    timing_init();
    timing_start(); // Start timing session

    for (int i = 0; i < NUM_LATENCY_SAMPLES; i++) {
        k_sem_reset(&isr_occurred_sem);

        // Ensure echo pin is low before triggering rising edge
        int ret = gpio_pin_set_dt(&trigger_gpio, 0);
        zassert_ok(ret, "Failed to set trigger pin low");
        k_busy_wait(10); // Short delay to ensure line is settled low

        // Critical section: measure latency
        k_sched_lock(); // Lock scheduler to minimize interference

        ts_start = timing_counter_get();
        ret = gpio_pin_set_dt(&trigger_gpio, 1); // Trigger the rising edge
        zassert_ok(ret, "Failed to set trigger pin high");

        // Busy wait for a very short period for the ISR to capture ts_end.
        // This is to ensure ts_start and the pin set are as close as possible.
        // The actual semaphore wait handles the ISR completion.
        // This part is sensitive; too long a wait here adds to measured time.
        // Too short, and the ISR might not have run yet if the system is slow.
        // The k_sem_take below is the primary synchronization.
        // For BCM2711, GPIOs are memory-mapped, so set should be fast.

        k_sched_unlock(); // Unlock scheduler

        // Wait for the ISR to signal completion
        if (k_sem_take(&isr_occurred_sem, K_MSEC(100)) != 0) {
            ztest_FAIL("ISR did not occur or semaphore timed out for sample %d", i);
            // Potentially break or continue, depending on desired strictness
            continue;
        }

        if (timing_cycles_get(&ts_start, &ts_end) > 0) { // Ensure end is after start
            total_latency_cycles += timing_cycles_get(&ts_start, &ts_end);
            num_samples++;
        } else {
            printk("Warning: ts_end <= ts_start for sample %d, discarding.\n", i);
        }

        // Reset trigger pin for next iteration
        ret = gpio_pin_set_dt(&trigger_gpio, 0);
        zassert_ok(ret, "Failed to reset trigger pin low");
        k_busy_wait(100); // Delay before next sample
    }

    timing_stop(); // Stop timing session

    if (num_samples > 0) {
        uint64_t avg_latency_cycles = total_latency_cycles / num_samples;
        uint64_t avg_latency_ns = timing_cycles_to_ns(avg_latency_cycles);
        printk("Average GPIO interrupt latency over %u samples: %llu cycles (%llu ns)\n",
               num_samples, avg_latency_cycles, avg_latency_ns);
        // Add an assertion if there's a known acceptable upper bound for latency
        // e.g., zassert_true(avg_latency_ns < 5000, "Avg latency %llu ns too high", avg_latency_ns);
    } else {
        ztest_FAIL("No valid latency samples were collected.");
    }
    zassert_true(num_samples > (NUM_LATENCY_SAMPLES / 2), "Too many samples were invalid.");
}

ZTEST_SUITE(gpio_bcm2711_interrupt_latency, NULL, setup_latency_test_pins, NULL, NULL, teardown_latency_test_pins);

ZTEST(gpio_bcm2711_interrupt_latency, test_latency)
{
    test_gpio_interrupt_latency();
}
