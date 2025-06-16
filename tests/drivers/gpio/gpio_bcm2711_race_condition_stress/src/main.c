#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>
#include <zephyr/sys/atomic.h>

// Pins within the same GPFSEL bank (0-9, 10-19, 20-27 for gpio0; 28-37, 38-45 for gpio1)
// And same GPPULL bank (0-15, 16-27 for gpio0; 28-43, 44-45 for gpio1)
// Using pins on &gpio1 for RPi 4B example
// GPIOs 28, 29, 30, 31 are within the same GPFSEL bank and GPPULL bank on BCM2711's &gpio1
#define NUM_THREADS 4
#define NUM_ITERATIONS 100 // Number of configuration attempts per thread

// DT Specs - these must be verified for rpi_4b
const struct gpio_dt_spec pin_specs[] = {
    GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_28), // Pin for thread 0
    GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_29), // Pin for thread 1
    GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_30), // Pin for thread 2
    GPIO_DT_SPEC_GET_BY_NAME(DT_NODELABEL(gpio1), gpios_31)  // Pin for thread 3
};

K_THREAD_STACK_ARRAY_DEFINE(thread_stacks, NUM_THREADS, 1024);
static struct k_thread threads[NUM_THREADS];
static atomic_t success_counts[NUM_THREADS]; // Count successful configurations

// Thread function to stress GPIO configuration
static void stress_gpio_config_thread(void *p1, void *p2, void *p3)
{
    int thread_idx = (int)(uintptr_t)p1;
    const struct gpio_dt_spec *spec = &pin_specs[thread_idx];
    int ret;

    printk("Thread %d starting, targeting GPIO pin %d (raw %d on port %s)\n",
           thread_idx, spec->pin, spec->pin, spec->port->name);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Varying configurations to maximize potential conflicts
        gpio_flags_t flags = GPIO_INPUT; // Default
        if ((i + thread_idx) % 4 == 0) {
            flags = GPIO_OUTPUT_LOW;
        } else if ((i + thread_idx) % 4 == 1) {
            flags = GPIO_OUTPUT_HIGH;
        } else if ((i + thread_idx) % 4 == 2) {
            flags = GPIO_INPUT | GPIO_PULL_UP;
        } else {
            flags = GPIO_INPUT | GPIO_PULL_DOWN;
        }

        ret = gpio_pin_configure_dt(spec, flags);
        if (ret == 0) {
            atomic_inc(&success_counts[thread_idx]);
        } else {
            // Log error but continue, as some ENOTSUP might be expected if trying unsupported flags
            printk("Thread %d: gpio_pin_configure_dt failed for pin %d with flags 0x%lx, ret %d\n",
                   thread_idx, spec->pin, flags, ret);
        }

        // Also stress interrupt configuration
        enum gpio_int_mode int_mode = ((i % 2) == 0) ? GPIO_INT_MODE_EDGE : GPIO_INT_MODE_DISABLED; // Toggle between edge and disabled
        enum gpio_int_trig int_trig = ((i % 2) == 0) ? GPIO_INT_TRIG_BOTH : GPIO_INT_TRIG_LOW; // Vary trigger

        if (int_mode != GPIO_INT_MODE_DISABLED) {
             ret = gpio_pin_interrupt_configure_dt(spec, int_mode | int_trig);
        } else {
             ret = gpio_pin_interrupt_configure_dt(spec, GPIO_INT_MODE_DISABLED);
        }

        if (ret == 0) {
            // Could add another atomic counter for interrupt config successes if needed
        } else {
             printk("Thread %d: gpio_pin_interrupt_configure_dt failed for pin %d, ret %d\n",
                    thread_idx, spec->pin, ret);
        }
        k_yield(); // Yield to give other threads a chance to run
    }
    printk("Thread %d finished.\n", thread_idx);
}

static void test_gpio_race_condition_stress(void)
{
    printk("Starting GPIO race condition stress test with %d threads, %d iterations each.\n",
           NUM_THREADS, NUM_ITERATIONS);

    for (int i = 0; i < NUM_THREADS; i++) {
        atomic_set(&success_counts[i], 0);
        zassert_true(device_is_ready(pin_specs[i].port), "GPIO port for pin_specs[%d] not ready", i);

        k_thread_create(&threads[i], thread_stacks[i], K_THREAD_STACK_SIZEOF(thread_stacks[i]),
                        stress_gpio_config_thread, (void *)(uintptr_t)i, NULL, NULL,
                        K_PRIO_PREEMPT(7), 0, K_NO_WAIT); // Use K_PRIO_PREEMPT if available
    }

    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        k_thread_join(&threads[i], K_FOREVER);
    }

    printk("All stress threads finished. Verifying results...\n");

    for (int i = 0; i < NUM_THREADS; i++) {
        printk("Thread %d configuration success count: %d/%d\n", i, atomic_get(&success_counts[i]), NUM_ITERATIONS);
        // If the spinlock works correctly, all configurations should ideally succeed.
        // A more robust test would try to read back the actual pin configuration,
        // but this is hard to do programmatically for all aspects (direction, pull, interrupt mode)
        // without special debug interfaces or external hardware.
        // For now, we primarily check that the calls didn't return errors frequently.
        // Some errors might be legitimate if trying an unsupported config, but widespread errors would indicate a problem.
        zassert_true(atomic_get(&success_counts[i]) >= (NUM_ITERATIONS - (NUM_ITERATIONS / 10)), // Allow for a few potential legitimate errors if any flags are not always supported
                     "Thread %d had too many configuration failures (%d/%d)", i, atomic_get(&success_counts[i]), NUM_ITERATIONS);
    }

    // Basic verification: try to configure all pins to a known state and check set/get
    // This doesn't fully prove the stress test didn't corrupt something subtly, but it's a basic check.
    for (int i = 0; i < NUM_THREADS; i++) {
        const struct gpio_dt_spec *spec = &pin_specs[i];
        int ret_cfg = gpio_pin_configure_dt(spec, GPIO_OUTPUT_LOW);
        zassert_ok(ret_cfg, "Final configure OUTPUT_LOW for pin %d failed", spec->pin);
        int ret_set = gpio_pin_set_dt(spec, 1);
        zassert_ok(ret_set, "Final set HIGH for pin %d failed", spec->pin);
        int val = gpio_pin_get_dt(spec); // Should read as 1 since it's output and set high
        zassert_equal(val, 1, "Final get for pin %d did not return 1 (was %d)", spec->pin, val);
    }
    printk("Stress test completed. If spinlocks are effective, success counts should be high.\n");
}

ZTEST_SUITE(gpio_bcm2711_race_condition_stress, NULL, NULL, NULL, NULL, NULL);

ZTEST(gpio_bcm2711_race_condition_stress, test_stress_config)
{
    test_gpio_race_condition_stress();
}
