# SensESP Fan Control Project

For controlling a Caframo Sirocco II fan remotely.  WiFi control and SignalK data collection

Comprehensive documentation for SensESP, including how to get started with your own project, is available at the [SensESP documentation site](https://signalk.org/SensESP/).


Hardware
This uses the Adafruit INA260 to measure power, with the power measurement you can determine the current speed of the fan and then know the number of button presses it needs to get to your desired state.  This accepts HTML PUT commands to set the fan speed.  Also a web page is provided at port 8081 to show the data from the unit and also set the fan speed.

