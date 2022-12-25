#include "Arduino.h"

#include <SPI.h>
#include <MIDI.h>

#define REFERENCE_FREQUENCY 2000 // Desired update frequency in Hz

#define INVALID_NOTE 128
#define NOTE_MEM_SIZE 4

// Precalc timer settings
#define TIMER_COUNT F_CPU / REFERENCE_FREQUENCY
#define PRESCALE_VALUE 32

// DAC settings
#define DAC_GAIN_2X 0
#define DAC_GAIN_1X 1
#define DAC_CHANNEL_A 0
#define DAC_CHANNEL_B 1

// MIDI Controller #
#define MIDI_CC_MODWHEEL 1
#define MIDI_CC_AFTERTOUCH 4
struct dac_cv
{
	int32_t target = 0;
	int32_t current = 0;
};

// DAC CV (12-bit) outs
dac_cv CV_1, CV_2;

// PWM CV (8-bit) outs
#define CV_3 OCR1A // ATmega pin 15
#define CV_4 OCR0A // ATmega pin 12
#define CV_5 OCR1B // ATmega pin 16
#define CV_6 OCR0B // ATmega pin 11

#define CV_VELOCITY CV_3
#define CV_ATOUCH CV_4
#define CV_MODWHEEL CV_5
#define CV_CUSTOM CV_6

// TODO: More gates?
//#define GATE_BIT 0x02 // GATE 4
//#define GATE_BIT 0x20 // GATE 3
#define GATE_BIT 0x08 // GATE 2
//#define GATE_BIT 0x04 // GATE 1

#define CLOCK_BIT 0x20

#define DAC_SS_BIT 0x02

uint8_t note_list[NOTE_MEM_SIZE];
int8_t num_playing_notes = 0;
uint8_t clock_counter = 0;
uint8_t midi_channel = 1; // TODO: Make this adjustable, auto-detect on first incoming note?
uint8_t glide_amount = 0;
int16_t bend = 0;

uint8_t playing = 0;

MIDI_CREATE_DEFAULT_INSTANCE();

void setup()
{
	// DAC SS pin
	pinMode(8, OUTPUT);

	// GATE outputs
  pinMode(A0, INPUT);
	pinMode(A1, OUTPUT);
	pinMode(A2, OUTPUT);
	pinMode(A3, OUTPUT);
  pinMode(A5, OUTPUT);

	MIDI.setHandleNoteOn(midi_note_on);
	MIDI.setHandleNoteOff(midi_note_off);
	MIDI.setHandleClock(midi_clock);
  MIDI.setHandleStart(midi_start);
  MIDI.setHandleStop(midi_stop);
	MIDI.setHandleControlChange(midi_cc);
	MIDI.setHandlePitchBend(midi_pitchbend);

	MIDI.begin(0);

	SPI.begin();

	//TIMER0: (PWM CV outputs)
	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);

	TCCR0A = 0; // Reset register

	TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM00) | _BV(WGM01);
	TCCR0B = _BV(CS00);

	OCR0A = 0;
	OCR0B = 0;

	//TIMER1: (PWM CV outputs)
	pinMode(9, OUTPUT);
	pinMode(10, OUTPUT);

	TCCR1A = 0;
	TCCR1B = 0;

	TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS10);

	OCR1A = 0;
	OCR1B = 0;

	//TIMER2: (Logic/update)

	TCCR2A = 0; // Reset register
	TCCR2B = 0;

	// Set CTC mode
	TCCR2A = _BV(WGM21);
	TCCR2B = _BV(CS20) | _BV(CS21);

	// Set Timer2 timeout
	OCR2A = (TIMER_COUNT / PRESCALE_VALUE) - 1;

	TCNT2 = 0; // Reset counter

	TIMSK2 =_BV(OCIE2A); // Enable timer compare interrupt

//	OCR2A = 255; // Set CTC
//	OCR2B = 64; // Set PWM duty cycle

	num_playing_notes = 0;

  // Set CV_2 (pitchbend) to default midrange value
  CV_2.target = CV_2.current = 2048;
}

void midi_start()
{
  playing = 1;
  clock_counter = 24; // Initialize the counter on the first beat
  num_playing_notes = 0;
  // Make sure gates are off
  PORTC &= ~CLOCK_BIT;
  PORTC &= ~GATE_BIT;
}

