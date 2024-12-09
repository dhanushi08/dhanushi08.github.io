    #include <avr/io.h>
    #include <avr/interrupt.h>
    #include <util/delay.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include "uart.h"
    #include "lib/ST7735.h"
    #include "lib/LCD_GFX.h"

    // Definitions
    #define F_CPU 16000000UL
    #define UART_BAUD_RATE 9600
    #define UART_BAUD_PRESCALER (((F_CPU / (UART_BAUD_RATE * 16UL))) - 1)
    #define __PRINT_NEW_LINE__ UART_putstring("\r\n");
    #define ULTRASONIC_TRIGGER PD5
    #define ULTRASONIC_ECHO PB4
    #define HX711_DATA PD2
    #define HX711_CLK PD4
    #define DISTANCE_THRESHOLD 15 // in cm
    

    // Global Variables
volatile uint16_t timer1OverflowCount = 0; // Overflow count for 5-minute timer (fits within 16 bits)
volatile uint16_t ultrasonicTimerCount = 0; // Timer for ultrasonic sensor (short intervals, fits 16 bits)
volatile uint8_t checkFlag = 0; // Flag to start periodic check (boolean, 1 or 0)
volatile uint8_t ultrasonicMeasurementReady = 0; // Flag for ultrasonic measurement completion
volatile uint16_t overflowCount = 0; // Timer overflow for distance calculation (fits within 16 bits)
volatile uint16_t distance = 0; // Distance measurement (cm, typical range fits 16 bits)
float currentWeight = 0.0f; // Current measured weight (floating point for precision)
float previousWeight = 20000.0f; // Previous recorded weight (floating point for consistency)
float missing = 0.0f; // Missing water amount (floating point for precision)
volatile int32_t targetWeight = 0; // Target weight after periodic or manual checks (signed 32-bit for range)
volatile uint8_t status = 0; // Device status: 1 = ON, 0 = OFF (boolean)
volatile uint8_t drinkstatus = 0; // Hydration status: 0 = hydrated, 1 = drink now (boolean)

// HX711 Configuration
static uint8_t GAIN = 1; // HX711 gain configuration: 1 = default gain of 128
static uint8_t Period = 10; // Period between checks (in seconds)
int32_t hx711Offset = 0; // HX711 offset for calibration (signed 32-bit to handle negative offsets)
float hx711CalibrationFactor = 215.0f; // HX711 calibration factor (floating point for accurate scaling)


