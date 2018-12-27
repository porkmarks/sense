https://www.arduino.cc/en/Tutorial/ArduinoToBreadboard
8Mhz, internal oscillator
FUSES: (E:FF, H:DA, L:A2)

Burn with arduino_sketches/Atmega_Board_Programmer/Atmega_Board_Programmer.ino
The sketch has the fuses hardcoded inside, lines 417 - 419

Fuse Calculator:
http://www.engbedded.com/fusecalc/

No BOD, not needed as we can detect in software

When programming, use the boards file in this folder and choose "Atmega328 on a breadboard (1Mhz internal oscillator)"

