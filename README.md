SMS_RC_SM5100B
==============

An application that will receive commands and send SMS responses. Can
be a base for an SMS interface towards SM5100B, or pretty much
anything else regarding the SM5100B GSM shield.

The SM5100B might be moved out as a separate library if there are
requests for that, for now, this is how I turn on and off the heating
at our cabin, and I just wanted it to work.

I have put this under GPL. Get in touch if you have issues with that.

You need to install the SHT1x library from here:
https://github.com/practicalarduino/SHT1x
Install it according to these instructions:
http://www.arduino.cc/en/Hacking/Libraries

In order to use this with SHT15, the easiest way is to get the
breakout board from SparkFun, wire ground and VCC to gnd and +5V on
the arduino, and connect SCK to pin 11 and Data to pin 10. In that
case, this should work out of the box with this code. Also, you should
add a relay in order to acutally control something. I have gone for
JW1SN-DC5V and ALE1PB05, both from panasonic. You also need a
transistor, resistor and a diode. Connect it like this:
http://playground.arduino.cc/uploads/Learning/relays.pdf

I run pin 13 and 14 equally. 13 is by default connected to a diode, so
you can monitor if it is activated or not.

Good luck!


Morgan Tørvolt
morgan(at)torvolt.com
