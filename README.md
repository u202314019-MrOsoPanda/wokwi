# FreshSense — prototipo del dispositivo

El dispositivo hace lo que define el informe en la rama `develop` de [1ASI0572-2620-8725/report](https://github.com/1ASI0572-2620-8725/report/tree/develop). La sección 5.6 está vacía en esa rama. El comportamiento sale de las secciones 4.2.4 y 4.2.7.

## Qué hace la placa

1. Mide la señal física del informe: temperatura, humedad y etileno.
2. Arma un `SensorReading` con `deviceId`, `timestamp`, `temperatureC`, `humidityPct`, `ethylenePpm` y `meta`.
3. `isValid()` descarta una lectura fuera de un rango físico.
4. Envía `POST /api/v1/sensor-readings` con el token del dispositivo (TS41).
5. Envía `POST /api/v1/devices/{id}/heartbeat` para que el servidor no lo marque `OFFLINE` (US06).

Las alertas `HIGH_ETHYLENE`, `TEMP_RISK` y `NEARING_EXPIRY`, y los estados `FRESH`, `AT_RISK` y `SPOILED`, los calcula el servidor con `FreshnessService` y `ThresholdEvaluationService`. Los umbrales (`maxTemperatureC`, `maxEthylenePpm`) viven en el perfil de la zona. La placa no los decide y no lleva LED.

## Circuito

| Pieza | Conexión | Para qué, según el informe |
|---|---|---|
| DHT22 VCC | 3V3 | Temperatura y humedad |
| DHT22 GND | GND | Tierra |
| DHT22 SDA | GPIO 12 | `temperatureC` y `humidityPct` |
| Sensor de gas VCC | 5V | Etileno |
| Sensor de gas GND | GND | Tierra |
| Sensor de gas AOUT | GPIO 34 | `ethylenePpm` |

Wokwi no tiene un sensor de etileno. El sensor de gas llena `ethylenePpm`, que es un campo obligatorio de `SensorReading`.

## Cómo verlo

1. Abre un proyecto ESP32 en [wokwi.com](https://wokwi.com).
2. Copia `diagram.json`, `sketch.ino`, `FreshSenseNode.h` y `libraries.txt`.
3. Inicia la simulación y abre el monitor serial.
4. Cada 10 segundos aparece el JSON con saltos de línea y, debajo, el código HTTP de los dos POST.

El DHT22 arranca en 6.4 °C y 82 %. Mueve su temperatura y su humedad, y el control del sensor de gas, para cambiar la lectura que se envía.

Sustituye `DEVICE_ID` por el UUID del alta (US31) y `DEVICE_TOKEN` por el token que valida el servidor (TS41). Si el API responde 401, el circuito y el JSON siguen siendo los del informe: falta ese token.
