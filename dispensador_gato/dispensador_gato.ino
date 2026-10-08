/*
 ============================================================
  SENSOR DE NIVEL – DISPENSADOR DE COMIDA PARA GATO (7 L)
  Hardware : ESP32 + VL53L0X (I2C)
  Notifica : Telegram Bot + MQTT (Home Assistant / Node-RED)
  IDE      : Arduino IDE 2.x
 ============================================================

  LIBRERÍAS NECESARIAS (instalar desde el Gestor de Librerías):
  ─────────────────────────────────────────────────────────────
  1. "Adafruit VL53L0X"          by Adafruit         (>= 1.2.0)
  2. "PubSubClient"              by Nick O'Leary     (>= 2.8.0)
  3. "UniversalTelegramBot"      by Brian Lough       (>= 1.3.0)
  4. "ArduinoJson"               by Benoit Blanchon  (>= 6.21.0)
  5. "WiFi"                      (incluida en el core ESP32)

  SOPORTE DE PLACA ESP32 en Arduino IDE:
  ─────────────────────────────────────
  Preferences → Additional Boards Manager URLs:
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  Luego: Tools → Board → Boards Manager → buscar "esp32" → instalar.

  CONEXIÓN VL53L0X → ESP32
  ─────────────────────────
  VCC/VIN  ──►  3.3 V
  GND      ──►  GND
  SDA      ──►  GPIO 21
  SCL      ──►  GPIO 22

  CONFIGURACIÓN (ajusta los valores de esta sección):
  ─────────────────────────────────────────────────────
*/

// ─── Wi-Fi ────────────────────────────────────────────────
const char* WIFI_SSID     = "TU_RED_WIFI";
const char* WIFI_PASSWORD = "TU_CONTRASEÑA";

// ─── MQTT ────────────────────────────────────────────────
const char* MQTT_SERVER   = "192.168.1.100";   // IP de tu broker (ej. Mosquitto/HA)
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "";                 // dejar vacío si no hay auth
const char* MQTT_PASS     = "";
const char* MQTT_CLIENT_ID = "esp32_dispensador_gato";

// Topics MQTT publicados
const char* TOPIC_NIVEL     = "mascota/dispensador/nivel_pct";   // 0..100
const char* TOPIC_DISTANCIA = "mascota/dispensador/distancia_cm";
const char* TOPIC_ESTADO    = "mascota/dispensador/estado";      // "OK" | "BAJO" | "VACIO"

// ─── Telegram ────────────────────────────────────────────
const char* BOT_TOKEN   = "123456789:ABCdefGhIJKlmNoPQRsTUVwxYZ";  // token BotFather
const char* CHAT_ID     = "987654321";                               // tu chat_id numérico

// ─── Calibración del dispensador ─────────────────────────
// Mide estas distancias con una regla tras instalar el sensor en la tapa:
const float D_VACIO_CM = 35.0;   // distancia sensor→fondo cuando está VACÍO
const float D_LLENO_CM =  5.0;   // distancia sensor→croquetas cuando está LLENO

// ─── Umbrales de alerta ───────────────────────────────────
const float UMBRAL_BAJO_PCT  = 30.0;   // % al que notifica "nivel bajo"
const float UMBRAL_VACIO_PCT =  5.0;   // % al que notifica "casi vacío"

// ─── Intervalos (ms) ─────────────────────────────────────
const unsigned long INTERVALO_MEDICION_MS = 30000UL;   // mide cada 30 s
const unsigned long INTERVALO_MQTT_MS     = 60000UL;   // publica MQTT cada 60 s
const unsigned long INTERVALO_TELEGRAM_MS = 300000UL;  // reenvía alerta Telegram mín. cada 5 min

// ─── Filtro de mediana ────────────────────────────────────
const int MEDIANA_N = 7;   // número de muestras para la mediana (impar)

// ═════════════════════════════════════════════════════════
//  NO MODIFIQUES NADA A PARTIR DE AQUÍ (salvo depuración)
// ═════════════════════════════════════════════════════════

#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ── Objetos globales ──────────────────────────────────────
Adafruit_VL53L0X sensor;
WiFiClient        wifiClient;
WiFiClientSecure  secureClient;
PubSubClient      mqttClient(wifiClient);
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

// ── Estado interno ────────────────────────────────────────
float    nivelActual_pct   = -1.0;
float    distActual_cm     = -1.0;
bool     alertaBajoEnviada = false;
bool     alertaVacioEnviada= false;

unsigned long ultimaMedicion  = 0;
unsigned long ultimaMQTT      = 0;
unsigned long ultimaTelegram  = 0;

// ─────────────────────────────────────────────────────────
//  UTILIDADES
// ─────────────────────────────────────────────────────────