// LCD Colors
uint16_t fColor = 0x0000; // Foreground color (16-bit for RGB565 format)
uint16_t bColor = 0xFFFF; // Background color (16-bit for RGB565 format)

    // Function Declarations
    void Initialize();
    void Timer1_Init();
    void Timer3_Init(); // Ultrasonic-specific timer
    void Ultrasonic_Init();
    void HX711_Init();
    uint32_t HX711_Read();
    float HX711_GetWeight();
    void HX711_Calibrate();
    void TriggerBuzzer();
    void TriggerUltrasonic();
    void PrintLCD(float weight, uint8_t status);
    uint32_t MeasureDistance();
    void TriggerWarning();
    void Timer1_Start();
    void Timer1_Stop();
    void Timer1_Reset();
    void UART_PrintDistance(uint32_t distance);
    void UART_PrintWeight(float weight);

    int main() {
        Initialize();
        HX711_Init();
        HX711_setGain(GAIN);
        HX711_Calibrate();
        Ultrasonic_Init();
        LCD_setScreen(bColor);

        while (1) {
            if (ultrasonicMeasurementReady) {
                uint32_t distance = MeasureDistance();
                UART_PrintDistance(distance);

                if (distance < DISTANCE_THRESHOLD) {// starts the process if distance below threshold
                    status = 1; // Turn ON
                    currentWeight = HX711_GetWeight();
                    UART_PrintWeight(currentWeight);
                    UART_PrintWeight(previousWeight);
                    LCD_drawString(0, 10, "On!!", fColor, bColor);
                    Timer1_Start();
                    if ((currentWeight > 50) && (currentWeight < previousWeight * 9 / 10)) { // if any sudden weight change before periodic check
                        previousWeight = currentWeight; // Update weight reference
                        Timer1_Reset();
                        drinkstatus = 0;
                        PORTC |= (1 << PC5);     
                        PORTC &= ~(1 << PC4);
                        missing = 0;
                    }
                    if (previousWeight >  2*missing){
                        targetWeight = previousWeight - 30 -missing;// set a target weight for comparison
                    }
                    else{
                        targetWeight = targetWeight;
                    }
                    if (checkFlag) { //periodic check
                        checkFlag = 0;
                        if (currentWeight > targetWeight) {
                            TriggerWarning();
                            drinkstatus = 1;
                            missing = currentWeight - targetWeight;
                            previousWeight = currentWeight;
                        } else {
                            drinkstatus = 0; // Hydrated
                            PORTC |= (1 << PC5);     
                            PORTC &= ~(1 << PC4);
                            previousWeight = currentWeight;
                            missing = 0;
                        }
                        //previousWeight = currentWeight; // Update weight reference
                        Timer1_Reset();
                    }

                    UART_PrintStatus(drinkstatus);
                    PrintLCD(currentWeight, drinkstatus);
                    PrintMiss(missing);
                    UART_PrintTime(timer1OverflowCount);
                } else { // Distance above threshold
                    //UART_PrintTime(timer1OverflowCount);

                        status = 0;
                        missing = 0;
                        LCD_drawString(0, 10, "Away", fColor, bColor);
                        Timer1_Stop();
                        PORTC &= ~(1 << PC5);     
                        PORTC &= ~(1 << PC4);
                }

                ultrasonicMeasurementReady = 0; // Reset ultrasonic flag
            }
        }
    }   



    // Initialization Functions
    void Initialize() {
        cli();
        UART_init(UART_BAUD_PRESCALER);
        lcd_init();
        Timer1_Init();
        Timer3_Init();
        setupPWM(150,200);
        TriggerBuzzer();// Initialize PWM for the buzzer
       // UART_init(UART_BAUD_PRESCALER); // Initialize UART for debugging
        DDRC |= (1 << PC5);      // Configure PC5 as output for green LED, PC4 for red LED
        DDRC |= (1 << PC4);
        //sei();                   // Enable global interrupts
        PORTC &= ~(1 << PC5);
        PORTC &= ~(1 << PC4);
        sei();
    }

    void Timer1_Init() { // initialize timer 1 for frequent checking
        TCCR1A = 0;
        TCCR1B |= (1 << CS12);   // Prescaler 256
        TIMSK1 |= (1 << TOIE1);  // Enable overflow interrupt
    }

    ISR(TIMER1_OVF_vect) {
        timer1OverflowCount++;
        if (timer1OverflowCount >= Period) { // Each overflow approx 1s, time period value for whole duration desired
            timer1OverflowCount = 0;
            checkFlag = 1;
        }
    }

    void Timer1_Start() {
        TCNT1 = 0; // Reset counter
        //timer1OverflowCount = 0;
        TCCR1B |= (1 << CS12); // Start Timer1 with prescaler 256
    }

    void Timer1_Stop() {
        TCCR1B &= ~(1 << CS12); // Stop Timer1
       // timer1OverflowCount = 0;
    }

    void Timer1_Reset() {
        timer1OverflowCount = 0;
        TCNT1 = 0;
        TCCR1B |= (1 << CS12);
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

    void Timer3_Init() {//Set up timer 3 for ultrasonic sensor
        TCCR3A = 0;
        TCCR3B |= (1 << CS31);   // Prescaler 8
        TIMSK3 |= (1 << TOIE3);  // Enable overflow interrupt
    }

    ISR(TIMER3_OVF_vect) {//Timer 3 overflow interrupt
        ultrasonicTimerCount++;
        if (ultrasonicTimerCount >= 15) { // Trigger every ~15ms
            ultrasonicTimerCount = 0;
            TriggerUltrasonic();
        }
    }

    // Ultrasonic Sensor Functions
    void Ultrasonic_Init() { // Set up pin for ultrasonic sensor
        DDRD |= (1 << ULTRASONIC_TRIGGER); // Trigger pin as output
        DDRB &= ~(1 << ULTRASONIC_ECHO);   // Echo pin as input
        PCICR |= (1 << PCIE0);             // Enable pin change interrupt
        PCMSK0 |= (1 << PCINT4);           // Interrupt on echo pin
    }

    ISR(PCINT0_vect) { //Interrupt and measure pulse width then convert to distance
        static uint16_t edge1 = 0, edge2 = 0;
        if (PINB & (1 << PB4)) {  // Rising edge detected
            edge1 = TCNT3;         // Capture rising edge time
            overflowCount = 0;
        } else {  // Falling edge detected
            edge2 = TCNT3;         // Capture falling edge time
            ultrasonicMeasurementReady = 1;
        }

        // Calculate distance
        uint32_t totalTicks = (overflowCount * 65536UL) + edge2 - edge1;
        uint32_t timeInUs = totalTicks / (F_CPU / 8 / 1000000UL);
        distance = timeInUs / 58;
    }

    void TriggerUltrasonic() { //Trigger 10us TX pulse
        PORTD |= (1 << ULTRASONIC_TRIGGER);
        _delay_us(10);
        PORTD &= ~(1 << ULTRASONIC_TRIGGER);
    }

    uint32_t MeasureDistance() { // return current distance
        return distance; // Convert to cm
    }

    void HX711_setGain(uint8_t gain) {//Set up HX711 gain
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
    // HX711 Functions
    void HX711_Init() { // Initiate HX711 pin
        DDRD &= ~(1 << HX711_DATA); // Data pin as input
        DDRD |= (1 << HX711_CLK);  // Clock pin as output
        PORTD &= ~(1 << HX711_CLK);
    }

    uint32_t HX711_Read() { //Read HX711 24 bit value and convert
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

    float HX711_GetWeight() { //return real weight by minus offset and divided by calibration factor
        int32_t raw = HX711_Read() - hx711Offset;
        return (float)raw / hx711CalibrationFactor;
    }

    void HX711_Calibrate() {// set offset value at start
        hx711Offset = HX711_Read();
    }

    void TriggerWarning() {//Trigger buzzer, LED and set drinkstatus to 1
        TriggerBuzzer();
        drinkstatus = 1;

        //LCD_drawString(55, 50, "DRINK NOW!", 2500, 100);
    }

    void TriggerBuzzer() { //Run buzzer and switch LED
        TCCR2A |= (1 << COM2B1);                // Enable PWM on PD3
        _delay_ms(3000);                         // Wait for the specified duration
        TCCR2A &= ~(1 << COM2B1);               // Disable PWM on PD3

        PORTC |= (1 << PC4);     // Red
        PORTC &= ~(1 << PC5);
    }

    void PrintLCD(float weight, uint8_t drinkstatus) { // Print weight and status on LCD
        char buffer[16];
        sprintf(buffer, "Weight: %.2f", weight);
        LCD_drawString(0, 85, buffer, fColor, bColor);
        LCD_drawString(0, 110, "Unit: g", fColor, bColor);
        if (drinkstatus) {
            LCD_drawString(0, 35, "Drink Now", fColor, bColor);
        } else {
            LCD_drawString(0, 35, "Hydrated!!", fColor, bColor);
        }
    }

    void PrintMiss(float weight) {     //Print missing water amount user has to drink
        char buffer[16];
        sprintf(buffer, "Missing: %.2f", weight);
        if (weight < currentWeight){//if missing weight above current cup weight, require refill
            LCD_drawString(0, 60, buffer, fColor, bColor);
        }
        //else if ((weight>30) &&(weight> currentWeight)){
          //  LCD_drawString(0, 60, "Please refill ASAP", 2500, 000);
        if (weight > 200){
               missing = 0;
            }
        }

    //}

    void PrintOff() {  //Print status off if nothing placed   
            LCD_drawString(0, 10, "Away", fColor, bColor);
    }


    // UART Helpers - Print distance, time counter, status and weight on uart for debugging
    void UART_PrintDistance(uint32_t distance) {
        char buffer[20];
        sprintf(buffer, "Distance: %lu cm", distance);
        UART_putstring(buffer);
        __PRINT_NEW_LINE__
    }

    void UART_PrintTime(uint32_t time) {
        char buffer[20];
        sprintf(buffer, "Time counter: %lu", time);
        UART_putstring(buffer);
        __PRINT_NEW_LINE__
    }

    void UART_PrintStatus(uint8_t status) {
        char buffer[20];
        sprintf(buffer, "Drink status: %u", status);
        UART_putstring(buffer);
        __PRINT_NEW_LINE__
    }

    void UART_PrintWeight(float weight) {
        char buffer[20];
        sprintf(buffer, "Weight: %.2f g", weight);
        UART_putstring(buffer);
        __PRINT_NEW_LINE__
    }
    void UART_PrintpWeight(float weight) {
        char buffer[20];
        sprintf(buffer, "Prev Weight: %.2f g", weight);
        UART_putstring(buffer);
        __PRINT_NEW_LINE__
    }
