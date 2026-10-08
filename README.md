# FreshSense — prototipo del dispositivo en Wokwi

Simulación publicada: https://wokwi.com/projects/477332077901100033

Nodo de cámara fría de FreshSense. El circuito es el de la sección 5.6 del informe: **ESP32 DevKit C v4** y **DHT22**, con el dato en **GPIO 12**, alimentación en **3V3** y **GND**.

Al arrancar, el sensor sale en **6.4 °C** y **82 %**, la lectura de ejemplo del informe. Durante la simulación se puede mover la temperatura y la humedad haciendo clic en el DHT22.

## Circuito

| DHT22 | ESP32 DevKit C v4 | Cable |
| --- | --- | --- |
| VCC | 3V3 | Rojo |
| GND | GND.1 | Negro |
| SDA | GPIO 12 | Verde |

El LED de la placa (GPIO 2) parpadea lento si la cámara está en rango, rápido si la temperatura sale de rango y muy lento si no hay Wi-Fi.

## Cómo abrirlo

1. Entra a [wokwi.com](https://wokwi.com) y crea un proyecto **ESP32**.
2. Sustituye `diagram.json`, `sketch.ino` y `FreshSenseNode.h` por los de esta carpeta.
3. Crea `libraries.txt` con el mismo contenido.
4. Pulsa **Play** y abre el **Serial Monitor** a 115200.

En el monitor debe verse el identificador `esp32-cocina-01`, el estado `FRESH` y un JSON como este:

```json
{"deviceId":"esp32-cocina-01","id":"rd-000001","temperature":6.4,"humidity":82.0,"time":"08/10/2026 11:20"}
```

Sube el slider del DHT22 por encima de 8 °C para ver `AT_RISK`, y por encima de 12 °C para ver `TEMP_RISK`. Esas etiquetas salen solo por serial: el JSON que viaja al API lleva temperatura, humedad, identificador y hora, como en el informe.

## Red

En el simulador la red es `Wokwi-GUEST`, canal 6, sin clave. En la placa física hay que cambiar `WIFI_SSID` y `WIFI_PASSWORD` por la red del local.

Cada lectura se envía con `POST` a `https://35-224-123-160.sslip.io/api/edge/readings` y la cabecera `X-Device-Key`. Sustituye `replace-with-device-key` por la clave que entrega la aplicación al registrar el dispositivo. Si el API responde 401, el circuito y la lectura siguen siendo válidos: falta esa clave.

En la placa real, GPIO 12 es un pin de arranque (MTDI). Si la placa no inicia con el DHT22 conectado, mueve el dato a GPIO 4 y cambia `DHT_PIN` y el cable verde.
