# cooper

Python script for use with [MicroPython on ESP8266](https://docs.micropython.org/en/latest/esp8266/esp8266/tutorial/index.html),
for controlling various home automation peripherals. Customized for my purposes but feel free to edit to your liking.

Installation:

1. Copy main.py to the ESP8266 (I use [webrepl\_cli.py](https://github.com/micropython/webrepl)) running MicroPython and reboot

A web server should now be running on the ESP8266, port 80, offering access to the connected hardware.
Edit the script to change the settings as needed. Can be used by itself but meant for use with 
[Homebridge](https://www.npmjs.com/package/homebridge) running on another system (I use a Raspberry Pi),
integrates well with the following Homebridge plugins:

* [homebridge-http](https://www.npmjs.com/package/homebridge-http): for turning GPIO outputs on/off, via given URLs
* [homebridge-udp-contactsensor](https://github.com/rxseger/homebridge-udp-contactsensor): for receiving datagrams when GPIO inputs change
* [homebridge-udp-lightsensor](https://github.com/rxseger/homebridge-udp-lightsensor): for receiving datagrams when ADC input changes significantly

## License

MIT
