# Arduino_RGBMusicalBench
Musical Bench with RGB LED Lighting effects

Idea based on the original Musical Bench by the Exploratorium Tinkering Studio.
 https://tinkering.exploratorium.edu/musical-bench
 
Lights were added to add visual feedback.
The LED shield uses 3x logic level N-Channel Mosfets to feed power to the RGB strip
PWM pins are used to adjust the voltage delivered to each color.
 
At the heart of this project is the VS1053 music instrument shield from Sparkfun.
 https://www.sparkfun.com/products/10587
 
This project was created for a display piece at my local church, and is on display in the youth center.
 
The concept is to read the analog value and map the lights to the sensor travel.
More could be done to improve on the basic idea.
Adding other features could be incorporated by adding new functions into the state machine manager.
 
The state machine is time based.
Sensor readings are measured on "1/16th" note intervals.
Lighting effects are running in the extra cycles to execute transitions between colors.
 
Requieres LEDFader Library from https://github.com/jgillick/arduino-LEDFader
The LEDFader library keeps it's own statemachine to run the fading effects in the background.
 
Color Hue conversion was made possible by http://www.brianneltner.com
Code was copied from: http://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
 
Button control library from https://github.com/pkourany/clickButton
Library was modified to run on Arduino.
