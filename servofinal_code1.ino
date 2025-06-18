#define SERVO_PIN 1
#define BUTTON_PIN 2

#define PWM_TOP 39999  // 20 ms period (50 Hz)
#define UP_STATE 2220   // ~1.11 ms pulse (20 degrees)
#define DOWN_STATE 4440 // ~2.22 ms pulse (160 degrees)

#define DEBOUNCE_DELAY 50
#define DELAY 500

volatile uint8_t buttonState = 0;
volatile uint8_t lastButtonState = 1;
volatile unsigned long lastDebounceTime = 0;

volatile unsigned long timer0_millis = 0;

unsigned long millis() {
    unsigned long ms;
    cli(); 
    ms = timer0_millis;
    sei();
    return ms;
}


ISR(TIMER0_OVF_vect) {
    timer0_millis++;
}

void setup() {

    TCCR1A = (1 << COM1A1) | (1 << WGM11); // Fast PWM
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // prescaler = 8
    ICR1 = PWM_TOP;
    OCR1A = DOWN_STATE;
    DDRB |= (1 << SERVO_PIN); //SERVO_PIN 1로 define

    DDRD &= ~(1 << BUTTON_PIN); 
    PORTD |= (1 << BUTTON_PIN); 

    TCCR0A = 0;                         // Normal mode (millis 용 timer)
    TCCR0B = (1 << CS01) | (1 << CS00); // prescaler = 64
    TIMSK0 |= (1 << TOIE0);             //interrupt enable

    sei(); 
}

void loop() {
    uint8_t reading = ((PIND & (1 << BUTTON_PIN)) ? 1 : 0);

    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != buttonState) {
            buttonState = reading;

            if (buttonState == 0) {

                OCR1A = UP_STATE;    //2220 (1.11ms) define(20도)
                _delay_ms(DELAY);

                OCR1A = DOWN_STATE;  //4440 (2.22ms) define(160도)
                _delay_ms(DELAY);
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