# Guía: Crear el Bot de Telegram para el Dispensador de Comida

## Paso 1 — Crear el bot con @BotFather

1. Abre Telegram y busca **@BotFather** (cuenta oficial con tilde azul ✔️).
2. Escribe `/start` para ver el menú de opciones.
3. Escribe `/newbot` y sigue las instrucciones:
   - **Nombre del bot** (visible para el usuario): por ejemplo, `Dispensador Gato`
   - **Username del bot** (debe terminar en `bot`): por ejemplo, `dispensador_gato_koba_bot`
4. BotFather te responderá con tu **token**, que tiene este formato:
   ```
   123456789:ABCdefGhIJKlmNoPQRsTUVwxYZ
   ```
   ⚠️ **Guárdalo en un lugar seguro. No lo compartas con nadie.**

5. Copia ese token y pégalo en el sketch Arduino en la variable:
   ```cpp
   const char* BOT_TOKEN = "123456789:ABCdefGhIJKlmNoPQRsTUVwxYZ";
   ```

---

## Paso 2 — Obtener tu Chat ID

El bot necesita saber a quién enviarle los mensajes. Para eso necesitas tu **Chat ID numérico**.

### Método A — Usando @userinfobot (más fácil)
1. Busca **@userinfobot** en Telegram.
2. Escribe `/start`.
3. Te responde con tu **Id** numérico, por ejemplo: `987654321`
4. Copia ese número y pégalo en el sketch:
   ```cpp
   const char* CHAT_ID = "987654321";
   ```

### Método B — Usando la API de Telegram (alternativo)
1. Abre un navegador y ve a esta URL (reemplaza `TU_TOKEN`):
   ```
   https://api.telegram.org/botTU_TOKEN/getUpdates
   ```
2. Envía cualquier mensaje a tu bot desde Telegram.
3. Recarga la URL y busca el campo `"id"` dentro de `"chat"`.

---

## Paso 3 — Activar el bot

1. Busca el username de tu bot en Telegram (ej. `@dispensador_gato_koba_bot`).
2. Pulsa **INICIAR** o escribe `/start`.
3. A partir de este momento el bot puede enviarte mensajes.

> ⚠️ **Importante:** Telegram no permite que un bot envíe mensajes a un usuario que no haya iniciado la conversación primero. Siempre pulsa /start antes de flashear el ESP32.

---

## Paso 4 — Flashear el ESP32 y probar

1. Abre el sketch `dispensador_gato.ino` en Arduino IDE.
2. Completa la sección de configuración (Wi-Fi, MQTT, token, chat_id, distancias).
3. Selecciona la placa: **Tools → Board → ESP32 Dev Module**.
4. Selecciona el puerto COM correcto.
5. Sube el código (botón ▶ **Upload**).
6. Abre el **Monitor Serie** (115200 baudios) para ver los mensajes de arranque.
7. Escribe `/nivel` en el chat de Telegram → el bot debe responder con el nivel actual.

---

## Comandos disponibles en el bot

| Comando | Respuesta |
|---|---|
| `/start` | Menú de ayuda |
| `/ayuda` | Lista de comandos |
| `/nivel` | Nivel actual en % y distancia en cm |
| `/estado` | Igual que /nivel |

El bot también envía alertas automáticas sin que hagas nada:

| Evento | Mensaje automático |
|---|---|
| Nivel ≤ 30% | ⚠️ Nivel bajo |
| Nivel ≤ 5% | 🚨 Casi vacío |
| Nivel vuelve a > 35% | ✅ Dispensador repuesto |

---

## Paso 5 — Opcional: Desactivar el bot o cambiarle el nombre

Desde **@BotFather** puedes usar estos comandos en cualquier momento:

| Comando BotFather | Función |
|---|---|
| `/mybots` | Ver todos tus bots |
| `/setname` | Cambiar el nombre visible |
| `/setdescription` | Cambiar la descripción |
| `/setuserpic` | Cambiar la foto de perfil del bot |
| `/deletebot` | Eliminar el bot permanentemente |

---

## Resumen rápido

```
@BotFather → /newbot → copiar TOKEN → pegarlo en el sketch
@userinfobot → copiar tu ID → pegarlo en el sketch
Abrir tu bot en Telegram → pulsar /start
Flashear ESP32 → escribir /nivel → ¡listo!
```
