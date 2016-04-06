
// For some reason these need to be included here or Arduino won't find them.
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>

#include "synth.h"
#include "fscale.h"

#include "Arduino.h"

float lfo_depth;
float lfo_rate;

// Our main synth object.
Synth synth;

void setup() {
  Serial.begin(115200);
  // wait for Arduino Serial Monitor
  delay(1000);

  synth.setup();

  setupInputs();

  // Midi setup.
  usbMIDI.setHandleNoteOff(OnNoteOff);
  usbMIDI.setHandleNoteOn(OnNoteOn);
  usbMIDI.setHandleControlChange(OnControlChange);
}

unsigned long last_time = millis();
void loop() {

  // Change this to if(1) for measurement output every 2 seconds
  if (1) {
    if (millis() - last_time >= 2000) {
      Serial.print("Proc = ");
      Serial.print(AudioProcessorUsage());
      Serial.print(" (");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("),  Mem = ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" (");
      Serial.print(AudioMemoryUsageMax());
      Serial.println(")");
      last_time = millis();
    }
  }

  usbMIDI.read();

  readInputs();
}

void OnNoteOn(byte channel, byte note, byte velocity) {
  Serial.print("Note On, ch=");
  Serial.print(channel, DEC);
  Serial.print(", note=");
  Serial.print(note, DEC);
  Serial.print(", velocity=");
  Serial.print(velocity, DEC);
  Serial.println();

  synth.noteOn(note);
}

void OnNoteOff(byte channel, byte note, byte velocity) {

  synth.noteOff(note);
}

void OnControlChange(byte channel, byte control, byte value) {
  // Sustain pedal.
  if (control == 64) {
    // TODO: respond to sustain pedal message.
    if (value >= 64) {
      synth.sustain(true);
    }
    else {
      synth.sustain(false);
    }
  }
}

// Inputs
const byte multiplexAPin = 17;
const byte multiplexBPin = 16;
const byte multiplexCPin = 15;

const byte multiplexInputPin1 = 21;
const byte multiplexInputPin2 = 20;
const byte multiplexInputPin3 = A10;
const byte multiplexInputPin4 = A11;

const byte keyInputPins[7] = {
  8, 5, 4, 3, 2, 1, 0
};

int p0, p1, p2, p3, p4, p5, p6, p7;

const byte propagationDelay = 50;

// Keep track of which keys are pressed.
bool keyState[100];


void readAdditionalInputs() {

  byte pressed = digitalRead(keyInputPins[0]);

  byte channel = 4;

  if (pressed == LOW) {
    if (!keyState[0]) {
      // Send sustain pedal message.
      usbMIDI.sendControlChange(64, 127, channel);
      synth.sustain(true);
      keyState[0] = true;
    }
  }
  else {
    if (keyState[0]) {
      usbMIDI.sendControlChange(64, 0, channel);
      synth.sustain(false);
      keyState[0] = false;
    }
  }
}

/**
 * Read a 12-position selector.
 */
byte readSelectorPin(byte pin) {
  int interval = (1023 - 1) / 11; // There are twelve positions, with 11 intervals.
  return (analogRead(pin)+interval/2)/interval;
}

void readInputKey(byte baseNote, byte keyOffset) {
  byte pressed = digitalRead(keyInputPins[keyOffset]);
  byte note = baseNote + keyOffset;
  byte velocity = 83;
  byte channel = 4;

  if (pressed == LOW) {
    if (!keyState[note]) {

      usbMIDI.sendNoteOn(note, velocity, channel);
      synth.noteOn(note);
      keyState[note] = true;
    }
  }
  else {
    if (keyState[note]) {
      usbMIDI.sendNoteOff(note, velocity, channel);
      synth.noteOff(note);
      keyState[note] = false;
    }
  }
}

void readInputKeyRow(byte baseNote) {
  for (unsigned int i = 0; i < sizeof(keyInputPins); i++) {
    readInputKey(baseNote, i);
  }
}


void setupInputs() {
  // Setup multiplex selector pins
  pinMode(multiplexAPin, OUTPUT);
  pinMode(multiplexBPin, OUTPUT);
  pinMode(multiplexCPin, OUTPUT);

  // Initialize multiplex selector pins.
  digitalWrite(multiplexAPin, LOW);
  digitalWrite(multiplexBPin, LOW);
  digitalWrite(multiplexCPin, LOW);


  // Setup input pins
  pinMode(multiplexInputPin1, INPUT);
  pinMode(multiplexInputPin2, INPUT);
  pinMode(multiplexInputPin3, INPUT);
  pinMode(multiplexInputPin4, INPUT);

  for (byte i = 0; i < sizeof(keyInputPins); i++) {
    pinMode(keyInputPins[i], INPUT_PULLUP);
  }

  // Initialize key state.
  for (byte i = 0; i < sizeof(keyState); i++) {
    keyState[i] = false;
  }
}

void readInputs() {

  /**
   * Multiplexer pin 0
   */
  digitalWrite(multiplexCPin, LOW);
  delayMicroseconds(propagationDelay);

  // Mode depth
  int value = analogRead(multiplexInputPin1);

  // Amplitude envelope attack
  value = analogRead(multiplexInputPin2);

  // Filter cutoff
  value = analogRead(multiplexInputPin3);
  if (value != p3) {
   p3 = value;
   float freq = fscale(1, 1023, 200, 8000, p3, -3);
   synth.setFilterFrequency(freq);
  }

  // Preset selector
  value = readSelectorPin(multiplexInputPin4);

  readInputKeyRow(39);


  /**
   * Multiplexer pin 1
   */
  digitalWrite(multiplexAPin, HIGH);
  delayMicroseconds(propagationDelay);

  // Synth osc 2 voice
  value = readSelectorPin(multiplexInputPin1);
  Serial.println(value);
   if (value != p1) {
     p1 = value;

     switch (p1) {
       case 0:
         synth.setWaveForm2(WAVEFORM_NOISE);
         break;

       case 1:
         synth.setWaveForm2(WAVEFORM_SINE);
         break;

       case 2:
         synth.setWaveForm2(WAVEFORM_TRIANGLE);
         break;

       case 3:
         synth.setWaveForm2(WAVEFORM_SAWTOOTH);
         break;

       case 4:
         synth.setWaveForm2(WAVEFORM_SQUARE);
         break;
     }
   }

  // Sampler selector
  value = readSelectorPin(multiplexInputPin4);

  readInputKeyRow(53);


  /**
   * Multiplexer pin 3
   */
  digitalWrite(multiplexBPin, HIGH);
  delayMicroseconds(propagationDelay);
  readInputKeyRow(74);

  // Transpose
  value = analogRead(multiplexInputPin1);

  // Filter envelope amount
  value = analogRead(multiplexInputPin3);

  // Master volume
  value = analogRead(multiplexInputPin4);
  if (value != p7) {
   p7 = value;

   synth.setMasterVolume((float)p7 / 1023 * 0.8);
  }


  /**
   * Multiplexer pin 2
   */
  digitalWrite(multiplexAPin, LOW);
  delayMicroseconds(propagationDelay);

  //  readAdditionalInputs();

  // Synth osc 1
  value = readSelectorPin(multiplexInputPin1);

  if (value != p0) {
    p0 = value;

    switch (p0) {
      case 0:
        synth.setWaveForm1(WAVEFORM_SINE);
        break;

      case 1:
        synth.setWaveForm1(WAVEFORM_TRIANGLE);
        break;

      case 2:
        synth.setWaveForm1(WAVEFORM_SAWTOOTH);
        break;

      case 3:
        synth.setWaveForm1(WAVEFORM_SQUARE);
        break;

      case 4:
        synth.setWaveForm1(WAVEFORM_CELLO);
        break;

      case 5:
        synth.setWaveForm1(WAVEFORM_PIANO);
        break;

      case 6:
        synth.setWaveForm1(WAVEFORM_EORGAN);
        break;
    }
  }

  // LFO amplitude
  value = analogRead(multiplexInputPin3);


  /**
   * Multiplexer pin 6
   */
  digitalWrite(multiplexCPin, HIGH);
  delayMicroseconds(propagationDelay);
  readInputKeyRow(81);

  // Tap tempo
  value = analogRead(multiplexInputPin1);

  // LFO target
  value = readSelectorPin(multiplexInputPin3);

//  if (value >= 512) {
//    synth.setFilterLFOAmount(fscale(512, 1023, 0, 3, value, -3));
//  }
//  else {
//    synth.setAmplitudeModulationLFOAmount(fscale(1, 512, 1.0, 0.0, value, 3));
//  }


  /**
  * Multiplexer pin 7
  */
  digitalWrite(multiplexAPin, HIGH);
  delayMicroseconds(propagationDelay);

  readInputKeyRow(67);

  // Detune
  value = analogRead(multiplexInputPin1);
  // 1.0293022 = 24th root of 2 = quarter-step.
  float detune = fscale(1, 1023, 1, 1.0293022, value, 0);
  synth.setDetune(detune);

  // Filter resonance
  value = analogRead(multiplexInputPin3);
  float res = fscale(1, 1023, 0.0, 5.0, value, 0);
  synth.setFilterResonance(res);


  /**
   * Multiplexer pin 5
   */
  digitalWrite(multiplexBPin, LOW);
  delayMicroseconds(propagationDelay);

  readInputKeyRow(60);

  // Mode selector
  value = readSelectorPin(multiplexInputPin1);

  // Sustain pedal?
  value = analogRead(multiplexInputPin3);

  // Sampler velocity
  value = analogRead(multiplexInputPin4);


  /**
   * Multiplexer pin 4
   */
  digitalWrite(multiplexAPin, LOW);
  delayMicroseconds(propagationDelay);

  readInputKeyRow(46);

  // Synth mix
  value = analogRead(multiplexInputPin1);
  synth.setMix(fscale(1, 1023, 0.0, 1.0, value, 0));

  // LFO rate
  value = analogRead(multiplexInputPin3);
  synth.setLFORate(fscale(1, 1023, 0.05, 10.0, value, -1));

  // Mixer: Synth volume
  value = analogRead(multiplexInputPin4);
}