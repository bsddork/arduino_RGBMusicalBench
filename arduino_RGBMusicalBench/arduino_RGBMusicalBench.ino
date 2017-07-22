/*
 * Created By: Brian Woertendyke	bsddork@gmail.com
 * On: July 22, 2017
 * Project: Musical Bench with light show
 * 
 * Idea based on the original Musical Bench by the Exploratorium Tinkering Studio.
 * https://tinkering.exploratorium.edu/musical-bench
 * 
 * Lights were added to add visual feedback.
 * The LED shield uses 3x logic level N-Channel Mosfets to feed power to the RGB strip
 * PWM pins are used to adjust the voltage delivered to each color.
 * 
 * At the heart of this project is the VS1053 music instrument shield from Sparkfun.
 * https://www.sparkfun.com/products/10587
 * 
 * This project was created for a display piece at my local church, and is on display in the youth center.
 * 
 * The concept is to read the analog value and map the lights to the sensor travel.
 * More could be done to improve on the basic idea.
 * Adding other features could be incorporated by adding new functions into the state machine manager.
 * 
 * The state machine is time based.
 * Sensor readings are measured on "1/16th" note intervals.
 * Lighting effects are running in the extra cycles to execute transitions between colors.
 * 
 * Requieres LEDFader Library from https://github.com/jgillick/arduino-LEDFader
 * The LEDFader library keeps it's own statemachine to run the fading effects in the background.
 * 
 * Color Hue conversion was made possible by http://www.brianneltner.com
 * Code was copied from: http://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
 * 
 * Button control library from https://github.com/bsddork/clickButton
 * Library was modified to run on Arduino.
 */


// Libraries
#include <SoftwareSerial.h>  // needed for MIDI communications with the MI shield
#include <math.h>
#include <LEDFader.h>
#include <ClickButton.h>

// Global Constants - These values will never change
#define DEBUG         false

// Music Stuff
#define SENSOR_HYSTERESIS 4 // ignore incremental changes smaller than this value...  needed to kill noise, but also affects how busy and dense the arpeggio is
#define LOWEST_NOTE 48      // 48 = C3 in MIDI; the piano keyboard ranges from A0 = 21 through C8 = 108, the lowest octaves can be pretty muddy sounding
#define HIGHEST_NOTE 96     // 96 = C7. A smaller range will result in a less busy feel, and may allow more repeated strikes of the same note.  You probably won't hear half of this range with human touch, but short the touch pads with metal and you will
#define SOUND_MS 150        // Approx. loop rate in millis, all note durations will be integer multiples of this.  Tweak to speed up or slow down the arpeggio.

// these should need less tweaking, but go ahead and see what they do
#define MY_INSTRUMENT 0  // max 127.  GM1 Melodic bank instrument 0 is the Acoustic Grand Piano.  You could tweak this, but beware: since we are not using any note off method, if you pick one with long or infinite sustain, you'll get cacaphony in a hurry
#define NOISE_FLOOR 1000  // out of 1023, higher than this is considered open circuit.
#define VELOCITY  100     // note strike "force", max 127
#define VOLUME    110     // max 127, 110 works well for most headphones and fine for most amplifiers as well

// Button Function
#define LONG    2000    // Long button press time in milliseconds

// LED Tempo
#define LED_MS        SOUND_MS*4    // Eighth note tempo
#define PRESTO        SOUND_MS*2    // Very fast
#define ALLEGRO       SOUND_MS*4    // Moderately fast
#define MODERATO      SOUND_MS*6    // At a moderate speed
#define ANDANTE       SOUND_MS*8    // Moderately slow, flowing along

// From here on down, tweaking is probably not a great idea...

// Sound pin assignments
#define RX_PIN      2   // Soft serial Rx; M.I shield TxD
#define TX_PIN      3   // shield RxD
#define RESET_PIN   4   // low active shield reset
#define SENSOR_PIN  A0  // also known as P14

// Button Pin
#define BUTT        5   // Button Pin Digital Input

// LED pin assignments
#define R_PIN        9  // PWM RGB Red Pin
#define G_PIN       10  // PWM RGB Green Pin
#define B_PIN       13  // PWM RGB Blue Pin
#define LED_NUM      3  // Number of LED PWM Pins in use
#define UP           1  // Up slope
#define DOWN         0  // Down slope

// Standard MIDI, ADDRESS == CH1 which is all we propose to use
#define MIDI_BAUD     31250
#define PROG_CHANGE   0xC0  // program change message
#define CONT_CHANGE   0xB0  // controller change message
#define NOTE_ON       0x90  // sorry about the Hex, but MIDI is all about the nybbles!
#define ALL_OFF       0x7B  // Using "all notes off" rather than "note off" for single voice MIDI is a common tactic; MIDI devices are notorious for orphaning notes
#define BANK_SELECT   0x00
#define CHANNEL_VOL   0x07

