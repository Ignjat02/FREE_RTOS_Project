

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

/* Hardware includes. */
#include "msp430.h"

/* User's includes */
#include "../ETF5529_HAL/hal_ETF_5529.h"

/* Task priorities */
/** "Button task" priority */
#define mainBUTTON_TASK_PRIO                ( 1 )

/* Diode Timer Period */
#define ACQUISITION_PERIOD_MS    500

/* This Semaphore will be used to signal potential "Button press" event */
xSemaphoreHandle    xEvent_Button;

/* Software Timer handler */
TimerHandle_t       xAcquisitionTimer;

/* Char queue parameters value */
#define mainADC_QUEUE_LENGTH               10
#define mainTASK_QUEUE_LENGTH              10

uint16_t avgA0;
uint16_t avgA1;

typedef struct {
    uint8_t channel;
    uint16_t temp;
} ADC_Message_t;

typedef enum {
    BUTTON_NONE,
    BUTTON_S3,
    BUTTON_S4
} button_t;

volatile button_t xPressedButton = BUTTON_NONE;

/* Queue handles */
QueueHandle_t xADCQueue;
QueueHandle_t xTaskQueue;

/* Task handles - potrebni za slanje notifikacija */
TaskHandle_t xTask1Handle = NULL;
TaskHandle_t xTask3Handle = NULL;
TaskHandle_t xTask4Handle = NULL;

static void prvSetupHardware( void );

/* Callback function za tajmer koji pokreće ADC */
void vAcquisitionCallback(TimerHandle_t xTimer)
{
    /* ADC12ENC (Enable Conversion) + ADC12SC (Start Conversion) */
    ADC12CTL0 |= ADC12ENC + ADC12SC;
}

/* Button task za obradu pritiska na S3 i S4 */
static void xTask2 ( void *pvParameters )
{
    uint16_t i;
    /* Initial button states are 1 because of pull-up configuration */
    uint8_t currentButtonState = 1;
    uint8_t targetTask = 0;

    for ( ;; )
    {
        /* waits for semaphore to be released from ISR */
        xSemaphoreTake(xEvent_Button, portMAX_DELAY);

        /* wait for a little to check that button is still pressed (debounce) */
        for(i = 0; i < 1000; i++);

        switch(xPressedButton) {
            case BUTTON_NONE:
                break;

            case BUTTON_S3:
                /* read button state after debouncing */
                currentButtonState = ((P1IN & 0x10) >> 4);
                if(currentButtonState == 0) {
                    targetTask = 3; // Target task flag
                    /* Sending which task is targeted */
                    xQueueSend(xTaskQueue, &targetTask, 0);
                }
                break;

            case BUTTON_S4:
                /* read button state after debouncing */
                currentButtonState = ((P1IN & 0x20) >> 5);
                if(currentButtonState == 0) {
                    targetTask = 4; // Target task flag
                    /* Sending which task is targeted */
                    xQueueSend(xTaskQueue, &targetTask, 0);
                }
                break;
        }
    }
}

/* Main programm task */
static void xTask1( void *pvParameters )
{
    ADC_Message_t receive;
    uint8_t target;
    uint8_t currentTarget = 0;

    uint8_t samplesA0[8] = {0};
    uint8_t samplesA1[8] = {0};

    uint8_t iA0 = 0;
    uint8_t iA1 = 0;

    uint16_t sumA0 = 0;
    uint16_t sumA1 = 0;

    uint32_t packedValue;
    uint8_t i;

    for ( ;; )
    {
        /* Receiving ADC conversion */
        if (xQueueReceive(xADCQueue, &receive, portMAX_DELAY) == pdTRUE)
        {
            /* Provera da li je stigla komanda sa tastera BEZ blokiranja */
            if (xQueueReceive(xTaskQueue, &target, 0) == pdTRUE)
            {
                currentTarget = target;
            }

            if (receive.channel == 0) {
                samplesA0[iA0] = (uint8_t)receive.temp;

                iA0 = (iA0 + 1) % 8;

                sumA0 = 0;
                for (i = 0; i < 8; i++) {
                    sumA0 += samplesA0[i];
                }
                avgA0 = sumA0 >> 3;
            }
            else if (receive.channel == 1) {
                samplesA1[iA1] = (uint8_t)receive.temp;

                iA1 = (iA1 + 1) % 8;

                sumA1 = 0;
                for (i = 0; i < 8; i++) {
                    sumA1 += samplesA1[i];
                }
                avgA1 = sumA1 >> 3;
            }

            /*
             * Pakovanje 32-bitne poruke:
             * Bajt 3 (najviši): Identifikator ciljanog taska (3 ili 4)
             * Bajt 2: Srednja vrednost A0
             * Bajt 0 (najniži): Srednja vrednost A1
             */
            packedValue = ((uint32_t)currentTarget << 24) |
                          ((uint32_t)avgA0 << 16)         |
                          ((uint32_t)avgA1);

            /* Sending notification depended on switch */
            if (currentTarget == 3 && xTask3Handle != NULL)
            {
                xTaskNotify(xTask3Handle, packedValue, eSetValueWithOverwrite);
            }
            else if (currentTarget == 4 && xTask4Handle != NULL)
            {
                xTaskNotify(xTask4Handle, packedValue, eSetValueWithOverwrite);
            }
        }
    }
}

