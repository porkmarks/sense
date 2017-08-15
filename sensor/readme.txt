https://www.arduino.cc/en/Tutorial/ArduinoToBreadboard
8Mhz, internal oscillator
FUSES: (E:FE, H:DA, L:E2)


http://www.atmel.com/webdoc/avrlibcreferencemanual/group__avr__fuse.html

BOD should be 1.8V!!!!!!
Turn off BOD while sleeping


Read fuses:
avrdude -P /dev/ttyACM0 -b 19200 -c avrisp -p m328p -v

Write Extended Fuse:
avrdude -P /dev/ttyACM0 -b 19200 -c avrisp -p m328p -U efuse:w:0xFE:m


