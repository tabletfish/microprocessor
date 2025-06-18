#define SERVO_PIN 1    
#define BUTTON_PIN 2   

// Servo PWM
#define PWM_TOP 39999  // 20 ms period (50 Hz)
#define UP_POSITION 2220   // ~1.11 ms pulse (20 degrees)
#define DOWN_POSITION 4440 // ~2.22 ms pulse (160 degrees)

// 디바운스
#define DEBOUNCE_DELAY 50  
#define DELAY_TIME 500     

// 디바운스 전역변수
volatile uint8_t buttonState = 0;
volatile uint8_t lastButtonState = 1; 
volatile unsigned long lastDebounceTime = 0;

unsigned long millis() {
    return (TCNT0 * 256UL + TCNT0) / 16; 
}

void setup() {
    TCCR1A = (1 << COM1A1) | (1 << WGM11); // Non-inverting PWM, Fast PWM mode (WGM11:10 = 01)
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // Fast PWM (WGM13:12 = 11), prescaler = 8
    ICR1 = PWM_TOP; 
    OCR1A = DOWN_POSITION; 
    
    
    DDRB |= (1 << SERVO_PIN);
    
   
    DDRD &= ~(1 << BUTTON_PIN); 
    PORTD |= (1 << BUTTON_PIN); 
    

    TCCR0A = (1 << WGM01) | (1 << WGM00); // Fast PWM mode
    TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler = 64
}

void loop() {
    
    uint8_t reading = (PIND & (1 << BUTTON_PIN)) ? 1 : 0;
    
    // Debouncing
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != buttonState) {
            buttonState = reading;
            
            
            if (buttonState == 0) {
                // Move servo to up position
                OCR1A = UP_POSITION;
                _delay_ms(DELAY_TIME);
                
                // Move servo to down position
                OCR1A = DOWN_POSITION;
                _delay_ms(DELAY_TIME);
            }
        }
    }
    
    lastButtonState = reading;
}

int main(void) {
    setup();
    while (1) {
        loop();
    }
    return 0;
}