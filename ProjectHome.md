This project is designed to run on a AT90USBKEY (or other AT90USB1287-based board) and control a USB modem key to retrieve data from the Internet.

The code contains a PPP stack designed to operate with the Contiki TCP/IP stack. The example code has been compiled with WINAVR 20100110.

So far, it has been tested with Huawei E160G & E160E USB Modems, but the idea is to support more modems in the future.

The project has been tested on the "3" and "Virgin" prepaid networks in Australia.


The project incorporates the following external frameworks:

**Dean Camera's LUFA USB stack - http://www.fourwalledcubicle.com/LUFA.php**

**The Contiki (formerly uIP) TCP/IP stack - http://sourceforge.net/projects/contiki/**


A note on power - in testing so far, it seems that the AT90USBKEY does not have sufficient power available at the USB connector to run some modems. The E160G seems to work well, but the E160E required the addition of an external 5V power supply on the USB port to enumerate properly. I have also tried a Huawei E169 modem, and this failed to enumerate even with external power (further research required).