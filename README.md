This is a revisited original clockwise project (canvas) from https://github.com/jnthas/clockwise 
I have modified this project for basically insert a new canvas without get it from internet (for private canvas messages/images)

In this project i have added a new local canvas named "vespa". It is in a serialized json onto the variable "vespa" in the vespa variable fit on the firmware/clockfaces/cw-cf-0x07/localcanvas.h file
It is also present a original local canvas "snoopy3" into the snoopy variable.
For use it you must put "LOCAL" in the Server Address setting, and put "vespa" or "snoopy" in the Description file setting.
It is possible to use original canvas repository raw.githubusercontent.com

Main list changes:
1) changed IMAGE_BUFFER_SIZE to 4096 (for a bigger images) - needed for vespa
2) Add reset WiFi configuration button in web page (click on WiFi name)
3) Add Date/Time of last NTP update in web page
4) Show Ip address at startup (if you have problem with canvas.local name on browser)
5) Add new free font cartographer3pt7b (only numbers for time display)
6) Changed all pin for ESP32 for a better wiring (see images)

New pin assigments (original in comment):
  mxconfig.gpio.a = 27; //23
  mxconfig.gpio.b = 17; //19
  mxconfig.gpio.c = 14; //5
  mxconfig.gpio.d = 16; //17
  mxconfig.gpio.e = 5; //18
  mxconfig.gpio.r1 = 32; //25
  mxconfig.gpio.g1 = 19; //26
  mxconfig.gpio.b1 = 33; //27
  mxconfig.gpio.r2 = 25; //14
  mxconfig.gpio.g2 = 18; //12
  mxconfig.gpio.b2 = 26; //13
  mxconfig.gpio.lat = 4; //4
  mxconfig.gpio.oe = 13; //15
  mxconfig.gpio.clk = 12; //16

I'm not a programmer, so I'm sure the changes could be done better. Please be patient with me...
  

  