void midi_stop()
{
  playing = 0;
  // Reset clock counter and note memory
  num_playing_notes = 0;
  clock_counter = 0;

  // Turn off gates
  PORTC &= ~CLOCK_BIT;
  PORTC &= ~GATE_BIT;
}

void midi_note_on(byte channel, byte note, byte velocity)
{
  // Auto-detect channel on first incoming note and set it,
  // then ignore messages on other channels
  /*
  if(midi_channel != channel) {
    if(midi_channel == 0) {
      midi_channel = channel;
    } else {
      return;
    }
  }
  */
	CV_VELOCITY = velocity << 1;
	PORTC |= GATE_BIT;
  
  note_list[num_playing_notes++] = note;
  
	CV_1.target = uint32_t(note) << 24; // Convert 7-bit MIDI message to 32-bit internal format

   // Reset glide if no notes were playing
  if (num_playing_notes == 1) {
    CV_1.current = CV_1.target;
  }
};

void midi_note_off(byte channel, byte note, byte velocity)
{
  uint8_t found = 0;
  
  // Find the note that was turned off
  for (int i = 0; i < num_playing_notes; i++) {
    if (note_list[i] == note) {
      found = 1;
    }
    // We found the note, so move everything above it down by one.
    if (found && i < num_playing_notes - 1) {
      note_list[i] = note_list[i + 1];
    }
  }
  num_playing_notes--;

  // Play the top note in list
  if (num_playing_notes > 0) {
    CV_1.target = uint32_t(note_list[num_playing_notes - 1]) << 24;
  } 

  // Turn GATE off
  else if (num_playing_notes <= 0)
	{
		num_playing_notes = 0;
		PORTC &= ~GATE_BIT;
	}
}

void midi_cc(byte channel, byte CC, byte value)
{
	if (channel == midi_channel && CC == MIDI_CC_MODWHEEL)
		CV_MODWHEEL = value << 1;
	else if (channel == midi_channel && CC == MIDI_CC_AFTERTOUCH)
		CV_ATOUCH = value << 1;
}

// NOTE: MIDI sends 24 clock pulses per quarter note. That's 6 pulses per 16th, or 96 pulses per beat
void midi_clock()
{
  // The midi_start() callback starts at 24, so the first tick is on the first beat
   
  // If we've counted 24 pulses (1/4th note), toggle the clock pin high for the next pulse
	if (clock_counter % 24 == 0 && playing) {
		PORTC |= CLOCK_BIT;
    clock_counter = 0;
  // Keep the pin normally low
  } else {
    PORTC &= ~CLOCK_BIT;
  }

  // If we've counted a 16th (every 6 pulses), toggle the 16th pin high for the next pulse
  if (clock_counter % 6 == 0 && playing) {
    // TODO: Toggle 16th pin
  }
  else {
    // TODO: Keep the 16th pin normally low
  }

  clock_counter++;
}

//NOTE: Pitchbend range is -8192 -> 8191 (14-bit MIDI message)
void midi_pitchbend(byte channel, int value)
{
	CV_2.target = (value + 8192) >> 2; // Make unsigned and shift to 12-bit for DAC out
	bend = value >> 6; // Shift to 8-bit internal resolution (for reasonable pbend range)
}

// TODO/IDEA: DAC config for each channel could be held in a variable and OR:ed with the data instead of all these shifts every tick?
inline void write_dac(uint16_t value, uint8_t channel)
{
	PORTB &= ~B00000001; // Set SS pin low
	SPI.transfer16(((channel << 7 | DAC_GAIN_2X << 5 | 1 << 4) << 8) | value);
	PORTB |= B00000001; // Set SS pin high
}

// TODO: Profile ISR, it needs to work at a few KHz at least
ISR(TIMER2_COMPA_vect) // 2KHz update freq.
{
  // TODO: HW switch for bend applied to pitch
	write_dac((CV_1.current >> 19)/* + bend */,  DAC_CHANNEL_A); // Shift back to 12 bits for DAC output and add pitchbend
	write_dac(CV_2.target, DAC_CHANNEL_B);

	// Do note glide
  int32_t glide = 1 + (analogRead(A0) << 4);
  CV_1.current += (CV_1.target - CV_1.current) / glide;
}

void loop()
{
	MIDI.read();
}