// GM1 instrument bank mapping implemented on VS1053b chip
#define MELODIC_BANK  0x79  // there are other options, just not any that are better.


// ---- Global Variables ----//
// Music Related
int travel;                     // sensor movement from previous (determines direction)
unsigned int distance;          // sensor movement length (absolute value)
unsigned int velocity;          // travel speed (strike force)
unsigned int touchSensor;       // holds the most recent accepted sensor value. 
unsigned int sensorBuffer;      // holds a provisional sensor value while we look at it
unsigned int note;              // holds the calculated MIDI note value

byte insCounter = 0;            // counter for current instrument
const byte iTable[30] = {           // Table of "good" sounding instruments, a lot sound similar
  0,  4,  5,  6,  8,  11, 13, 15,
  17, 18, 19, 20, 23, 27, 32, 36,
  45, 47, 67, 71, 79, 80, 86, 88,
  104,  105,  108,  113,  118,  124
};

unsigned int rgb[LED_NUM];           // RGB value storage, array of 3 values
unsigned int lastHue = 0;     // Previous Hue value

int function = 0;            // Button results

boolean playNote = true;      // Default state of sound
byte ledFadeState = 0;        // state of LED fading

unsigned int ledFadeDelay = 2*LED_MS;   // Time to fade LED colors, this can change with tempo
unsigned long currentMillis;      // store the current time
unsigned long lastLEDtic;         // stateMachine timer for LED display
unsigned long lastNotetic;        // stateMachine timer for Sound
boolean sustain = false;           // stateMachine for sustaining notes
unsigned int timeDiff;            // placeholder for storing delay calculations


// ---- Initialize Librarys ---- //
// Define Music board
SoftwareSerial MIDIserial(RX_PIN, TX_PIN);  // channel for MIDI comms with the MI shield

// set PWM pins: red, green, blue
LEDFader leds[LED_NUM] = {
  LEDFader(9),
  LEDFader(10),
  LEDFader(13)
};

// Define the button properties
ClickButton bob(BUTT, HIGH);

// ---- Functions ----//
void setup() {
  Serial.begin(115200);  // USB is for debugging and tuning info
    while (!Serial) {
      ; // wait for serial port to connect. Needed for Leonardo only
     }
   
  // reset pin set-up and hwr reset of the VS1053 on the MI shield.  It's probably going to make a fart-y sound if you have an external amplifier!
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(100);
  digitalWrite(RESET_PIN, HIGH);
  delay(1000);

  //Start soft serial channel for MIDI, and send one-time only messages
  MIDIserial.begin(MIDI_BAUD);
  talkMIDI(CONT_CHANGE , CHANNEL_VOL, VOLUME);   // set channel volume
  talkMIDI(CONT_CHANGE, BANK_SELECT, MELODIC_BANK);  // select instrument bank 
  talkMIDI(PROG_CHANGE, MY_INSTRUMENT, 0);   // set specific instrument voice within bank.

  Serial.println("Begin - Start with Random Color");

  // Start the timer
  currentMillis = millis();
  lastLEDtic = currentMillis;
  lastNotetic = currentMillis;

  // Setup button timers (all in milliseconds / ms)
  bob.longClickTime = LONG;     // time until "held-down clicks" register
  
}

void chkTime() {
  // State Machine Manager
  // Check the time and decide to execute upon next timecode
  
  currentMillis = millis();     // Update the current timer value

  // ----------------------
  // Add more state machines here for new functions that require delay time management
  // Create a static variable to keep track of time
  // ----------------------

  
  // ---- LED time check ---- //
  // call the LED fade statemachine for background effects
  rgbUpdate();
  timeDiff = currentMillis - lastLEDtic;
  if ( timeDiff >= SOUND_MS) {                       // stateMachine to apply changes based on the LED delay value
      
    if (playNote) {         // Fade in the lights from dark
      illuminate(lastHue);
    }
    else if (!playNote && !ledFadeState) {        // Note is done playing, and fader is done fading
      //change the fade delay slow
      ledFadeDelay = LED_MS/2;
      darken();
    }
    lastLEDtic = currentMillis;           //Update the statemachine for next cycle
  }   //----END LED State Machine----//


  // Sound time check
  //-----------------//
  timeDiff = currentMillis - lastNotetic;
  if (timeDiff >= SOUND_MS) {
    //Call the function to enable tone output
    sensorInput();
    //Update the statemachine for next cycle
    lastNotetic = currentMillis;
  }
  
  // Add more functions here
  // #Define a timeout for delay between checks
  //
}

