#include <stdbool.h>

#include "rtos-kochab.h"
#include "debug.h"

#define DEMO_ERROR_ID_WATCHDOG_A 0xfa
#define DEMO_ERROR_ID_WATCHDOG_B 0xfb
#define DEMO_ERROR_ID_TEST_FAIL 0xff

#define DEMO_A_WAKE_PERIOD 3
#define DEMO_A_SLEEP_TICKS (2 * DEMO_A_WAKE_PERIOD)
#define DEMO_A_WAIT_COUNT 10
#define DEMO_A_EXPECTED_TICKS ((DEMO_A_WAIT_COUNT / 2) * (DEMO_A_WAKE_PERIOD + DEMO_A_SLEEP_TICKS))

#define DEMO_B_SLEEP_TICKS 2
#define DEMO_B_WATCHDOG_TICKS (DEMO_B_SLEEP_TICKS + 1)

#define DEMO_FAIL_IF(cond, fail_str) \
    if (cond) { \
        debug_println(fail_str); \
        fatal(DEMO_ERROR_ID_TEST_FAIL); \
    }
#define DEMO_FAIL_UNLESS(cond, fail_str) DEMO_FAIL_IF(!cond, fail_str)

bool
tick_irq(void)
{
    static uint8_t count;

    asm volatile(
        /* Write-1-to-clear:
         *   In TSR (timer status register)
         *     TSR[FIS] (fixed-interval timer interrupt status) */
        "lis %%r3,0x400\n"
        "mttsr %%r3\n"
        ::: "r3");

    debug_print("tick_irq: ");
    debug_printhex32(count);
    debug_println("");
    count++;

    rtos_timer_tick();

    return true;
}

void
fatal(const RtosErrorId error_id)
{
    debug_print("FATAL ERROR: ");
    debug_printhex32(error_id);
    debug_println("");
    for (;;)
    {
    }
}

void
fn_a(void)
{
    uint8_t count;
    RtosTicksAbsolute demo_start;

    debug_println("a: enabling watchdog timer");
    rtos_timer_error_set(RTOS_TIMER_ID_WATCHDOG_A, DEMO_ERROR_ID_WATCHDOG_A);
    demo_start = rtos_timer_current_ticks;
    rtos_timer_oneshot(RTOS_TIMER_ID_WATCHDOG_A, DEMO_A_EXPECTED_TICKS + 1);

    debug_println("a: enabling periodic wake timer");
    rtos_timer_signal_set(RTOS_TIMER_ID_WAKE_A, RTOS_TASK_ID_A, RTOS_SIGNAL_ID_WAKE);
    rtos_timer_reload_set(RTOS_TIMER_ID_WAKE_A, DEMO_A_WAKE_PERIOD);
    rtos_timer_enable(RTOS_TIMER_ID_WAKE_A);

    for (count = 0; count < DEMO_A_WAIT_COUNT; count++) {
        if (count % 2) {
            /* No time elapses */
            DEMO_FAIL_UNLESS(rtos_signal_poll(RTOS_SIGNAL_ID_WAKE), "a: signal poll unexpectedly failed!");
            debug_println("a: polled pending wake signal");
        } else {
            /* DEMO_A_WAKE_PERIOD elapses */
            DEMO_FAIL_IF(rtos_signal_peek(RTOS_SIGNAL_ID_WAKE), "a: signal peek unexpectedly succeeded!");
            debug_println("a: waiting for wake signal");
            rtos_signal_wait(RTOS_SIGNAL_ID_WAKE);

            /* DEMO_A_SLEEP_TICKS elapses */
            debug_println("a: sleeping to overflow wake timer");
            rtos_sleep(DEMO_A_SLEEP_TICKS);

            /* This check should clear the overflow status */
            DEMO_FAIL_UNLESS(rtos_timer_check_overflow(RTOS_TIMER_ID_WAKE_A), "a: timer should have overflowed!");
        }
        DEMO_FAIL_IF(rtos_timer_check_overflow(RTOS_TIMER_ID_WAKE_A), "a: timer unexpectedly overflowed!");
    }

    debug_println("a: disabling periodic wake timer");
    rtos_timer_disable(RTOS_TIMER_ID_WAKE_A);

    DEMO_FAIL_UNLESS(rtos_timer_current_ticks == demo_start + DEMO_A_EXPECTED_TICKS, "a: unexpected elapsed time!");

    debug_println("a: the watchdog should fatal error before this sleep completes");
    rtos_sleep(1);

    debug_println("a: shouldn't be here!");
    for (;;)
    {
    }
}

void
fn_b(void)
{
    debug_println("b: starting secondary watchdog");
    rtos_timer_error_set(RTOS_TIMER_ID_WATCHDOG_B, DEMO_ERROR_ID_WATCHDOG_B);

    for (;;) {
        rtos_timer_oneshot(RTOS_TIMER_ID_WATCHDOG_B, DEMO_B_WATCHDOG_TICKS);
        rtos_sleep(DEMO_B_SLEEP_TICKS);
        debug_println("b: restarting secondary watchdog");
        rtos_timer_disable(RTOS_TIMER_ID_WATCHDOG_B);
    }
}

int
main(void)
{
    /*
     * Configure a fixed interval timer
     * Enable:
     *  In TCR (timer control register)
     *    TCR[FIE] (fixed-interval interrupt enable)
     *  In MSR (machine state register)
     *    MSR[EE] (external enable) -> Note: this also enables other async interrupts
     *
     * Set period: TCR[FPEXT] || TCR[FP]
     */
    asm volatile(
        "mftcr %%r3\n"
        "oris %%r3,%%r3,0x380\n" /* 0x300 = TCR[FP], 0x80 = TCR[FIE] */
        "mttcr %%r3"
        ::: "r3");

    debug_println("Starting RTOS");
    rtos_start();
    /* Should never reach here, but if we do, an infinite loop is
       easier to debug than returning somewhere random. */
    for (;;) ;
}
