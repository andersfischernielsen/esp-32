# Apple Home Climate Sensing (SCD41)

This project will monitor temperature, humidity and CO2 levels from an SCD41 sensor connected to an ESP32 and transmit the data to Apple Home.

To configure the project, run

```sh
idf.py menuconfig
```

or set `CONFIG_WIFI_SSID` and `CONFIG_WIFI_PASSWORD` in `sdkconfig`, then navigate to "Wi-Fi Configuration" and set your SSID and password.

Finally, flash the project to an ESP with the SCD41 sensor connected to pin `21` and `22`.
