#include <SoftwareSerial.h>

#define Data_PIN  5 
#define Latch_PIN 6 
#define Clock_PIN 7  

#define CamButton_PIN PD2 
#define SendButton_PIN PD3  
#define Vibration_PIN 9 
#define State_PIN 8    

#define TXD_PIN 10  
#define RXD_PIN 11   
SoftwareSerial phonSerial(TXD_PIN, RXD_PIN);

char TextBuffer[64];
int TextIndex = 0;
int CamButtonState = 0;
int LastCamButtonState = HIGH; 
int SendButtonState = 0;
int LastSendButtonState = HIGH;
bool LastBluetoothState = LOW;
unsigned long VibrationStartTime = 0;
bool NowVibrating = false;
int VibrationCount = 0;
volatile bool EmptyTextVibration = false;
volatile int VibrationPulseCount = 0;
volatile unsigned long VibrationTimerCount = 0;
volatile bool VibrationCompleteFlag = false;

// 점자 패턴 배열
byte BraillePattern[127] = {
  0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00000000, 0b00001011, 0b00000000, 0b00000000, 0b00010010, 0b00000000, 0b00000000,
  0b00001011, 0b00000001, 0b00100001, 0b00001001, 0b00000100, 0b00000110, 0b00001101, 0b00010101, 0b00011100, 0b00100000,
  0b00101000, 0b00110000, 0b00110100, 0b00100100, 0b00111000, 0b00111100, 0b00101100, 0b00011000, 0b00011100, 0b00000100,
  0b00000101, 0b00000100, 0b00001100, 0b00000111, 0b00001011, 0b00000000, 0b00100000, 0b00101000, 0b00110000, 0b00110100,
  0b00100100, 0b00111000, 0b00111100, 0b00101100, 0b00011000, 0b00011100, 0b00100010, 0b00101010, 0b00110010, 0b00110110,
  0b00100110, 0b00111010, 0b00111110, 0b00101110, 0b00011010, 0b00011110, 0b00100011, 0b00101011, 0b00011101, 0b00110011,
  0b00110111, 0b00100111, 0b00001011, 0b00010000, 0b00000101, 0b00000000, 0b00000000, 0b00100000, 0b00101000, 0b00110000,
  0b00110100, 0b00100100, 0b00111000, 0b00111100, 0b00101100, 0b00011000, 0b00011100, 0b00100010, 0b00101010, 0b00110010,
  0b00110110, 0b00100110, 0b00111010, 0b00111110, 0b00101110, 0b00011010, 0b00011110, 0b00100011, 0b00101011, 0b00011101,
  0b00110011, 0b00110111, 0b00100111, 0b00001011, 0b00000000, 0b00000100, 0b00000000
};

// UART 초기화
void uart_init() {
    UBRR0H = 0;
    UBRR0L = 25; // 16MHz / (16 * 38400) -1 ≈ 25
    UCSR0A &= ~(1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = *str++;
    }
}

void uart_println(const char* str) {
    uart_print(str);
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = '\n';
}

// Timer2 인터럽트
void initTimer2() {
    TCCR2A = (1 << WGM21); // CTC 
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); 
    OCR2A = 156; // 16MHz / 1024 / 100 = 156 
    TIMSK2 = (1 << OCIE2A); 
}

// Timer2 ISR
ISR(TIMER2_COMPA_vect) {
    if (EmptyTextVibration) {
        VibrationTimerCount++;
        if (VibrationPulseCount < 3) {
            if (VibrationTimerCount <= 30) { 
                TCCR1A |= (1 << COM1A1); 
            } else if (VibrationTimerCount <= 50) { 
                TCCR1A &= ~(1 << COM1A1); 
            } else {
                VibrationPulseCount++;
                VibrationTimerCount = 0;
                if (VibrationPulseCount < 3) {
                    TCCR1A |= (1 << COM1A1); 
                }
            }
        } else {
            TCCR1A &= ~(1 << COM1A1); 
            PORTB &= ~(1 << PORTB1); 
            EmptyTextVibration = false;
            VibrationPulseCount = 0;
            VibrationTimerCount = 0;
            VibrationCompleteFlag = true;
        }
    }
}

void setup() {
    uart_init();
    phonSerial.begin(38400); 

    // 핀 모드
    DDRD |= (1 << DDD5) | (1 << DDD6) | (1 << DDD7); 
    DDRD &= ~((1 << CamButton_PIN) | (1 << SendButton_PIN));
    PORTD |= (1 << CamButton_PIN) | (1 << SendButton_PIN);
    DDRB |= (1 << DDB1); 
    DDRB &= ~(1 << DDB0); 
    PORTB |= (1 << PORTB0);
    PORTB &= ~(1 << PORTB1);

    // PWM
    TCCR1A = (1 << WGM10); 
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); 
    OCR1A = 178; 
    TCCR1A &= ~(1 << COM1A1); 

 
    initTimer2();

   
    shiftOutBraille(0x00);

    sei();
}

