# Dispensador de Comida para Gato – Sensor de Nivel

Sensor de nivel de comida para el dispensador automático **KOBA 7L**, usando un **ESP32** y un sensor de distancia **VL53L0X** (ToF). Notificaciones vía **Telegram Bot** y publicación de datos por **MQTT** (compatible con Home Assistant).

## Componentes

- ESP32 (DevKit)
- Sensor VL53L0X (I2C, ToF láser)
- Cables Dupont (4 cables)

## Conexión

| Pin VL53L0X | Pin ESP32 |
|---|---|
| VCC/VIN | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

## Estructura del repositorio

```
dispensador_gato/
  dispensador_gato.ino       ← Sketch Arduino para ESP32
homeassistant/
  homeassistant_dispensador.yaml  ← Configuración HA (sin packages)
  packages/
    dispensador_gato.yaml    ← Package todo-en-uno para HA
docs/
  guia_telegram_bot.md       ← Guía paso a paso para crear el bot
```

## Librerías necesarias

- Adafruit VL53L0X
- PubSubClient
- UniversalTelegramBot
- ArduinoJson

## Características

- Medición de nivel con filtro de mediana (7 muestras)
- Cálculo de porcentaje restante en tiempo real
- Alertas automáticas por Telegram (nivel bajo ≤30% y casi vacío ≤5%)
- Comandos del bot: `/nivel`, `/estado`, `/ayuda`
- Publicación MQTT con `retain=true` (3 topics: nivel, distancia, estado)
- Compatible con Home Assistant (sensores, automatizaciones y tarjeta Lovelace incluidos)

## Licencia

MIT
