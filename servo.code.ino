#define Servo_PIN PB1           
#define ServoButton_PIN PD2    

// pwm
#define PwmTopValue 39999
#define ServoUp 2220
#define ServoDown 4440

#define Debounce 50
#define Delay 500

volatile uint8_t ButtonState = 0;
volatile uint8_t LastButtonState = 1;
volatile unsigned long LastDebounceTime = 0;
volatile unsigned long Timer0_millis = 0;

char uart_tx_buffer[64];

// UART 설정
void uart_init() {
    UBRR0H = 0;
    UBRR0L = 103;
    UCSR0A &= ~(1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// 
void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

// 문자열
void uart_print(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

// 줄바꿈 포함 문자열 전송
void uart_println(const char* str) {
    uart_print(str);
    uart_putc('\n');
}


// 인터럽트
ISR(TIMER0_OVF_vect) {
    Timer0_millis++;
}


unsigned long millis() {
    unsigned long ms;
    cli();
    ms = Timer0_millis;
    sei();
    return ms;
}

void setup() {
    
    uart_init();
    uart_println("서브모터제어시작");

    // Timer1 
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = PwmTopValue;
    OCR1A = ServoDown;
    DDRB |= (1 << Servo_PIN);
    DDRD &= ~(1 << ServoButton_PIN);
    PORTD |= (1 << ServoButton_PIN); 

    // Timer0 
    TCCR0A = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);
    TIMSK0 = (1 << TOIE0);

    sei();
}

void loop() {
    uint8_t raw_reading = (PIND >> ServoButton_PIN) & 0x01;
    char debug_msg[32];
    sprintf(debug_msg, "Raw Pin %d State: %d", ServoButton_PIN, raw_reading);
    uart_println(debug_msg);

    if (raw_reading != LastButtonState) {
        LastDebounceTime = millis();
    }

    if ((millis() - LastDebounceTime) > Debounce) {
        if (raw_reading != ButtonState) {
            ButtonState = raw_reading;
            if (ButtonState == 0) {
                uart_println("버튼 눌림(디버깅용)");
                
                OCR1A = ServoUp;
                unsigned long start = millis();
                while (millis() - start < Delay);

                OCR1A = ServoDown;
                start = millis();
                while (millis() - start < Delay);
            } else {
                uart_println("버튼 떼어짐(디버깅용)");
            }
        }
    }

    LastButtonState = raw_reading;
    _delay_ms(100);
}

int main(void) {
    setup();
    while (1) {
        loop();
    }
    return 0;
}