// ---- HSI to RGB ---- //
// 
// Function example takes H, S, I, and a pointer to the 
// returned RGB colorspace converted vector. It should
// be initialized with:
//
// int rgb[LED_NUM];
//
// in the calling function. After calling hsi2rgb
// the vector rgb will contain red, green, and blue
// calculated values.
//
// Hue 0-360 degrees, Saturation 0-1 dec percent, Intensity 0-1 dec percent
//
void hsi2rgb(float H, float S, float I, int* rgb) {
  // Convert HSV
  int r, g, b;
  H = fmod(H,360); // cycle H around to 0-360 degrees
  H = 3.14159*H/(float)180; // Convert to radians.
  S = S>0?(S<1?S:1):0; // clamp S and I to interval [0,1]
  I = I>0?(I<1?I:1):0;
    
  // Math! Thanks in part to Kyle Miller.
  if(H < 2.09439) {
    r = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    g = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    b = 255*I/3*(1-S);
  } else if(H < 4.188787) {
    H = H - 2.09439;
    g = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    b = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    r = 255*I/3*(1-S);
  } else {
    H = H - 4.188787;
    b = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    r = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    g = 255*I/3*(1-S);
  }
  // Update the RGB Values, but first pass them through the value mapper to compensate for Common_Annode flip/flop
  rgb[0]=r;
  rgb[1]=g;
  rgb[2]=b;
}

void darken() {
  // Simple fade out
  // Update all LEDs and start new fades
  for (byte i = 0; i < LED_NUM; i++) {
    LEDFader *led = &leds[i];
    led->fade(0, ledFadeDelay);       // Darken to 0
  }
}

void illuminate(int h) {       // pass in a Hue value, use a random value as default
  // Apply changes to the lighting effects

  // start with default settings
  static float s = 1;
  //static float y = .75;
  
  hsi2rgb(h,s,velocity,rgb);       // convert the HSI into RGB values, rgb array will be updated in the background

  // Update all LEDs and start new fades
  for (byte i = 0; i < LED_NUM; i++) {
    LEDFader *led = &leds[i];
    // Start face to new value
    led->fade(rgb[i], ledFadeDelay);
  }
  // End of RGB update
}

// Update the RGB state
void rgbUpdate () {
  // reset fading state before update
  ledFadeState = 0;

  // Check each PWM pin and update the value
  for (byte i = 0; i < LED_NUM; i++) {
    LEDFader *led = &leds[i];
    led->update();
    // update fade state of each pin
    ledFadeState += led->is_fading();         // when all LEDs are finished fading, the result should be 0
  }
  // End of loop
}

//Sends all the MIDI messages we need for this sketch. Use with caution outside of this sketch: most of the MIDI protocol is not supported!
void talkMIDI(byte cmd, byte data1, byte data2) {
  //digitalWrite(LED_PIN, HIGH);  // use Arduino LED to signal MIDI activity
  MIDIserial.write(cmd);
  MIDIserial.write(data1);
  if(cmd != PROG_CHANGE) MIDIserial.write(data2); //Send 2nd data byte if needed; all of our supported commands other than PROG_CHANGE have 2 data bytes
}

// Change the active instrument
void changeInstrument() {
  if (insCounter < sizeof(iTable)-1) {      // Check the current counter value before increasing
    insCounter++;
  } 
  else { insCounter = 0; }                        // reset counter when limit found

  Serial.print("Instrument:");
  Serial.println(iTable[insCounter]);
  
  talkMIDI(PROG_CHANGE, iTable[insCounter], 0);   // set specific instrument voice within bank.
}