// Ordena un arreglo por burbuja (in-place) para calcular mediana
void ordenar(float arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - 1 - i; j++) {
      if (arr[j] > arr[j + 1]) {
        float tmp = arr[j]; arr[j] = arr[j + 1]; arr[j + 1] = tmp;
      }
    }
  }
}

// Lee MEDIANA_N veces el sensor y devuelve la mediana de distancias en cm.
// Retorna -1.0 si no se pudo leer ninguna muestra válida.
float leerDistanciaFiltrada() {
  float muestras[MEDIANA_N];
  int validas = 0;

  for (int i = 0; i < MEDIANA_N; i++) {
    VL53L0X_RangingMeasurementData_t medida;
    sensor.rangingTest(&medida, false);

    if (medida.RangeStatus != 4) {           // 4 = fuera de rango / error
      float cm = medida.RangeMilliMeter / 10.0;
      if (cm > 0.5 && cm < 200.0) {          // descarta lecturas absurdas
        muestras[validas++] = cm;
      }
    }
    delay(30);   // pequeña pausa entre muestras
  }

  if (validas == 0) return -1.0;

  ordenar(muestras, validas);
  return muestras[validas / 2];             // mediana
}

// Convierte distancia en cm a porcentaje de nivel (0–100 %)
float distanciaAPorcentaje(float dist_cm) {
  if (dist_cm < 0) return -1.0;
  float pct = ((D_VACIO_CM - dist_cm) / (D_VACIO_CM - D_LLENO_CM)) * 100.0;
  if (pct < 0.0)   pct = 0.0;
  if (pct > 100.0) pct = 100.0;
  return pct;
}

// Devuelve etiqueta de estado según el porcentaje
String estadoTexto(float pct) {
  if (pct < 0)                         return "SIN_DATO";
  if (pct <= UMBRAL_VACIO_PCT)         return "VACIO";
  if (pct <= UMBRAL_BAJO_PCT)          return "BAJO";
  return "OK";
}

// ─────────────────────────────────────────────────────────
//  TELEGRAM
// ─────────────────────────────────────────────────────────

void enviarTelegram(const String& mensaje) {
  bot.sendMessage(CHAT_ID, mensaje, "");
  Serial.println("[Telegram] Enviado: " + mensaje);
}

void gestionarAlertasTelegram(float pct) {
  unsigned long ahora = millis();

  // Alerta "casi vacío" (prioridad alta, notifica siempre que se cumpla)
  if (pct <= UMBRAL_VACIO_PCT) {
    if (!alertaVacioEnviada || (ahora - ultimaTelegram >= INTERVALO_TELEGRAM_MS)) {
      String msg = "🚨 *Dispensador gato: CASI VACÍO*\n"
                   "Nivel: " + String(pct, 1) + "% (≤ " + String(UMBRAL_VACIO_PCT, 0) + "%)\n"
                   "¡Recarga el dispensador cuanto antes!";
      enviarTelegram(msg);
      alertaVacioEnviada = true;
      ultimaTelegram = ahora;
    }
    return;
  }
  alertaVacioEnviada = false;

  // Alerta "nivel bajo"
  if (pct <= UMBRAL_BAJO_PCT) {
    if (!alertaBajoEnviada || (ahora - ultimaTelegram >= INTERVALO_TELEGRAM_MS)) {
      String msg = "⚠️ *Dispensador gato: Nivel bajo*\n"
                   "Nivel: " + String(pct, 1) + "% (≤ " + String(UMBRAL_BAJO_PCT, 0) + "%)\n"
                   "Considera recargar pronto.";
      enviarTelegram(msg);
      alertaBajoEnviada = true;
      ultimaTelegram = ahora;
    }
    return;
  }

  // Nivel volvió a ser normal → resetea flags para próxima alerta
  if (alertaBajoEnviada && pct > UMBRAL_BAJO_PCT + 5.0) {
    alertaBajoEnviada = false;
    enviarTelegram("✅ *Dispensador gato: Nivel repuesto*\nNivel actual: " + String(pct, 1) + "%");
  }
}

// Maneja comandos enviados al bot (responde /nivel y /estado)
void procesarComandosTelegram() {
  int numMensajes = bot.getUpdates(bot.last_message_received + 1);
  while (numMensajes) {
    for (int i = 0; i < numMensajes; i++) {
      String texto    = bot.messages[i].text;
      String remitente= bot.messages[i].chat_id;
      texto.toLowerCase();

      if (texto == "/nivel" || texto == "/estado") {
        String resp = "🐱 *Dispensador de comida*\n";
        if (nivelActual_pct < 0) {
          resp += "Sin lectura disponible aún.";
        } else {
          resp += "Nivel: " + String(nivelActual_pct, 1) + "%\n";
          resp += "Distancia: " + String(distActual_cm, 1) + " cm\n";
          resp += "Estado: " + estadoTexto(nivelActual_pct);
        }
        bot.sendMessage(remitente, resp, "Markdown");
      } else if (texto == "/ayuda" || texto == "/help" || texto == "/start") {
        bot.sendMessage(remitente,
          "🤖 *Comandos disponibles:*\n"
          "/nivel — Muestra el nivel actual\n"
          "/estado — Igual que /nivel\n"
          "/ayuda — Esta pantalla", "Markdown");
      }
    }
    numMensajes = bot.getUpdates(bot.last_message_received + 1);
  }
}

