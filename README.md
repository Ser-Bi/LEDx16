# LEDx16 - Panel LED para acuario (2 matrices 8x8 RGB)

Este repositorio ahora incluye un firmware base para controlar **2 matrices de 8x8 LEDs RGB (128 LEDs en total)** con un ESP, creando ciclos de iluminación para acuario:

- **Noche**: lila/fuxia tenue con solo los 4 LEDs centrales por matriz.
- **Salida de sol**: transición gradual desde noche a espectro completo (con dominante roja inicial).
- **Horas de sol**: espectro blanco/rojo/azul, bloque nublado, descanso apagado, y vuelta a nublado/espectro.
- **Atardecer**: paso de espectro completo a tonos cálidos/rojizos y luego noche.
- **Control manual** y **configuración de horarios/intensidades** por servidor web.

## Archivo principal

- `firmware/aquarium_panel.ino`

## Hardware esperado

- 1 controlador ESP8266/ESP32 compatible con Arduino.
- 2 matrices RGB de 8x8 direccionables (tipo WS2812/NeoPixel).
- Fuente adecuada para el consumo total de LEDs.

> Nota: en el código se usan librerías de ESP8266 (`ESP8266WiFi.h`, `ESP8266WebServer.h`).
> Si usas un ESP32, cambia esas cabeceras por `WiFi.h` y `WebServer.h`.

## Dependencias Arduino

- `Adafruit NeoPixel`
- (Core) `ESP8266` para Arduino

## Configuración rápida

1. Abre `firmware/aquarium_panel.ino` en Arduino IDE.
2. Cambia:
   - `WIFI_SSID`
   - `WIFI_PASS`
3. Sube el firmware al microcontrolador.
4. Entra en la IP del dispositivo desde el navegador.

## Qué se puede configurar desde la web

- Horarios (`HH:MM`) de cada fase:
  - inicio amanecer
  - inicio modo día
  - inicio nublado
  - inicio/fin descanso
  - inicio atardecer
  - inicio noche
- Intensidades de luna, día y nublado.
- Zona horaria POSIX (por ejemplo, España peninsular: `CET-1CEST,M3.5.0/2,M10.5.0/3`).
- Modo manual (RGB + intensidad), para forzar color temporalmente.

## Comportamiento implementado (resumen)

- **Noche**: solo 4 LEDs centrales por matriz con color lunar tenue.
- **Amanecer**: dos etapas automáticas:
  1. lunar → cálido rojizo
  2. cálido rojizo → espectro de día
- **Día**: espectro completo.
- **Nublado**: transición a espectro más suave.
- **Descanso**: apagado total.
- **Post-descanso**: transición de nublado a día.
- **Atardecer**: transición de día a cálido.
- Fuera de horario: vuelve a noche.

## Mejoras sugeridas para producción

- Guardar configuración en memoria persistente (LittleFS/EEPROM).
- Añadir autenticación al servidor web.
- Curvas gamma y balance por canal para calibración fina.
- Efecto nubes dinámico aleatorio suave.
- Endpoint REST para integrarlo con Home Assistant.

## Conexión de hardware

- Ver guía paso a paso en `CONEXION_HARDWARE.md` (matrices, fuente, GND común, ESP-01/ESP32 y recomendaciones de seguridad).

## Límite por fuente 5V 8A (con 2x8x8)

Con 2 matrices de 8x8 (128 LEDs), el máximo teórico en blanco al 100% es:

- **128 × 60 mA ≈ 7.68 A**.

Con una fuente de **5V 8A** este escenario es viable, pero conviene mantener margen:

- el firmware fija `MAX_SAFE_INTENSITY = 200`,
- evita mantener blanco 100% durante largos periodos,
- y vigila temperatura de fuente/cables en pruebas iniciales.
