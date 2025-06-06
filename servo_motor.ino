#include <avr/io.h>
#include <avr/interrupt.h>

#define Servo_PIN 1    // PB1 (Arduino Pin 9)
#define ServoButton_PIN 2   // PD2 (Arduino Pin 2)

// Servo PWM settings (for 16 MHz clock, prescaler = 8)
#define PwmTopValue 39999  // 20 ms period (50 Hz)
#define ServoUp 2220   // ~1.11 ms pulse (20 degrees)
#define ServoDown 4440 // ~2.22 ms pulse (160 degrees)

// Debouncing settings
#define Debounce 50  // Debounce Delay in ms
#define Delay 500     // Delay for servo movement in ms

// Global variables
volatile uint8_t ButtonState = 0;
volatile uint8_t LastButtonState = 1;
volatile unsigned long LastDebounceTime = 0;

// --- millis용 전역 변수 ---
volatile unsigned long Timer0_millis = 0;

// 정확한 1ms 추적을 위한 오버플로우 인터럽트
ISR(TIMER0_OVF_vect) {
    // 1 overflow = 256 ticks
    // tick = 1 / (16MHz / 64) = 4µs
    // 256 ticks = 1024µs = 1.024ms

    Timer0_millis++;
}

// 정확한 millis() 함수
unsigned long millis() {
    unsigned long ms;
    cli(); // 인터럽트 중단
    ms = Timer0_millis;
    sei(); // 다시 인터럽트 허용
    return ms;
}

void setup() {
    // Timer1 for Servo
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // prescaler = 8
    ICR1 = PwmTopValue;
    OCR1A = ServoDown;
    DDRB |= (1 << Servo_PIN);

    // Button input
    DDRD &= ~(1 << ServoButton_PIN);
    PORTD |= (1 << ServoButton_PIN);

    // Timer0 setup for millis (normal mode + overflow interrupt)
    TCCR0A = 0; // Normal mode
    TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler = 64
    TIMSK0 = (1 << TOIE0); // Enable overflow interrupt

    sei(); // Enable global interrupts
}

void loop() {
    uint8_t reading = (PIND & (1 << ServoButton_PIN)) ? 1 : 0;

    if (reading != LastButtonState) {
        LastDebounceTime = millis();
    }

    if ((millis() - LastDebounceTime) > Debounce) {
        if (reading != ButtonState) {
            ButtonState = reading;

            if (ButtonState == 0) {
                OCR1A = ServoUp;
                unsigned long start = millis();
                while (millis() - start < Delay);

                OCR1A = ServoDown;
                start = millis();
                while (millis() - start < Delay);
            }
        }
    }

    LastButtonState = reading;
}

int main(void) {
    setup();
    while (1) {
        loop();
    }
    return 0;
}