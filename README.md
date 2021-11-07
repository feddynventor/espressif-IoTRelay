# espressif-IoTRelay
Compatible with ESP8266 and ESP32, includes features like Clock controlled Relay and Emon power monitor. Everything bridged by MQTT and NTP synchronization

## MQTT Endpoints
- `/current/interval` sets current measurement publish interval (default: 10)
- `/current/zero` sets the value to add or subtract to current measurements (default: 0)
- `/current/threshold` value threshold in which the timer won't trigger if value is higher (default: 0 or disabled)
- `timer/[on,off]/[hour,min]` sets the timer in 24H format
- `timer/off/go` if sent value is different from `1` the timer won't trigger until is set up to `1`
- `timer/days` array of values like `1111111` indicating which days of the week the timer should trigger on turn off (first element is the first day of the week returned from NTP server)
- `out*/[set,get]` gathers info or sets the state about the specified digital output