/* Activated on switch 3*/
static void xTask3( void *pvParameters )
{
    uint32_t receivedValue = 0;
    uint8_t  taskID = 3;
    uint8_t  valA0 = 0;
    uint8_t  hexDigit = 0;
    uint8_t  i;

    for ( ;; )
    {
        /* Waiting for notification */
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &receivedValue, 0) == pdTRUE)
        {
            taskID = (receivedValue >> 24) & 0xFF;
            valA0  = (receivedValue >> 16) & 0xFF;


            hexDigit = (valA0 >> 4) & 0x0F;


            for (i = 0; i < 50; i++)
            {
                /* Displaying '3' */
                HAL_7SEG_DISPLAY_1_ON;
                HAL_7SEG_DISPLAY_2_OFF;
                vHAL7SEGWriteDigit(taskID);
                vTaskDelay( pdMS_TO_TICKS(3) );

                /* Displaying value */
                HAL_7SEG_DISPLAY_1_OFF;
                HAL_7SEG_DISPLAY_2_ON;
                vHAL7SEGWriteDigit(hexDigit);
                vTaskDelay( pdMS_TO_TICKS(3) );
            }


            HAL_7SEG_DISPLAY_1_OFF;
            HAL_7SEG_DISPLAY_2_OFF;
        }
    }
}

/* Activated on switch 4*/
static void xTask4( void *pvParameters )
{
    uint32_t receivedValue = 0;
    uint8_t  taskID = 4;
    uint8_t  valA1 = 0;
    uint8_t  hexDigit = 0;
    uint8_t  i;

    for ( ;; )
    {
        /* Waiting for notification */
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &receivedValue, portMAX_DELAY) == pdTRUE)
        {
            taskID = (receivedValue >> 24) & 0xFF;
            valA1  = receivedValue & 0xFF;


            hexDigit = (valA1 >> 4) & 0x0F;


            for (i = 0; i < 50; i++)
            {
                /* Displaying '4' */
                HAL_7SEG_DISPLAY_1_ON;
                HAL_7SEG_DISPLAY_2_OFF;
                vHAL7SEGWriteDigit(taskID);
                vTaskDelay( pdMS_TO_TICKS(3) );

                /* Displaying value */
                HAL_7SEG_DISPLAY_1_OFF;
                HAL_7SEG_DISPLAY_2_ON;
                vHAL7SEGWriteDigit(hexDigit);
                vTaskDelay( pdMS_TO_TICKS(3) );
            }


            HAL_7SEG_DISPLAY_1_OFF;
            HAL_7SEG_DISPLAY_2_OFF;
        }
    }
}

/**
 * @brief main function
 */
