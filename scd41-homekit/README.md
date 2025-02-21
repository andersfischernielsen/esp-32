# Apple Home Climate Sensing (SCD41)

<p align="center">
<img width="535" alt="home-2" src="https://github.com/user-attachments/assets/971c91d2-50bb-444b-99a3-8c6510601f68" />
</p>

---

This project will monitor temperature, humidity and CO2 levels from an SCD41 sensor connected to an ESP32 and transmit the data to Apple Home.

To configure the project, run

```sh
idf.py menuconfig
```

or set `CONFIG_WIFI_SSID` and `CONFIG_WIFI_PASSWORD` in `sdkconfig`, then navigate to "Wi-Fi Configuration" and set your SSID and password.

Finally, flash the project to an ESP with the SCD41 sensor connected to pin `21` and `22`: 

```
GND (ESP32) -> GND (SCD41 sensor)
3.3V (ESP32) -> VDD (SCD41 sensor)
Pin 21 (ESP32) -> SDA (SCD41 sensor)
Pin 22 (ESP32) -> SCL (SCD41 sensor)
```
