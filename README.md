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

La protoboard junta las tierras. Del ESP32 sale un solo cable negro al riel GND, uno rojo de 3V3 y uno rojo de 5V. Cada pieza toma tierra de ese riel, no del pin GND del ESP32.

| Pieza en el dibujo | Conexión | Qué es |
|---|---|---|
| DHT22 | Dato en GPIO 12, 3V3 y riel GND | Temperatura y humedad |
| Placa azul «Etileno» | Señal en GPIO 34, 5V y riel GND | Sensor de gas, el campo `ethylenePpm` |
| LED rojo «ALERTA» | GPIO 27 y resistencia de 220 ohm al riel GND | Aviso de `HIGH_ETHYLENE` |
| Círculo negro «Bocina» | GPIO 25 y riel GND | Bocina de `HIGH_ETHYLENE` |
| Módulo rojo «Relé» | GPIO 26, 5V y riel GND | Interruptor del ventilador. Se cierra en `TEMP_RISK` |
| Pieza «Ventilador» | Mismo GPIO 26, 5V y riel GND | Ventilador dibujado en Wokwi. Las aspas giran cuando el relé está cerrado. En la placa física el motor es un ventilador |

Wokwi no tiene un sensor de etileno. El sensor de gas llena `ethylenePpm`, que es un campo obligatorio de `SensorReading`. Wokwi tampoco tiene un ventilador que gire: `ventilador.chip.c` dibuja las aspas. En la placa física el motor es un ventilador y el relé le da la corriente.

## Qué es cada pieza y si funciona

El circuito mide y actúa en el simulador. Cada 10 segundos arma la lectura y decide los actuadores. El envío al servidor puede responder 401 mientras `DEVICE_TOKEN` sea un texto de relleno. El circuito local no depende de esa respuesta.

| Pieza | Qué es | Qué hace al arrancar |
|---|---|---|
| DHT22 | Sensor de temperatura y humedad. El dato va por el cable verde al GPIO 12. | Arranca en 6.4 °C y 82 %. |
| Etileno | Placa azul. Sensor de gas leído como etileno por el cable naranja, GPIO 34. | Abre cerca de 35 ppm. |
| Protoboard | Rieles de 3.3 V, 5 V y tierra. Del ESP32 sale un cable de cada uno. | Las demás piezas toman corriente y tierra de ahí. |
| Ventilador | Círculo de aspas. Pieza dibujada para el simulador. | Quieto, porque la cámara está a 6.4 °C. Gira si la temperatura pasa de 8 °C. |
| Relé | Módulo rojo. Interruptor del ventilador, GPIO 26. | Abierto al arrancar. Se cierra junto con las aspas. |
| Bocina | Círculo negro, GPIO 25. | Pita, porque el gas está por encima de 20 ppm. |
| LED rojo «ALERTA» | Aviso de gas, GPIO 27, con resistencia de 220 ohm. | Encendido por la misma alerta de gas. |
| ESP32 | Microcontrolador. Lee, actúa y manda la lectura por Wi-Fi. | Publica la lectura y el heartbeat cada 10 segundos. |

Con esos valores de arranque el LED está encendido y la bocina pita. El ventilador sigue apagado.

## Cómo verlo

1. Abre un proyecto ESP32 en [wokwi.com](https://wokwi.com).
2. Copia `diagram.json`, `sketch.ino`, `FreshSenseNode.h`, `libraries.txt`, `ventilador.chip.c` y `ventilador.chip.json`.
3. Inicia la simulación y abre el monitor serial.
4. Cada 10 segundos aparece el JSON con saltos de línea y, debajo, el código HTTP de los dos POST.

El DHT22 arranca en 6.4 °C y 82 %. El ventilador parte apagado. Sube la temperatura del DHT22 por encima de 8 °C para ver el relé del ventilador. El sensor de gas abre cerca de 35 ppm, por encima de 20 ppm, así que el LED rojo parte encendido y la bocina pita. Baja el control del gas para apagarlos.

Sustituye `DEVICE_ID` por el UUID del alta (US31) y `DEVICE_TOKEN` por el token que valida el servidor (TS41). Si el API responde 401, el circuito y el JSON siguen siendo los del informe: falta ese token.
