# ESP32
Project with ESP32 :
- luminous_alarm
- wifi_test
- first_test


## Requirement

Install esp idf and read the toolchain setup :  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html#get-started-linux-macos-first-steps

## Build

if you already have an alias in your bash :
``` bash
get_idf
```

You can now build your project :
``` bash
idf.py build
```
To flash into your device (you can also monitor) :
``` bash
idf.py -p /dev/ttyUSB0 flash monitor
```

# debug
 Sometime you have to give acces to the serial port :
``` bash
sudo chown $USER:$USER /dev/ttyUSB0
```

To stop monitor :

``` bash
ctrl+T X
```
