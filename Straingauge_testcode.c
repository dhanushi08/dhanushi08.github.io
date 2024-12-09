#include "uart.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#define F_CPU               16000000UL
#define UART_BAUD_RATE      9600
#define UART_BAUD_PRESCALER (((F_CPU / (UART_BAUD_RATE * 16UL))) - 1)
#define __PRINT_NEW_LINE__  UART_putstring("\r\n");

void Initialize_ADC() {
    // Clear power reduction bit for ADC
    PRR0 &= ~(1 << PRADC);

    // Select Vref = AVcc (5V)
    ADMUX |= (1 << REFS0);
    ADMUX &= ~(1 << REFS1);

    // Set ADC clock prescaler to 128 (16M/128=125kHz)
    ADCSRA |= (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);

    // Select ADC0 (pin C0)
    ADMUX &= ~(1 << MUX0) & ~(1 << MUX1) & ~(1 << MUX2) & ~(1 << MUX3);

    // Enable ADC auto-triggering in free-running mode
    ADCSRA |= (1 << ADATE);
    ADCSRB &= ~(1 << ADTS0) & ~(1 << ADTS1) & ~(1 << ADTS2);

    // Disable digital input buffer on ADC0
    DIDR0 |= (1 << ADC0D);

    // Enable ADC
    ADCSRA |= (1 << ADEN);

    // Start the ADC conversion
    ADCSRA |= (1 << ADSC);
}

// This function converts ADC reading to weight based on a calibration factor
float convert_ADC_to_weight(uint16_t adc_value) {
    // Calibration factor: grams per ADC unit
    float calibration_factor = 0.5; // Placeholder; replace with actual calibrated value
    return adc_value * calibration_factor;
}

// Calibration function to determine scale factor based on known weights
float calibrate_scale_factor(uint16_t adc_reading_for_known_weight, float known_weight) {
    return known_weight / adc_reading_for_known_weight;
}

int main(void) {
    Initialize_ADC();
    UART_init(UART_BAUD_PRESCALER);

    // Calibration: Adjust this part after measuring with a known weight
    uint16_t adc_reading_for_known_weight = 200;  // Example ADC reading for known weight
    float known_weight = 100.0;  // Example known weight in grams
    float calibration_factor = calibrate_scale_factor(adc_reading_for_known_weight, known_weight);

    while (1) {
        uint16_t adc_value = ADC;   // Read the ADC value
        float weight = adc_value * calibration_factor;  // Convert ADC to weight
        
        // Print the weight
        char buffer[20];
        sprintf(buffer, "Weight: %.2f g", weight);
        UART_putstring(buffer);
        __PRINT_NEW_LINE__

        _delay_ms(500);  // Delay for readability
    }
}