void main( void )
{
    /* Configure peripherals */
    prvSetupHardware();

    /* Create tasks */
    xTaskCreate( xTask1,
                 "Main task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainBUTTON_TASK_PRIO,
                 &xTask1Handle
               );

    xTaskCreate( xTask2,
                 "Button Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainBUTTON_TASK_PRIO,
                 NULL
               );

    xTaskCreate( xTask3,
                 "Output 0",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainBUTTON_TASK_PRIO,
                 &xTask3Handle
               );

    xTaskCreate( xTask4,
                 "Output 1",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainBUTTON_TASK_PRIO,
                 &xTask4Handle
               );

    /* Create timer */
    xAcquisitionTimer = xTimerCreate("Acquisition timer",
                                     pdMS_TO_TICKS(ACQUISITION_PERIOD_MS),
                                     pdTRUE,
                                     NULL,
                                     vAcquisitionCallback);

    /* Create queue */
    xADCQueue  = xQueueCreate(mainADC_QUEUE_LENGTH, sizeof(ADC_Message_t));
    xTaskQueue = xQueueCreate(mainTASK_QUEUE_LENGTH, sizeof(uint8_t));

    /* Create semaphores */
    xEvent_Button = xSemaphoreCreateBinary();

    /* Start timer */
    xTimerStart(xAcquisitionTimer, 0);

    /* Start the scheduler. */
    vTaskStartScheduler();

    for( ;; );
}

/**
 * @brief Configure hardware upon boot
 */
static void prvSetupHardware( void )
{
    taskDISABLE_INTERRUPTS();

    /* Disable the watchdog. */
    WDTCTL = WDTPW + WDTHOLD;

    hal430SetSystemClock( configCPU_CLOCK_HZ, configLFXT_CLOCK_HZ );

    /* Init 7 seg display */
    vHAL7SEGInit();

    /* - Init buttons - */
    /* Set direction to input for P1.4 (SW3) and P1.5 (SW4) */
    P1DIR &= ~0x30;
    /* Enable pull-up resistor */
    P1REN |= 0x30;
    P1OUT |= 0x30;

    /* Enable interrupts for pins connected to SW3 (P1.4) and SW4 (P1.5) */
    P1IE  |= 0x30;
    P1IFG &= ~0x30;
    /* Interrupt is generated during high to low transition */
    P1IES |= 0x30;

    /* Initialize ADC */
    ADC12CTL0  = ADC12SHT02 + ADC12ON + ADC12MSC;   // Sampling time, ADC12 on, multiple sample and conversion
    ADC12CTL1  = ADC12SHP + ADC12CONSEQ_1;           // Use sampling timer, sequence-of-channels
    ADC12IE    = 0x01 + 0x02;                       // Enable interrupt for channel 0 and 1
    ADC12MCTL0 |= ADC12INCH_0;
    ADC12MCTL1 |= ADC12INCH_1 + ADC12EOS;
    ADC12CTL0  |= ADC12ENC;
    P6SEL      |= 0x01 + 0x02;                      // P6.0, P6.1 ADC option select

    /* Enable global interrupts */
    taskENABLE_INTERRUPTS();
}

void __attribute__ ( ( interrupt( ADC12_VECTOR ) ) ) vADC12ISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    ADC_Message_t msg;

    switch(__even_in_range(ADC12IV, 34))
    {
        case  0: break;
        case  2: break;
        case  4: break;
        case  6: // ADC12IFG0 (Kanal A0)
            msg.temp    = ADC12MEM0 >> 4; // Skaliranje na 8 bita
            msg.channel = 0;
            xQueueSendToBackFromISR(xADCQueue, &msg, &xHigherPriorityTaskWoken);
            break;

        case  8: // ADC12IFG1 (Kanal A1)
            msg.temp    = ADC12MEM1 >> 4; // Skaliranje na 8 bita
            msg.channel = 1;
            xQueueSendToBackFromISR(xADCQueue, &msg, &xHigherPriorityTaskWoken);
            break;

        default: break;
    }

    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void __attribute__ ( ( interrupt( PORT1_VECTOR ) ) ) vPORT1ISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if((P1IFG & 0x10) == 0x10) {
        /* Enter here if button SW3 is pressed */
        xPressedButton = BUTTON_S3;
        P1IFG &= ~0x10;
        xSemaphoreGiveFromISR(xEvent_Button, &xHigherPriorityTaskWoken);
    }
    else if((P1IFG & 0x20) == 0x20) {
        /* Enter here if button SW4 is pressed */
        xPressedButton = BUTTON_S4;
        P1IFG &= ~0x20;
        xSemaphoreGiveFromISR(xEvent_Button, &xHigherPriorityTaskWoken);
    }
    else {
        P1IFG &= ~0x30;
        xPressedButton = BUTTON_NONE;
    }

    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
