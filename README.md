# Arduino RGB Musical Bench

Musical Bench with RGB LED Lighting effects. Based on the original Musical Bench by the Exploratorium Tinkering Studio.

## Background
This is the home for the code used to create the art piece. For the hardware and build details, visit the Instructables page.

The children's ministry director was looking for a fun and interactive art piece to install for the summer camp. The project had to be themed around "Gadgets & Gizmos". The original idea was found on the Exploratorium's Tinkering Studio site. The project had a great start and deserved a little more flash to catch the eye. Adding lighting effects was a simple effort since I already had the spare parts in my garage.

## Improvements Made

* RGB LED strip - changes colors with the sound
* A single button - controls various aspects of sound and light

### RGB LEDs
Lights were added to include visual feedback in combination with the sound. The LED shield uses 3x logic level N-Channel MOSFETs to feed power to the RGB strip. Using the PWM pins from the Arduino to adjust the voltage delivered to each color. The PWM output controls the gate on the MOSFET. In order to generate vivid colors, the code depends on using HSL (hue, saturation, lightness) values as a starting point, then converts into RGB values for driving the LED strip.

Sound generation controls the light movement. The light travels around the color wheel in proportion to the distance from the touch sensor.

### Mode Button
The single button controls many functions by detecting multiple clicks or long clicks. Mainly used for switching instruments within the chip library on the VS1053. Only a few choice instruments were included into the table since the full list would be far too many to cycle through.

## Prerequisites

* SparkFun Music Instrument Shield - https://www.sparkfun.com/products/10587
* Arduino Leonardo - https://store.arduino.cc/usa/arduino-leonardo-with-headers
* Arduino IDE - https://www.arduino.cc/en/Main/Software

## Installing

Clone the repository and extract. Remove the "-master" from the folder name, then move into your Arduino projects folder.

Get the required libraries also. Or else you will have some errors during compile time.
### Dependencies
* https://github.com/jgillick/arduino-LEDFader
* https://github.com/bsddork/clickButton

## Authors

* **Brian Woertendyke**

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* Original Musical Bench project - https://tinkering.exploratorium.edu/musical-bench
* Button control library forked from pkourany - https://github.com/bsddork/clickButton
* Color Hue conversion - http://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
* LEDFader Library - https://github.com/jgillick/arduino-LEDFader
* Rob McGee from Life Community Church for inspiring me to create this piece of art!

