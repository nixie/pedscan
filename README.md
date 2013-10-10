pedscan
=======
USB orgue pedalboard MIDI scanner using AVR!


Aims of this project
-------------------
There are many projects on the web about midifying organ pedalboards. Mostly, they are based on AVR/PIC controller with classical MIDI connector wired to UART. Or using commercial (expensive) keyboard scanners. I was looking for some AVR solution with USB without success. But then I found [midibox](http://www.ucapps.de/index.html?page=midio128.html) community and really usefull library [V-USB](http://www.obdev.at/products/vusb/index.html). So here it is, with documentation and everything you will need to midify your old analog organs.

Features
--------
  - USB connectivity
  - uses ATmega8
  - uses shift registers for I/O like midibox (it's compatible with midibox DIN modules) and thus it is scalable - it can be used not only as a pedalboard scanner, I scan with this circuit all keys of my two-manual organs in in about 10uS
  - fast key debouncing
  - nice and well documented code

Hardware
--------
Daisy-chained 74HC165 shift registers are connected to atmega's SPI pins.
USB cable is connected to atmega as described in V-USB documentation. I use 3V6 zener-diodes to drop voltage on USB data pins.
In the future I might create a schematic diagram, but there is nothing special with this, only that all shift registers are latched using PB0.

Firmware
--------
Project code is based on AVR USB controller [V-USB-MIDI](http://cryptomys.de/horo/V-USB-MIDI/).

Everything can be configure in `main.c` through `#define`s.
There is also a file `descriptors.h` and there are defined USB interfaces and endpoints of this USB device. When connected to PC, you will see that this is a MIDI streaming device with one MIDI OUT port.



Links
-----
Some links are in text, here is some other interesting stuff:
  - [Nice solution with PIC](http://www.appletonaudio.com/ele-projects/midi-console-controller/) (there are also some promising sample sets)
