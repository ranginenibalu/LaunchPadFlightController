/* Copyright (C) 2014 Kristian Lauszus, TKJ Electronics. All rights reserved.

 This software may be distributed and modified under the terms of the GNU
 General Public License version 2 (GPL2) as published by the Free Software
 Foundation and appearing in the file GPL2.TXT included in the packaging of
 this file. Please note that GPL2 Section 2[b] requires that all works based
 on this software must also be made publicly available under the terms of
 the GPL2 ("Copyleft").

 Contact information
 -------------------

 Kristian Lauszus, TKJ Electronics
 Web      :  http://www.tkjelectronics.com
 e-mail   :  kristianl@tkjelectronics.com
*/

#include <stdint.h>
#include <stdbool.h>

#include "RX.h"
#include "time.h"

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
//#include "utils/uartstdio.h" // Add "UART_BUFFERED" to preprocessor

volatile uint16_t rxChannel[RX_NUM_CHANNELS];
volatile bool validRXData;

void CaptureHandler(void) {
    static uint8_t channelIndex = 0;
    static uint32_t prev = 0;
    static bool last_edge = false;
    //static uint32_t prev_micros = 0;

    TimerIntClear(WTIMER1_BASE, TIMER_CAPA_EVENT); // Clear interrupt
    uint32_t curr = TimerValueGet(WTIMER1_BASE, TIMER_A); // Read capture value
    bool edge = GPIOPinRead(GPIO_PORTC_BASE, GPIO_PIN_6); // Read the GPIO pin

    if (last_edge && !edge) { // Check that we are going from a positive to falling egde
        uint32_t diff = curr - prev; // Calculate diff
        uint32_t diff_us = 1000000UL / (SysCtlClockGet() / diff); // Convert to us
#if 0
        UARTprintf("%u %u %d\n", diff, diff_us,  micros() - prev_micros);
#else
        if (diff_us > 2700) { // Check if sync pulse is received - see: https://github.com/multiwii/baseflight/blob/master/src/drv_pwm.c
            channelIndex = 0; // Reset channel index
            validRXData = true;
            for (uint8_t i = 0; i < RX_NUM_CHANNELS; i++) {
                if (rxChannel[i] == 0) // Make sure that all are above 0
                    validRXData = false;
            }
            if (validRXData)
                TimerLoadSet(WTIMER1_BASE, TIMER_B, SysCtlClockGet() / 10 - 1); // Reset timeout value to 100ms
#if 0
            for (uint8_t i = 0; i < RX_NUM_CHANNELS; i++) {
                if (rxChannel[i] > 0)
                    UARTprintf("%u\t", rxChannel[i]);
                else
                    break;
            }
            UARTprintf("\n");
#endif
        } else if (channelIndex < RX_NUM_CHANNELS)
            rxChannel[channelIndex++] = diff_us;
#endif
    }

    prev = curr; // Store previous value
    last_edge = edge; // Store last edge
    //prev_micros = micros();
}

void TimeoutHandler(void) {
    TimerIntClear(WTIMER1_BASE, TIMER_TIMB_TIMEOUT); // Clear interrupt
    validRXData = false; // Indicate that connection was lost
}

// WTimer1A is used to measure the width of the pulses
// WTimer1B is used to turn off motors if the connection to the RX is lost
void initRX(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER1); // Enable Wide Timer1 peripheral
    SysCtlDelay(2); // Insert a few cycles after enabling the peripheral to allow the clock to be fully activated

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC); // Enable GPIOC peripheral
    SysCtlDelay(2); // Insert a few cycles after enabling the peripheral to allow the clock to be fully activated
    GPIOPinConfigure(GPIO_PC6_WT1CCP0); // Use alternate function
    GPIOPinTypeTimer(GPIO_PORTC_BASE, GPIO_PIN_6); // Use pin with timer peripheral

    // Split timers and enable timer A event up-count timer and timer B as a periodic timer
    TimerConfigure(WTIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME_UP | TIMER_CFG_B_PERIODIC);

    // Configure WTimer1A
    TimerControlEvent(WTIMER1_BASE, TIMER_A, TIMER_EVENT_BOTH_EDGES); // Interrupt on both edges
    TimerIntRegister(WTIMER1_BASE, TIMER_A, CaptureHandler); // Register interrupt handler
    TimerIntEnable(WTIMER1_BASE, TIMER_CAPA_EVENT); // Enable timer capture A event interrupt
    IntPrioritySet(INT_WTIMER1A, 0); // Configure Timer 1A interrupt priority as 0
    IntEnable(INT_WTIMER1A); // Enable wide Timer 1A interrupt

    // Configure WTimer1B
    TimerLoadSet(WTIMER1_BASE, TIMER_B, SysCtlClockGet() / 10 - 1); // Set to interrupt every 100ms
    TimerIntRegister(WTIMER1_BASE, TIMER_B, TimeoutHandler); // Register interrupt handler
    TimerIntEnable(WTIMER1_BASE, TIMER_TIMB_TIMEOUT); // Enable timer timeout interrupt
    IntPrioritySet(INT_WTIMER1B, 0); // Configure Timer0A interrupt priority as 0
    IntEnable(INT_WTIMER1B); // Enable wide Timer 1B interrupt

    TimerEnable(WTIMER1_BASE, TIMER_BOTH); // Enable both timers

    validRXData = false;
}
