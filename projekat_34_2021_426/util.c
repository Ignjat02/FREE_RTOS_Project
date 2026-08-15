/**
 * @file util.c
 * @author Strahinja Jankovic (jankovics@etf.bg.ac.rs)
 * @date 2019
 * @brief Utility functions
 *
 * Hooks that are not needed in everyday work
 */

/* Standard includes. */
#include <stdio.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* Hardware includes. */
#include "msp430.h"

/* User's includes */
#include "ETF5529_HAL/hal_ETF_5529.h"

/**
 * @brief Tick hook
 */
void vApplicationTickHook( void )
{
    ;
}

/**
 * @author FreeRTOS
 * @brief Configure Tick
 *
 * The MSP430X port uses this callback function to configure its tick interrupt.
 * This allows the application to choose the tick interrupt source.
 * configTICK_VECTOR must also be set in FreeRTOSConfig.h to the correct
 * interrupt vector for the chosen tick interrupt source.  This implementation of
 * vApplicationSetupTimerInterrupt() generates the tick from timer A0, so in this
 * case configTICK_VECTOR is set to TIMER0_A0_VECTOR.
 */
void vApplicationSetupTimerInterrupt( void )
{
    const unsigned short usACLK_Frequency_Hz = 32768;

    /* Ensure the timer is stopped. */
    TA0CTL = 0;

    /* Run the timer from the ACLK. */
    TA0CTL = TASSEL_1;

    /* Clear everything to start with. */
    TA0CTL |= TACLR;

    /* Set the compare match value according to the tick rate we want. */
    TA0CCR0 = usACLK_Frequency_Hz / configTICK_RATE_HZ;

    /* Enable the interrupts. */
    TA0CCTL0 = CCIE;

    /* Start up clean. */
    TA0CTL |= TACLR;

    /* Up mode. */
    TA0CTL |= MC_1;
}


void vApplicationMallocFailedHook( void )
{
    HAL_7SEG_DISPLAY_1_ON;
    HAL_7SEG_DISPLAY_2_ON;
    vHAL7SEGWriteDigit(0xE);   /* shows 'E' -> ran out of heap */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pxTask;
    ( void ) pcTaskName;
    HAL_7SEG_DISPLAY_1_ON;
    HAL_7SEG_DISPLAY_2_ON;
    vHAL7SEGWriteDigit(0xF);   /* shows 'F' -> stack overflow */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}

/**
 * @author FreeRTOS
 * @brief Idle Hook
 */
void vApplicationIdleHook( void )
{
    /* Called on each iteration of the idle task.  In this case the idle task
    just enters a low(ish) power mode. */
    __bis_SR_register( LPM0_bits + GIE );
}

/**
 * @author FreeRTOS
 * @brief Called when malloc fails
 */