// ─────────────────────────────────────────────────────────
//  MQTT
// ─────────────────────────────────────────────────────────

void publicarMQTT() {
  if (!mqttClient.connected()) return;

  char buf[16];

  dtostrf(nivelActual_pct, 5, 1, buf);
  mqttClient.publish(TOPIC_NIVEL, buf, true);       // retain = true

  dtostrf(distActual_cm, 5, 1, buf);
  mqttClient.publish(TOPIC_DISTANCIA, buf, true);

  String estado = estadoTexto(nivelActual_pct);
  mqttClient.publish(TOPIC_ESTADO, estado.c_str(), true);

  Serial.println("[MQTT] Publicado → nivel:" + String(nivelActual_pct, 1) +
                 "% dist:" + String(distActual_cm, 1) + "cm estado:" + estado);
}

bool reconectarMQTT() {
  int intentos = 0;
  while (!mqttClient.connected() && intentos < 3) {
    Serial.print("[MQTT] Conectando... ");
    bool ok = (strlen(MQTT_USER) > 0)
              ? mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)
              : mqttClient.connect(MQTT_CLIENT_ID);
    if (ok) {
      Serial.println("conectado.");
      return true;
    }
    Serial.println("fallo rc=" + String(mqttClient.state()));
    intentos++;
    delay(2000);
  }
  return false;
}

// ─────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Dispensador Gato – Iniciando ===");

  // I2C y sensor VL53L0X
  Wire.begin(21, 22);   // SDA=21, SCL=22
  if (!sensor.begin()) {
    Serial.println("[ERROR] VL53L0X no encontrado. Revisa el cableado.");
    while (true) delay(1000);   // detiene ejecución hasta reinicio
  }
  sensor.setMeasurementTimingBudgetMicroSeconds(200000);   // 200 ms → mayor precisión
  Serial.println("[OK] VL53L0X listo.");

  // Wi-Fi
  Serial.print("[WiFi] Conectando a " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int intentosWifi = 0;
  while (WiFi.status() != WL_CONNECTED && intentosWifi < 30) {
    delay(500); Serial.print("."); intentosWifi++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Conectado. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Sin conexión. El dispositivo seguirá midiendo localmente.");
  }

  // MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  reconectarMQTT();

  // Telegram – acepta certificados de Telegram
  secureClient.setInsecure();

  // Primera medición inmediata
  distActual_cm   = leerDistanciaFiltrada();
  nivelActual_pct = distanciaAPorcentaje(distActual_cm);
  Serial.printf("[Inicio] Distancia: %.1f cm | Nivel: %.1f%%\n",
                distActual_cm, nivelActual_pct);

  ultimaMedicion = ultimaMQTT = ultimaTelegram = millis();

  Serial.println("=== Setup completado ===\n");
}

// ─────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────
void loop() {
  unsigned long ahora = millis();

  // ── Mantener MQTT vivo ───────────────────────────────
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) reconectarMQTT();
    mqttClient.loop();
  }

  // ── Lectura del sensor ───────────────────────────────
  if (ahora - ultimaMedicion >= INTERVALO_MEDICION_MS) {
    ultimaMedicion = ahora;

    float dist = leerDistanciaFiltrada();
    if (dist < 0) {
      Serial.println("[Sensor] Lectura inválida, se omite.");
    } else {
      distActual_cm   = dist;
      nivelActual_pct = distanciaAPorcentaje(dist);
      Serial.printf("[Sensor] Distancia: %.1f cm | Nivel: %.1f%% | Estado: %s\n",
                    distActual_cm, nivelActual_pct, estadoTexto(nivelActual_pct).c_str());
    }
  }

  // ── Publicación MQTT ─────────────────────────────────
  if (ahora - ultimaMQTT >= INTERVALO_MQTT_MS && nivelActual_pct >= 0) {
    ultimaMQTT = ahora;
    publicarMQTT();
  }

  // ── Alertas y comandos Telegram ──────────────────────
  if (WiFi.status() == WL_CONNECTED && nivelActual_pct >= 0) {
    // Alertas automáticas
    gestionarAlertasTelegram(nivelActual_pct);

    // Comandos del usuario cada 2 s (sin bloquear)
    static unsigned long ultimoCheckTelegram = 0;
    if (ahora - ultimoCheckTelegram >= 2000) {
      ultimoCheckTelegram = ahora;
      procesarComandosTelegram();
    }
  }
}
