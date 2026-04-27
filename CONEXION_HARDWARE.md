# Conexión de 2 matrices 8x8 RGB + fuente + ESP (ESP-01 / ESP8266)

Este documento explica un cableado **seguro y recomendado** para 2 matrices de 8x8 LEDs RGB direccionables (tipo WS2812B/NeoPixel), una fuente externa y un módulo ESP-01/ESP-01S.

---

## 1) Resumen rápido de arquitectura

- **Matriz 1 DIN** recibe datos desde el ESP.
- **Matriz 2 DIN** recibe datos desde **DOUT de Matriz 1**.
- Todas las matrices comparten la **misma fuente de 5V**.
- El ESP se alimenta según tu placa: **5V (si tiene regulador onboard)** o **3.3V regulados** si es módulo ESP-01/ESP-01S puro.
- **GND común obligatorio** entre fuente de LEDs y ESP.

---

## 2) Material recomendado

- 2 x matriz 8x8 RGB direccionable (64 LEDs c/u).
- 1 x ESP-01 / ESP-01S (ESP8266).
- 1 x fuente 5V DC con corriente suficiente (ver cálculo abajo).
- 1 x regulador 5V -> 3.3V estable para el ESP (si no lo tienes integrado).
- 1 x resistor de **330–470 Ω** en la línea de datos.
- 1 x condensador electrolítico **1000 µF / 6.3V o superior** entre +5V y GND (cerca de las matrices).
- (Opcional recomendado) 1 x level shifter 3.3V->5V para señal DATA (74AHCT125/74HCT14).

---

## 3) Cálculo de fuente de alimentación

Total LEDs = 2 × 64 = **128 LEDs**.

Consumo teórico máximo WS2812B ≈ **60 mA por LED** (blanco al 100%).

- 128 × 0.06 A = **7.68 A** (pico teórico máximo)

Recomendación práctica para acuario (intensidad moderada):

- En tu caso concreto: **fuente 5V 8A**.
- Es suficiente para 2x8x8 si mantienes margen de seguridad en intensidad.
- El firmware limita a `MAX_SAFE_INTENSITY = 200` para dejar margen térmico y de picos.

---

### 3.1 Ajuste aplicado para tu fuente (5V 8A)

Con 128 LEDs, el máximo teórico es 7.68A.

Para trabajar con margen, el proyecto fija un límite de intensidad (`MAX_SAFE_INTENSITY = 200`) que capea:

- intensidades automáticas (luna/día/nublado),
- intensidad manual desde la web,
- y el escalado final aplicado a cada color.

## 4) Esquema de conexión (texto)

### 4.1 Alimentación de LEDs

- Fuente 5V `+` -> `+5V` de Matriz 1 y Matriz 2 (en paralelo).
- Fuente 5V `-` -> `GND` de Matriz 1 y Matriz 2 (en paralelo).
- Colocar condensador 1000 µF entre `+5V` y `GND` cerca de la primera matriz.

### 4.2 Cadena de datos

- ESP GPIO de datos (en el firmware actual: `GPIO2`) -> **resistor 330–470 Ω** -> `DIN` Matriz 1.
- `DOUT` Matriz 1 -> `DIN` Matriz 2.

### 4.3 Masa común

- `GND` del ESP -> `GND` de la fuente de 5V (masa común).

> Sin masa común, los LEDs pueden parpadear o no responder.

---

## 5) Alimentación del ESP (tu caso actual: 5V)

Como indicas que tu ESP actual se alimenta a **5V**, asumimos que es una placa con regulador integrado (por ejemplo, pin `5V`/`VIN`).

### Si tu placa acepta 5V de entrada

- Entrada `5V`/`VIN` de la placa -> 5V de la fuente.
- GND de la placa -> GND común.

### Si usas un ESP-01 / ESP-01S "puro" (sin regulador)

El chip trabaja a **3.3V**, por lo que en ese caso:

- VCC ESP-01 -> salida 3.3V regulada.
- GND ESP-01 -> GND común.
- CH_PD/EN -> 3.3V (pull-up, normalmente 10k).
- RST -> pull-up a 3.3V (10k, opcional según placa).

Para programar ESP-01 habitualmente:

- GPIO0 a GND durante arranque para modo flash.
- Luego GPIO0 en alto para arranque normal.

---

## 6) Señal de datos 3.3V -> LEDs 5V

Muchas tiras/matrices WS2812 aceptan 3.3V en DIN si están cerca y cable corto, pero no siempre.

Recomendado para robustez:

- usar **level shifter** de 3.3V a 5V (familia AHCT/HCT).
- mantener cable de datos corto.
- evitar pasar junto a cables de potencia largos.

---

## 7) Protección y buenas prácticas

1. **Conecta GND primero**, luego +5V.
2. No conectes/desconectes matrices con la fuente encendida.
3. Inyecta alimentación en más de un punto si notas caída de tensión (panel grande/cables largos).
4. Usa cables de sección adecuada para corriente alta.
5. Si hay reinicios del ESP, mejora la regulación de 3.3V y separación de potencia/señal.

---

## 8) Relación con el firmware del repositorio

En `firmware/aquarium_panel.ino` está definido:

- `LED_PIN = 2` (GPIO2)
- `MATRIX_COUNT = 2`
- `LEDS_PER_MATRIX = 64`

Si usas otro pin de datos, cambia `LED_PIN` en el firmware.

---

## 9) Diagrama ASCII rápido

```text
             +---------------------------+
             |       FUENTE 5V DC        |
             |      +5V ----------+-------+-------------------> +5V M1/M2
             |      GND ----------+-------+-------------------> GND M1/M2
             +---------------------------+
                                 |
                                 +-----------------------------> GND ESP

ESP (placa con entrada 5V o ESP-01 a 3.3V)
  GPIO2 ---[330R]---> DIN M1 ---> DOUT M1 ---> DIN M2
  VIN/5V <--- 5V (si placa con regulador)
  VCC    <--- 3.3V (si ESP-01 puro)
  GND  <--- GND común

(Condensador 1000uF entre +5V y GND cerca de M1)
```

---

## 10) Nota sobre “ESP32 01”

En la práctica, “ESP-01” suele referirse al módulo **ESP8266 ESP-01/ESP-01S**.

Si tu placa real es un **ESP32**, el concepto de cableado de LEDs/fuente es el mismo, pero:

- cambian pines disponibles,
- cambian librerías de red (`WiFi.h`/`WebServer.h` en vez de `ESP8266WiFi.h`/`ESP8266WebServer.h`).
