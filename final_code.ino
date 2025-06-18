#include <SoftwareSerial.h>

// 점자표시기 설정 (쉬프트 레지스터 핀)
#define Data_PIN  5  // PD5
#define Latch_PIN 6  // PD6
#define Clock_PIN 7  // PD7

// 버튼 및 기타 핀 설정
#define BTN1_BIT PD2  // PORTD2 (CamButton_PIN)
#define BTN2_BIT PD3  // PORTD3 (SendButton_PIN)
#define Buzzer_PIN 4  // PD4 (Buzzer pin)
#define Vibration_PIN 9 // PB1 (Timer1 PWM - OC1A)
#define State_PIN 8     // PB0

// 소프트웨어 시리얼 (Bluetooth)
#define TXD_PIN 10    // PB2
#define RXD_PIN 11    // PB3
SoftwareSerial phonSerial(TXD_PIN, RXD_PIN);

char TextBuffer[64];
int TextIndex = 0;
int CamButtonState = 0;
int LastCamButtonState = LOW;
int SendButtonState = 0;
int LastSendButtonState = LOW;
bool LastBluetoothState = LOW;
unsigned long VibrationStartTime = 0;
bool NowVibrating = false;
int VibrationCount = 0;

// 점자 패턴 배열 (ASCII 32~126)
byte BraillePattern[127] = {
  0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
  0b00000000, 0b00000000, 0b00001110, 0b00001011, 0b00000000, 0b00000000, 0b00010010, 0b00000000, 0b00000000,
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

// UART 초기화 (하드웨어 UART, 38400bps)
void uart_init() {
    UBRR0H = 0;                     // 보레이트 상위 비트
    UBRR0L = 25;                    // 16MHz / (16 * 38400) -1 ≈ 25
    UCSR0A &= ~(1 << U2X0);         // 2배속 비활성화
    UCSR0B = (1 << TXEN0);          // 송신 활성화
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8비트 데이터, 1스톱 비트
}

// 문자열 전송 함수 (레지스터 직접 제어)
void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1 << UDRE0))); // 송신 버퍼 준비 대기
        UDR0 = *str++;
    }
}

// 줄바꿈 포함 문자열 전송
void uart_println(const char* str) {
    uart_print(str);
    while (!(UCSR0A & (1 << UDRE0))); 
    UDR0 = '\n'; // 줄바꿈 추가
}

// 부저 울리는 함수 (PD4에서 1000Hz 수동 토글)
void soundBuzzer() {
    // 1000Hz = 1ms 주기 (0.5ms HIGH, 0.5ms LOW), 500ms 동안
    for (unsigned long i = 0; i < 500; i++) { // 500 cycles for 500ms
        PORTD |= (1 << PORTD4); // PD4 HIGH
        delayMicroseconds(500); // 0.5ms
        PORTD &= ~(1 << PORTD4); // PD4 LOW
        delayMicroseconds(500); // 0.5ms
    }
    PORTD &= ~(1 << PORTD4); // 부저 확실히 OFF
}

void setup() {
    uart_init();                    // 하드웨어 UART 초기화
    phonSerial.begin(38400);        // Bluetooth 초기화
    // Serial.begin(38400);           // 디버깅용 하드웨어 시리얼 (필요 시 주석 해제)

    // 핀 모드 설정
    DDRD |= (1 << DDD5) | (1 << DDD6) | (1 << DDD7) | (1 << DDD4); // dataPin, latchPin, clockPin, buzzerPin 출력
    DDRD &= ~((1 << BTN1_BIT) | (1 << BTN2_BIT));    // buttonPin1, buttonPin2 입력
    DDRB |= (1 << DDB1);  // vibrationPin 출력
    DDRB &= ~(1 << DDB0); // statePin 입력
    PORTB |= (1 << PORTB0); // statePin 풀업 저항 활성화
    PORTB &= ~(1 << PORTB1); // 진동 모터 초기 OFF
    PORTD &= ~(1 << PORTD4); // 부저 초기 OFF

    // PWM 설정 (Timer1, Fast PWM, 8-bit, 250Hz, 70% 듀티 사이클)
    TCCR1A = (1 << COM1A1) | (1 << WGM10); // Fast PWM 8-bit, 비반전 모드
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // Prescaler 64, 250Hz
    OCR1A = 178; // 70% 듀티 사이클 (255 * 0.7 ≈ 178)
    TCCR1A &= ~(1 << COM1A1); // PWM 처음에는 비활성화

    // 쉬프트 레지스터 초기화 (모든 솔레노이드 OFF)
    shiftOutBraille(0x00);
}

