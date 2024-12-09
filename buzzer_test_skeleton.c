
/*
 * File:   Combined_Buzzer_PWM.c
 * Author: Kiet Cao
 * Created on October 10, 2024, 3:42 PM
 */



#define F_CPU               16000000UL
#define UART_BAUD_RATE      9600
#define UART_BAUD_PRESCALER (((F_CPU / (UART_BAUD_RATE * 16UL))) - 1)
#define PRESCALER           64
#define PWM_FREQUENCY       1000  // Frequency of the buzzer tone in Hz
#define CHECK_INTERVAL      5     // Time in minutes
#define VALUE_THRESHOLD     5     // Percentage change threshold

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
//#include "uart.h"

volatile uint16_t overflowCount = 0;      // Overflow counter for Timer1
volatile uint32_t previousValue = 100;   // Example initial value
volatile uint32_t currentValue = 120;    // Current value
volatile uint8_t checkFlag = 0;          // Flag to indicate the need to check the value



void Initialize_Timer1() {
    // Configure Timer1 for overflow every ~1 second
    TCCR1A = 0;
    TCCR1B |= (1 << CS12);  // Set prescaler to 256
    TIMSK1 |= (1 << TOIE1); // Enable overflow interrupt
}

void setupPWM(uint8_t dutyCycle, uint8_t frequency) { //setup buzzer
    TCCR2A |= (1 << WGM20);                 // Phase Correct PWM mode
    TCCR2A &= ~(1 << WGM21);
    TCCR2B &= ~(1 << WGM22);

    TCCR2B |= (1 << CS22);                  // Set prescaler to 64
    TCCR2B &= ~((1 << CS21) | (1 << CS20));

    TCCR2A &= ~(1 << COM2B1);               // Set non-inverting mode for OC2B (PD3)
    TCCR2A &= ~(1 << COM2B0);

    OCR2A = frequency;                      // Set OCR2A for frequency
    OCR2B = (OCR2A * dutyCycle) / 100;      // Set OCR2B for duty cycle

    DDRD |= (1 << DDD3);                    // Set PD3 (OC2B) as output
}

void triggerPWM() {//Trigger buzzer for a duration
    TCCR2A |= (1 << COM2B1);                // Enable PWM on PD3
    _delay_ms(100);                         // Wait for the specified duration
    TCCR2A &= ~(1 << COM2B1);               // Disable PWM on PD3

    PORTB |= (1 << PB2);     // Red
    PORTB &= ~(1 << PB1);

}


ISR(TIMER1_OVF_vect) {
    overflowCount++;
    // Check if 5 minutes have passed (assuming ~1 second per overflow)
    if (overflowCount >= (CHECK_INTERVAL * 5)) {
        overflowCount = 0; // Reset overflow counter
        checkFlag = 1;     // Set flag to perform the check
    }
}

int main(void) {
    cli();                   // Disable global interrupts
    Initialize_Timer1();     // Initialize Timer1 for 5-minute checks
    setupPWM(50,100); // Initialize PWM for the buzzer
   // UART_init(UART_BAUD_PRESCALER); // Initialize UART for debugging
    DDRB |= (1 << PB2);      // Configure PB2 as output for LED
    DDRB |= (1 << PB1);
    sei();                   // Enable global interrupts
    PORTB |= (1 << PB1);
   // UART_putstring("System Initialized\r\n");

    while (1) {
        //triggerPWM();
        
        if (checkFlag) {     // If the check flag is set
            checkFlag = 0;   // Reset the flag

        
            currentValue = 80; // Replace this with real sensor reading

            // Check if the value decreases by more than the threshold
            if (previousValue > 0) {
                uint32_t change = ((previousValue - currentValue) * 100) / previousValue;
                if (change > VALUE_THRESHOLD) {
                   // UART_putstring("Threshold Exceeded\r\n");
                    triggerPWM(); // Trigger buzzer and LED
                }
                else{
                    PORTB &= ~(1 << PB2);
                    PORTB |= (1 << PB1);
                }
            }

            previousValue = currentValue; // Update the previous value
        }
    }
}