void loop() {
   
    bool currentBtState = (PINB >> PINB0) & 0x01;
    static unsigned long lastStateChange = 0;
    unsigned long currentTime = millis();
    if (currentBtState != LastBluetoothState && (currentTime - lastStateChange > 50)) {
        if (currentBtState == HIGH) {
            if (!EmptyTextVibration) {
                VibrationCount = 0;
                NowVibrating = true;
                VibrationStartTime = currentTime;
                TCCR1A |= (1 << COM1A1);
                uart_println("페어링 진동시작");
            }
        } else {
            if (!EmptyTextVibration) {
                uart_println("연결끊김-진동끝");
                TCCR1A &= ~(1 << COM1A1);
                PORTB &= ~(1 << PORTB1);
                NowVibrating = false;
            }
        }
        lastStateChange = currentTime;
    }

    if (NowVibrating && !EmptyTextVibration) {
        if (currentTime - VibrationStartTime >= 500) {
            TCCR1A &= ~(1 << COM1A1);
            uart_println("진동 끝");
            if (currentTime - VibrationStartTime >= 700) {
                VibrationCount++;
                if (VibrationCount < 2) {
                    TCCR1A |= (1 << COM1A1);
                    VibrationStartTime = currentTime;
                    uart_println("진동 시작");
                } else {
                    TCCR1A &= ~(1 << COM1A1);
                    PORTB &= ~(1 << PORTB1);
                    NowVibrating = false;
                    uart_println("진동 완료");
                }
            }
        }
    }
    LastBluetoothState = currentBtState;

    // 버튼1
    CamButtonState = (PIND & (1 << CamButton_PIN)) ? HIGH : LOW;
    if (CamButtonState == LOW && LastCamButtonState == HIGH) {
        phonSerial.println("camera");
        uart_println("버튼눌림");
        delay(200);
    }
    LastCamButtonState = CamButtonState;

    // 버튼 2
    SendButtonState = (PIND & (1 << SendButton_PIN)) ? HIGH : LOW;
    if (SendButtonState == LOW && LastSendButtonState == HIGH) {
        phonSerial.println("send");
        uart_println("버튼눌림");
        if (TextIndex > 0) {
            TextBuffer[TextIndex] = '\0';
            printBraille(TextBuffer);
            TextIndex = 0;
        }
        delay(50);
    }
    LastSendButtonState = SendButtonState;

    // Bluetooth
    if (phonSerial.available()) {
        char c = phonSerial.read();
        if (c >= 32 && c <= 126) {
            if (TextIndex < sizeof(TextBuffer) - 1) {
                TextBuffer[TextIndex++] = c;
            }
        } else if (c == '\r' || c == '\n') {
            if (TextIndex > 0) {
                TextBuffer[TextIndex] = '\0';
                uart_print("보낸문자:");
                uart_println(TextBuffer);
                printBraille(TextBuffer);
                TextIndex = 0;
            } else {
                if (!NowVibrating && !EmptyTextVibration) {
                    uart_println("blutooth connect");
                    EmptyTextVibration = true;
                    VibrationPulseCount = 0;
                    VibrationTimerCount = 0;
                    TCCR1A |= (1 << COM1A1);
                }
            }
        }
    }

    // 진동
    if (VibrationCompleteFlag) {
        uart_println("Text empty");
        VibrationCompleteFlag = false;
    }
}

// 쉬프트 레지스터
void shiftOutBraille(byte pattern) {
    PORTD &= ~(1 << PORTD6); 
    for (int i = 5; i >= 0; i--) {
        if (pattern & (1 << i)) {
            PORTD |= (1 << PORTD5);
        } else {
            PORTD &= ~(1 << PORTD5);
        }
        PORTD |= (1 << PORTD7);
        delayMicroseconds(20);
        PORTD &= ~(1 << PORTD7);
    }
    PORTD |= (1 << PORTD6);
    delay(2); 
    PORTD &= ~(1 << PORTD6);

   
    uart_print("pattern: ");
    for (int i = 5; i >= 0; i--) {
        uart_print((pattern & (1 << i)) ? "1" : "0");
    }
    uart_println("");
}

//문자를 패턴으로 전환
void ascii_braille(int code) {
    if (code >= 32 && code < 127) {
        byte pattern = BraillePattern[code];
        shiftOutBraille(pattern);
    }
}

// 텍스트를 점자로
void printBraille(const char *text) {
    for (size_t i = 0; text[i]; i++) {
        char ch = text[i];
        if (ch >= 32 && ch < 127) {
            ascii_braille(ch);
            delay(500); /
            shiftOutBraille(0x00); 
            delay(250); 
        }
    }
}