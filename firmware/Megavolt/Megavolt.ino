#include "Arduino.h"

#include <SPI.h>
#include <MIDI.h>

#define REFERENCE_FREQUENCY 2000 // Desired update frequency in Hz

// Precalc timer settings
#define TIMER_COUNT F_CPU / REFERENCE_FREQUENCY
#define PRESCALE_VALUE 32

// DAC settings
#define DAC_GAIN_2X 0
#define DAC_GAIN_1X 1
#define DAC_CHANNEL_A 0
#define DAC_CHANNEL_B 1

struct dac_cv
{
	int32_t target = 0;
	int32_t current = 0;
};

// DAC CV (12-bit) outs
dac_cv CV_1, CV_2;

// PWM CV (8-bit) outs
#define CV_3 OCR1A // ATmega pin 6
#define CV_4 OCR1B // ATmega pin 5
#define CV_5 OCR0A // ATmega pin 9
#define CV_6 OCR0B // ATmega pin 10

// TODO: More gates?
#define GATE_BIT 0x08
#define CLOCK_BIT 0x10

#define DAC_SS_BIT 0x02

uint8_t note_list[256];
uint8_t num_playing_notes = 0;
uint8_t clock_counter = 0;
uint8_t midi_channel = 1; // TODO: Make this adjustable, auto-detect on first incoming note?
uint8_t glide_amount = 0;
int16_t bend = 0;

MIDI_CREATE_DEFAULT_INSTANCE();

void setup()
{
	// DAC SS pin
	pinMode(8, OUTPUT);

	// GATE outputs
	pinMode(A3, OUTPUT);
	pinMode(A4, OUTPUT);

	MIDI.setHandleNoteOn(midi_note_on);
	MIDI.setHandleNoteOff(midi_note_off);
	MIDI.setHandleClock(midi_clock);
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
}

int find_last_note()
{
	return 0; // TODO: Implement
}

void midi_note_on(byte channel, byte note, byte velocity)
{
	CV_6 = velocity << 1;
	PORTC |= GATE_BIT;

	CV_1.target = uint32_t(note) << 24; // Convert 7-bit MIDI message to 32-bit internal format

	// Reset glide
	if (num_playing_notes <= 0)
		CV_1.current = CV_1.target;

	num_playing_notes++;

};

void midi_note_off(byte channel, byte pitch, byte velocity)
{
	num_playing_notes--;

	if (num_playing_notes <= 0)
	{
		num_playing_notes = 0;
		PORTC &= ~GATE_BIT;
	}
}

void midi_cc(byte channel, byte CC, byte value)
{
	if (channel == midi_channel && CC == 01)
		CV_3 = value << 1;
	else if (channel == midi_channel && CC == 04)
		CV_4 = value << 1;
}

void midi_clock()
{
	clock_counter++;
	if (clock_counter % 12 == 0)
	{
		PINC |= CLOCK_BIT;
	}
}

//NOTE: Pitchbend range is -8192 -> 8191 (14-bit MIDI message)
void midi_pitchbend(byte channel, int value)
{
//	CV_2.target = (value + 8192); // NOTE/TODO: Use this for hardware pitchbend?
	bend = value >> 6; // Shift to 8-bit internal resolution (for reasonable pbend range)
}

inline void write_dac(uint16_t value, uint8_t channel)
{
	PORTB &= ~B00000001; // Set SS pin low
	SPI.transfer16(((channel << 7 | DAC_GAIN_2X << 5 | 1 << 4) << 8) | value);
	PORTB |= B00000001; // Set SS pin high
}

// TODO: Profile ISR, it needs to work at a few KHz at least
ISR(TIMER2_COMPA_vect) // 2KHz update freq.
{
	write_dac((CV_1.current >> 19) + bend,  DAC_CHANNEL_A); // Shift back to 12 bits for DAC output and add pitchbend
	write_dac(CV_2.target, DAC_CHANNEL_B);

	// Do note glide
	CV_1.current += int32_t(CV_1.target - CV_1.current) / (analogRead(A0) + 1);
}

void loop()
{
	MIDI.read();
}

