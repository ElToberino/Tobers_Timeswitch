# Tobers Timeswitch for ESP8266
Tobers Timeswitch is a versatile and multifunctional time switch for ESP8266 devices, based on the great time switches on www.fipsok.de (Jens Fleischer).

Beside the "classical" timeswitch functions widely configurable for every single day, it offers a lot of different sunrise/sunset and twilight modes. A countdown timer is also available.<br>
Using the inbuilt Master/Client function you can programm multiple devices "by one click". This can be very helpful if you have lot of devices running that shall all use the same scheme of switching times.
Tobers Timeswitch is easily and extensively configurable via a comfortable web interface.<br>
<br>

**Webinterface and Functions**



**Requirements**<br>
* *Hardware*<br>
ESP8266 or ESP8266 based device with at least 1MB flash memory <br>

* *Arduino IDE and the following libraries:*<br>
[My fork of WifiManager library (development branch) by tzapu/tablatronix](https://github.com/ElToberino/WiFiManager_for_Multidisplay)<br>

* *Installed boards:*<br>
[ESP8266 core for Arduino](https://github.com/esp8266/Arduino)<br>

* *Recommended tools:*<br>
[Arduino ESP8266 LittleFS Filesystem Uploader](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin)<br>


For complete an detailed information about the setup of the device and the steps to be taken before compiling please visit the article I wrote at hackster.io. You will also find some more pictures and a video there.<br>
<br>
**Important Note**<br>
This code has been made for ESP8266. There is a large number of smart devices availabe basing on these chips - some of them are easily flashable with a custom firmware. Many tips and tutorials can be found in the web explainig how this flashing process can be made. I don't give any advise concerning this because it can be (potentially) very dangerous: Most of these devices are working with mains voltage - so modifying these devices can become a life threatening thing, if you don't have the expertise for electrical works!
As clearly stated in the license * *the code is published "without any warranty; without even the implied warranty of merchantability or fitness for a particular purpose."*<br>
<br>
**General information about smart devices, smart plugs etc.**<br>
A lot of smart devices are used for switching loads on and off. It is very important (!) to keep within the specifications of the device and not to exceed the maximum load declared on the devices. (Of course, this should be self-evident and a matter of common sense (or as we say: "Hausverstand"). Having read through a lot of forums and hearing complaints about melt-down or burnt-off devices that people have used to switch large boilers etc., I feel myself forced to give this hint).
I myself wouldn't even think about reaching the maximum load - high loads should always be switched by contactors (German: "Schütz").<br>
<br>
Loads including a * *"Switching power supply"* (most of the modern devices and gadgets have inbuilt SWPs) can also cause problems, because - in the moment of switching on - many SWPs can pull very high inrush currents, exceeding the maxium rated load of the device by multiple times. Of course, only for a short time of some ms, but the resulting arc between the switching contacts of the relay can weld them together and destroy your switching device. This can be prevented by using contactors or inrush current limiters.
Many people are not aware, that even a couple of simlutaniously switched on "retrofit LED bulbs" can cause this damage, though the rated current of these lamps is very low. But each of these has a small built-in SWP and this can be to much for the relay contacts. You can often read complaints like "My smart device has been damaged after some weeks and can't be switched off any more" - but mostly this isn't a failure of the smart device but of the wrong or not properly calculated load attached to it.
