# Laser Tag SAOs

<img width="1415" height="985" alt="Laser Tag Gun Image" src="https://github.com/user-attachments/assets/8649a870-c8c0-4e07-ae77-97874baa10a1" />

These are little badge add-ons so you can play laser-tag against your friends :D 

- VSLY5940 Narrow beam IR transmitter
- TSOP38438 IR receiver
- NPN for turning on the IR transmitter
- SMD4020 side mount neopixels
- 0.91" 128x32 OLED/display for funsies
- PS1240P02BT Piezo buzzer
- Button to shoot and a button to change modes or other stuff
- 1 SAO port
- ATtiny84 DIP 14

## Firmware

You can program the laser tag guns over SPI broken out over the SAO connector.

First bridge the jumpers on the back with solder.

Next, you'll need to setup your Arduino as an ISP. I've set mine up for use with the Pi Pico, so you'll need to modify the ISP code for other boards. 
- Select your board by going tools -> boards -> whatever board you're using. For the raspberry pi, you'll need to install it in the board manager tab. 
- Upload the ArduinoISP code in [/programming/ArduinoISP](/programming/ArduinoISP).

Then you should open up the PCB in KiCad and wire up the SAO connector to the Pi Pico, I'll make a second version with silkscreen labels in the future:
- 3.3V to 3.3V
- GND to GND
- IO19 to MOSI
- IO18 to SCK
- IO16 to MISO
- GPIO10 to RESET

Then change the board to be the ATtiny24/44/84 No bootloader from the ATTinyCore library and set tools -> programmer to be "Arduino as ISP".

Next run tools -> burn bootloader and then upload the ATtiny code in [/programming/laser-tag](/programming/laser-tag) and you're all flashed!
