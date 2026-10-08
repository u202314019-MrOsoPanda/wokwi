# FreshSense — prototipo del dispositivo

El dispositivo hace lo que define el informe en la rama `develop` de [1ASI0572-2620-8725/report](https://github.com/1ASI0572-2620-8725/report/tree/develop). La sección 5.6 está vacía en esa rama. El comportamiento sale de las secciones 4.2.4 y 4.2.7.

## Qué hace la placa

1. Mide la señal física del informe: temperatura, humedad y etileno.
2. Arma un `SensorReading` con `deviceId`, `timestamp`, `temperatureC`, `humidityPct`, `ethylenePpm` y `meta`.
3. `isValid()` descarta una lectura fuera de un rango físico.
4. Envía `POST /api/v1/sensor-readings` con el token del dispositivo (TS41).
5. Envía `POST /api/v1/devices/{id}/heartbeat` para que el servidor no lo marque `OFFLINE` (US06).
6. Si el etileno supera 20 ppm (`HIGH_ETHYLENE`), enciende el LED rojo y hace sonar la bocina.
7. Si la temperatura supera 8 °C (`TEMP_RISK`), el relé enciende el ventilador.

El servidor sigue calculando `FRESH`, `AT_RISK` y `SPOILED`. En la cámara, el gas se avisa con el LED rojo y la bocina. Si la cámara se calienta, el relé enciende el ventilador. En la placa física ese motor es un ventilador: mueve el aire. No es un extractor.

## Diagrama de clases

El código del diagrama está en `docs/class-diagram.puml`. En el informe va en el apartado **5.6 IoT Device Design**, junto a la tabla de hardware. En el **4.2.7** solo se menciona que `HIGH_ETHYLENE` acciona el LED y la bocina, y que `TEMP_RISK` acciona el ventilador.

## Qué hace cada parte del código

| Parte | Archivo | Qué hace |
|---|---|---|
| `sketch.ino` | `sketch.ino` | Arranca el nodo y llama a `tick()` en cada vuelta. |
| `ClimateSensor` | `FreshSenseNode.h` | Lee temperatura y humedad del DHT22 en el GPIO 12. |
| `EthyleneSensor` | `FreshSenseNode.h` | Lee el sensor de gas en el GPIO 34 y lo convierte a `ethylenePpm`. |
| `ReadingClock` | `FreshSenseNode.h` | Arma el `timestamp` de la lectura. |
| `SensorReading` | `FreshSenseNode.h` | Junta los campos del informe, valida los rangos y escribe el JSON con saltos de línea. |
| `AlertActuators` | `FreshSenseNode.h` | LED rojo (GPIO 27) y bocina (GPIO 25) cuando el etileno supera 20 ppm. |
| `ColdRoomFan` | `FreshSenseNode.h` | Relé del ventilador (GPIO 26) cuando la temperatura supera 8 °C. |
| `DeviceLink` | `FreshSenseNode.h` | Wi-Fi y los dos POST: la lectura y el heartbeat. |
| `FreshSenseNode` | `FreshSenseNode.h` | Cada 10 segundos mide, decide los actuadores y envía. |

## Circuito

| Pieza | Conexión | Para qué, según el informe |
|---|---|---|
| DHT22 VCC | 3V3 | Temperatura y humedad |
| DHT22 GND | GND | Tierra |
| DHT22 SDA | GPIO 12 | `temperatureC` y `humidityPct` |
| Sensor de gas VCC | 5V | Etileno |
| Sensor de gas GND | GND | Tierra |
| Sensor de gas AOUT | GPIO 34 | `ethylenePpm` |
| LED rojo | GPIO 27 y resistencia de 220 ohm | Encendido en `HIGH_ETHYLENE` |
| Bocina | GPIO 25 | Pitido intermitente en `HIGH_ETHYLENE` |
| Relé del ventilador VCC | 5V | Alimenta el módulo |
| Relé del ventilador GND | GND | Tierra |
| Relé del ventilador IN | GPIO 26 | Enciende el ventilador en `TEMP_RISK` |

Wokwi no tiene un sensor de etileno. El sensor de gas llena `ethylenePpm`, que es un campo obligatorio de `SensorReading`.

## Cómo verlo

1. Abre un proyecto ESP32 en [wokwi.com](https://wokwi.com).
2. Copia `diagram.json`, `sketch.ino`, `FreshSenseNode.h` y `libraries.txt`.
3. Inicia la simulación y abre el monitor serial.
4. Cada 10 segundos aparece el JSON con saltos de línea y, debajo, el código HTTP de los dos POST.

El DHT22 arranca en 6.4 °C y 82 %. El ventilador parte apagado. Sube la temperatura del DHT22 por encima de 8 °C para ver el relé del ventilador. El sensor de gas abre cerca de 35 ppm, por encima de 20 ppm, así que el LED rojo parte encendido y la bocina pita. Baja el control del gas para apagarlos.

Sustituye `DEVICE_ID` por el UUID del alta (US31) y `DEVICE_TOKEN` por el token que valida el servidor (TS41). Si el API responde 401, el circuito y el JSON siguen siendo los del informe: falta ese token.