void sensorInput() {
  // Read the sensor and play music
  int newHue;                               // buffer for new value
  sensorBuffer = analogRead(SENSOR_PIN);      // measure the sensor input
  travel = (sensorBuffer-touchSensor);    // direction
  distance = abs(travel);                 // amount of movement

  if (sensorBuffer <= NOISE_FLOOR) {  // only play a note if the accepted sensor value is below the noise floor  
    if (sensorBuffer <= 5 && lastNotetic > 5000) {  // Special Demo for cheaters, ignore the first 5 sec after reset
      easterEgg();
    }
    else if (distance >= SENSOR_HYSTERESIS)  {  // only accept a new sensor value if it differs from the last accepted value by the minimum amount
      touchSensor = sensorBuffer;  // accept the new sensor value
      
      // calculate and play a new note 
      note = map(touchSensor, 0, 1023, HIGHEST_NOTE, LOWEST_NOTE);  // high sensor values (high resistence, light touch) map to low notes and V.V.
      
      velocity = map(distance, 0, 1023, 25, 127);  // large movements will equal stronger hits
      
      playNote = true;    // a note is playing
      
      Serial.print("Note:");
      Serial.println(note);

      if (!sustain) {      // check the mode for sustain
        talkMIDI(CONT_CHANGE , ALL_OFF, 0);  // turn off previous note(s); comment this out if you like the notes to ring over each other
      }
      talkMIDI(NOTE_ON , note, VELOCITY);  // play new note! --Using constant velocity definition for even notes
    }
    travel = map(travel, -100, 100, -30, 30);                 // limit the travel distance in either direction
    newHue = lastHue + travel;                         // Change LED Color Hue
    if (newHue <= 0) {                           // Use the buffer to find direction to travel
      lastHue = 360 + travel;
    }
    else if (newHue >= 360) {
      lastHue = 0 + travel;
    }
    else {
      lastHue = newHue;
    }
  }
  else {
    // Sensor is considered open, turn everything off
    talkMIDI(CONT_CHANGE , ALL_OFF, 0);  // turn off previous note(s);
    travel = 0;
    distance = 0;
    velocity = 0;
    playNote = false;
  }
  
  if (DEBUG) {
    Serial.print(sensorBuffer);
    Serial.print(",");
    Serial.print(note);
    Serial.print(",");
    Serial.print(velocity);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(",");
    Serial.print(travel);
    Serial.print(",");
    Serial.print(lastHue);
    Serial.print("\n");
  }
}

// Odd surprise for anyone who trys shorting the sensors
void easterEgg() {
  //delay(3000);
  Serial.print("\n\n");
  Serial.print("Play Time");
  Serial.print("\n\n");

  for (byte p = 0; p<3; p++) {
    analogWrite(R_PIN,255);
    delay(500);
    analogWrite(R_PIN,0);
    delay(500);
  }
  delay(1500);
  
  for (byte i = 0; i<25; i++) {
    analogWrite(G_PIN,255);
    talkMIDI(NOTE_ON, HIGHEST_NOTE, 100);
    delay(50);
    analogWrite(G_PIN,0);
    talkMIDI(CONT_CHANGE , ALL_OFF, 0);
    delay(50);
  }
  for (byte i = 0; i<25; i++) {
    analogWrite(B_PIN,255);
    talkMIDI(NOTE_ON, HIGHEST_NOTE-16, 100);
    delay(50);
    analogWrite(B_PIN,0);
    talkMIDI(CONT_CHANGE , ALL_OFF, 0);
    delay(50);
  }
  for (byte i = 0; i<25; i++) {
    analogWrite(R_PIN,255);
    talkMIDI(NOTE_ON, HIGHEST_NOTE-32, 100);
    delay(50);
    analogWrite(R_PIN,0);
    talkMIDI(CONT_CHANGE , ALL_OFF, 0);
    delay(50);
  }
  delay(750);

  for (int j = 0; j < 360; j++) {
    hsi2rgb(j,1,1,rgb);
    analogWrite(R_PIN,rgb[0]);
    analogWrite(G_PIN,rgb[1]);
    analogWrite(B_PIN,rgb[2]);
    delay(10);
  }
  delay(750);

  for (int k=0; k<600; k++) {     // ~40 Hz Pulse
    analogWrite(R_PIN,255);
    analogWrite(G_PIN,255);
    analogWrite(B_PIN,255);
    talkMIDI(NOTE_ON, 24, 100);
    delay(12);
    analogWrite(R_PIN,0);
    analogWrite(G_PIN,0);
    analogWrite(B_PIN,0);
    talkMIDI(CONT_CHANGE , ALL_OFF, 0);
    delay(12);
  }
}

void buttonFunction() {
  // ---- Button Presses ---- //
  bob.Update();   // Check the button state
  
  if (bob.clicks != 0) function = bob.clicks;     // Save the button codes before next update
  if(function == 2) {             // Double Click mode
    sustain = sustain ? 0 : 1;    // Change sustain state (when true set false & vise versa)
    Serial.print("Sustain:");
    Serial.println(sustain);
  }
  if(function == 3) {             // Triple click
    ;   // Do something special
  }
  if(function == -1) {            // Single Long press
    changeInstrument();           // Change instrument
  }
  
  function = 0;           // Reset function for next cycle
}

void loop(){
  // Main Loop

  buttonFunction();   // Button presses
  
  chkTime();      // call the stateMachine manager
  
}
