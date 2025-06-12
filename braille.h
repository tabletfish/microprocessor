#define MAX_BRAILLE_NO 10

class braille
{
  public:
    uint8_t data[MAX_BRAILLE_NO]; // 점자 표시기 저장 데이타 최대 10개까지 가능
    braille(int data_pin, int latch_pin, int clock_pin, int no);
    void begin();
    void on(int no, int index);
    void off(int no, int index);
    void refresh();
    void all_off();

  private:
    int braille_no;
    int dataPin;
    int latchPin;
    int clockPin;
};

braille::braille(int data_pin, int latch_pin, int clock_pin, int no)
{
  dataPin = data_pin;
  latchPin = latch_pin;
  clockPin = clock_pin;
  braille_no = no;
}

void braille::begin()
{
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
}

void braille::on(int no, int index)
{
  data[braille_no - no + -1] = data[braille_no - no - 1] | ( 0b00000001 << index );
}

void braille::off(int no, int index)
{
  data[braille_no - no + -1] = data[braille_no - no - 1] & ~( 0b00000001 << index );
}

void braille::refresh()
{
  digitalWrite(latchPin, LOW);
  for ( int i = 0; i < braille_no; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, data[i]);
  }
  digitalWrite(latchPin, HIGH);
}

void braille::all_off()
{
  for ( int i = 0; i < MAX_BRAILLE_NO; i++)
  {
    data[i] = 0b00000000;
  }
}