void loop() {
    // 블루투스 연결 상태 확인 및 PWM 진동
    bool currentBtState = (PINB >> PINB0) & 0x01; // 수정: PIND -> PINB
    static unsigned long lastStateChange = 0;
    if (currentBtState != LastBluetoothState && (millis() - lastStateChange > 50)) {
        if (currentBtState == HIGH) {
            VibrationCount = 0;
            NowVibrating = true;
            VibrationStartTime = millis();
            TCCR1A |= (1 << COM1A1); // PWM 활성화
            uart_println("[DEBUG] Bluetooth connected, vibration ON");
        } else {
            uart_println("[DEBUG] Bluetooth disconnected, vibration OFF");
            TCCR1A &= ~(1 << COM1A1); // PWM 비활성화
            PORTB &= ~(1 << PORTB1); // 진동 모터 OFF
            NowVibrating = false;
        }
        lastStateChange = millis();
    }
    if (NowVibrating) {
        unsigned long currentTime = millis();
        if (VibrationCount < 2) {
            if (currentTime - VibrationStartTime >= 500) { // 0.5초
                TCCR1A &= ~(1 << COM1A1); // PWM 비활성화
                uart_println("[DEBUG] Vibration pulse OFF");
                if (currentTime - VibrationStartTime >= 700) { // 0.2초 대기
                    VibrationCount++;
                    if (VibrationCount < 2) {
                        TCCR1A |= (1 << COM1A1); // PWM 재활성화
                        VibrationStartTime = currentTime;
                        uart_println("[DEBUG] Vibration pulse ON");
                    }
                }
            }
        } else {
            TCCR1A &= ~(1 << COM1A1); // PWM 비활성화
            PORTB &= ~(1 << PORTB1); // 진동 모터 OFF
            NowVibrating = false;
            uart_println("[DEBUG] Vibration sequence complete");
        }
    }
    LastBluetoothState = currentBtState;

    // 버튼 1 처리 ("camera" 전송)
    CamButtonState = (PIND & (1 << BTN1_BIT)) ? 1 : 0;
    if (CamButtonState && !LastCamButtonState) {
        phonSerial.println("camera");
        uart_println("[DEBUG] Button1: 'camera' sent");
        delay(200);
    }
    LastCamButtonState = CamButtonState;

    // 버튼 2 처리 ("send" 전송 및 점자 출력 또는 부저)
    SendButtonState = (PIND & (1 << BTN2_BIT)) ? 1 : 0;
    if (SendButtonState && !LastSendButtonState) {
        phonSerial.println("send");
        uart_println("[DEBUG] Button2: 'send' sent");
        if (TextIndex > 0) {
            TextBuffer[TextIndex] = '\0';
            printBraille(TextBuffer);
            TextIndex = 0;
        }
        delay(50);
    }
    LastSendButtonState = SendButtonState;

    // Bluetooth 수신 처리
    if (phonSerial.available()) {
        char c = phonSerial.read();
        if (c >= 32 && c <= 126) {
            if (TextIndex < sizeof(TextBuffer) - 1) {
                TextBuffer[TextIndex++] = c;
            }
        } else if (c == '\r' || c == '\n') {
            if (TextIndex > 0) {
                TextBuffer[TextIndex] = '\0';
                uart_print("[BT RECV] ");
                uart_println(TextBuffer);
                printBraille(TextBuffer);
                TextIndex = 0;
            } else {
                // 빈 텍스트 수신 시 부저 울림
                uart_println("[BT RECV] Empty text received, sounding buzzer");
                soundBuzzer();
            }
        }
    }
}

// 쉬프트 레지스터로 점자 패턴 전송
void shiftOutBraille(byte pattern) {
    PORTD &= ~(1 << PORTD6); // LatchPin LOW
    for (int i = 5; i >= 0; i--) { // 6비트 처리 (점자용)
        if (pattern & (1 << i)) {
            PORTD |= (1 << PORTD5); // DataPin HIGH
        } else {
            PORTD &= ~(1 << PORTD5); // DataPin LOW
        }
        PORTD |= (1 << PORTD7); // ClockPin HIGH
        PORTD &= ~(1 << PORTD7); // ClockPin LOW
    }
    PORTD |= (1 << PORTD6); // LatchPin HIGH
    PORTD &= ~(1 << PORTD6); // LatchPin LOW
}

// ASCII 코드를 점자로 변환 및 출력
void ascii_braille(int code) {
    if (code >= 32 && code < 127) {
        byte pattern = BraillePattern[code];
        shiftOutBraille(pattern);
    }
}

// 텍스트를 점자로 출력
void printBraille(char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch >= 32 && ch < 127) {
            ascii_braille(ch);
            delay(500); // 점자 표시 시간 500ms
            shiftOutBraille(0x00); // 모든 점 OFF
            delay(250); // 점자 OFF 시간 250ms
        }
    }
}