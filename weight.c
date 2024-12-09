

#define F_CPU               16000000UL
#define UART_BAUD_RATE      9600
#define UART_BAUD_PRESCALER (((F_CPU / (UART_BAUD_RATE * 16UL))) - 1)
#define PRESCALER           64
#define __PRINT_NEW_LINE__  UART_putstring("\r\n");
#define HX711_DATA PD2  // Data pin
#define HX711_CLK PD3   // Clock pin


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include "uart.h"


static uint8_t GAIN = 2; // Default gain
static int32_t offset = 0; // Offset value for calibration
static float calibrationFactor = 1.0; // Calibration factor

void HX711_init() {
    DDRD &= ~(1 << HX711_DATA);  // Set DATA pin as input
    DDRD |= (1 << HX711_CLK);   // Set CLOCK pin as output
    PORTD &= ~(1 << HX711_CLK); // Ensure CLOCK starts low
}

void HX711_setGain(uint8_t gain) {
    switch (gain) {
        case 128:
            GAIN = 1; // 1 clock pulse
            break;
        case 64:
            GAIN = 3; // 3 clock pulses
            break;
        case 32:
            GAIN = 2; // 2 clock pulses
            break;
        default:
            GAIN = 1; // Default to gain of 128
    }

    PORTD &= ~(1 << HX711_CLK); // Ensure CLOCK is low
   
}

uint32_t HX711_read() {
    uint8_t data[3] = {0};

    // Wait for the DATA pin to go low, indicating data is ready
    while (PIND & (1 << HX711_DATA));

    // Read 24 bits of data (3 bytes)
    for (int8_t i = 2; i >= 0; i--) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            PORTD |= (1 << HX711_CLK);       // Set CLOCK high
            _delay_us(1);                    // Short delay
            data[i] = (data[i] << 1) | ((PIND & (1 << HX711_DATA)) >> HX711_DATA); // Read DATA pin bit
            PORTD &= ~(1 << HX711_CLK);      // Set CLOCK low
            _delay_us(1);                    // Short delay
        }
    }

    // Apply gain by sending additional clock pulses
    for (uint8_t i = 0; i < GAIN; i++) {
        PORTD |= (1 << HX711_CLK); // Set CLOCK high
        _delay_us(1);
        PORTD &= ~(1 << HX711_CLK); // Set CLOCK low
        _delay_us(1);
    }

    // Convert 24-bit signed data to 32-bit signed value
    if (data[2] & 0x80) { // If the MSB (bit 23) is set
       return ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | data[0]  | 0xFF000000; // Sign-extend for negative values
    } else {
      return ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | data[0]; // Positive value
    }
}
void HX711_calibrate() {
    offset = HX711_read(); // Capture offset value with no load
}

float HX711_getWeight() {
    int32_t rawValue = HX711_read() - offset; // Subtract offset
    return (float) rawValue / calibrationFactor; // Apply calibration factor
}

int main() {
    HX711_init();
    UART_init(UART_BAUD_PRESCALER);
    HX711_setGain(GAIN); 
    HX711_calibrate();
    while (1) {
        float weight = HX711_getWeight();
        // Do something with 'weight'
        calibrationFactor = 420;
        char buffer[20];
        sprintf(buffer, "Weight: %.2f", weight);
        UART_putstring("ADC Read:\t");
        UART_putstring(buffer);
        __PRINT_NEW_LINE__
        _delay_ms(500); 
    }

    return 0;
}
