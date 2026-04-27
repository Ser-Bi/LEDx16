#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

// ------------------------ Hardware ------------------------
static const uint8_t LED_PIN = 2;          // GPIO2 (D4 en NodeMCU)
static const uint16_t LEDS_PER_MATRIX = 64;
static const uint8_t MATRIX_COUNT = 2;
static const uint16_t TOTAL_LEDS = LEDS_PER_MATRIX * MATRIX_COUNT;

// Límite de seguridad por fuente (5V 8A para 2x8x8 = 128 LEDs).
// Máximo teórico: 128 * 60mA = 7.68A.
// Se usa 200/255 (~78%) para dejar margen térmico y de picos.
static const uint8_t MAX_SAFE_INTENSITY = 200;

Adafruit_NeoPixel strip(TOTAL_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

// ------------------------ WiFi ----------------------------
const char* WIFI_SSID = "TU_SSID";
const char* WIFI_PASS = "TU_PASSWORD";

// Timezone POSIX string (ejemplo España peninsular)
String timezonePosix = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// --------------------- Configuración ----------------------
struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct ScheduleConfig {
  // Horarios HH:MM
  String sunriseStart = "08:00";
  String dayStart = "09:00";
  String cloudyStart = "13:00";
  String breakStart = "14:00";
  String breakEnd = "16:00";
  String sunsetStart = "20:00";
  String nightStart = "21:00";

  // Intensidades 0..255
  uint8_t moonIntensity = 14;
  uint8_t dayIntensity = 180;
  uint8_t cloudyIntensity = 110;

  // Colores base
  Rgb moonColor = {180, 40, 190};
  Rgb sunriseWarm = {255, 40, 10};
  Rgb daySpectrum = {180, 220, 255};
  Rgb cloudySpectrum = {120, 140, 170};
  Rgb sunsetWarm = {255, 90, 20};
};

ScheduleConfig cfg;

// Modo manual
bool manualOverride = false;
Rgb manualColor = {255, 255, 255};
uint8_t manualIntensity = 64;

// --------------------- Utilidades -------------------------
int parseMinutes(const String& hhmm) {
  if (hhmm.length() != 5 || hhmm.charAt(2) != ':') return -1;
  int hh = hhmm.substring(0, 2).toInt();
  int mm = hhmm.substring(3, 5).toInt();
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
  return hh * 60 + mm;
}

Rgb lerpColor(const Rgb& a, const Rgb& b, float t) {
  t = constrain(t, 0.0f, 1.0f);
  Rgb out;
  out.r = static_cast<uint8_t>(a.r + (b.r - a.r) * t);
  out.g = static_cast<uint8_t>(a.g + (b.g - a.g) * t);
  out.b = static_cast<uint8_t>(a.b + (b.b - a.b) * t);
  return out;
}

uint32_t scaleColor(const Rgb& c, uint8_t intensity) {
  intensity = min(intensity, MAX_SAFE_INTENSITY);
  uint8_t r = (static_cast<uint16_t>(c.r) * intensity) / 255;
  uint8_t g = (static_cast<uint16_t>(c.g) * intensity) / 255;
  uint8_t b = (static_cast<uint16_t>(c.b) * intensity) / 255;
  return strip.Color(r, g, b);
}

// 4 LEDs centrales de cada matriz 8x8 (indices locales)
const uint8_t centerIndices[4] = {27, 28, 35, 36};

void setAll(uint32_t color) {
  for (uint16_t i = 0; i < TOTAL_LEDS; i++) strip.setPixelColor(i, color);
}

void setCentersOnly(uint32_t color) {
  strip.clear();
  for (uint8_t m = 0; m < MATRIX_COUNT; m++) {
    uint16_t base = m * LEDS_PER_MATRIX;
    for (uint8_t i = 0; i < 4; i++) {
      strip.setPixelColor(base + centerIndices[i], color);
    }
  }
}

void applyNight() {
  setCentersOnly(scaleColor(cfg.moonColor, cfg.moonIntensity));
}

void applyManual() {
  setAll(scaleColor(manualColor, manualIntensity));
}

void applyTransition(int now, int start, int end, const Rgb& fromC, uint8_t fromI, const Rgb& toC, uint8_t toI, bool centersAtStart = false) {
  if (end <= start) {
    setAll(scaleColor(toC, toI));
    return;
  }
  float t = float(now - start) / float(end - start);
  Rgb c = lerpColor(fromC, toC, t);
  uint8_t intensity = static_cast<uint8_t>(fromI + (toI - fromI) * constrain(t, 0.0f, 1.0f));

  if (centersAtStart && t < 0.15f) {
    setCentersOnly(scaleColor(c, intensity));
  } else {
    setAll(scaleColor(c, intensity));
  }
}

void runScheduler() {
  time_t now = time(nullptr);
  struct tm localTm;
  localtime_r(&now, &localTm);
  int current = localTm.tm_hour * 60 + localTm.tm_min;

  int sunriseStart = parseMinutes(cfg.sunriseStart);
  int dayStart = parseMinutes(cfg.dayStart);
  int cloudyStart = parseMinutes(cfg.cloudyStart);
  int breakStart = parseMinutes(cfg.breakStart);
  int breakEnd = parseMinutes(cfg.breakEnd);
  int sunsetStart = parseMinutes(cfg.sunsetStart);
  int nightStart = parseMinutes(cfg.nightStart);

  if (sunriseStart < 0 || dayStart < 0 || cloudyStart < 0 || breakStart < 0 || breakEnd < 0 || sunsetStart < 0 || nightStart < 0) {
    applyNight();
    return;
  }

  if (current >= nightStart || current < sunriseStart) {
    applyNight();
  } else if (current >= sunriseStart && current < dayStart) {
    // Amanecer: lunar -> rojizo -> espectro
    int midpoint = sunriseStart + (dayStart - sunriseStart) / 2;
    if (current < midpoint) {
      applyTransition(current, sunriseStart, midpoint, cfg.moonColor, cfg.moonIntensity, cfg.sunriseWarm, cfg.dayIntensity / 2, true);
    } else {
      applyTransition(current, midpoint, dayStart, cfg.sunriseWarm, cfg.dayIntensity / 2, cfg.daySpectrum, cfg.dayIntensity);
    }
  } else if (current >= dayStart && current < cloudyStart) {
    setAll(scaleColor(cfg.daySpectrum, cfg.dayIntensity));
  } else if (current >= cloudyStart && current < breakStart) {
    applyTransition(current, cloudyStart, breakStart, cfg.daySpectrum, cfg.dayIntensity, cfg.cloudySpectrum, cfg.cloudyIntensity);
  } else if (current >= breakStart && current < breakEnd) {
    strip.clear();
  } else if (current >= breakEnd && current < sunsetStart) {
    applyTransition(current, breakEnd, sunsetStart, cfg.cloudySpectrum, cfg.cloudyIntensity, cfg.daySpectrum, cfg.dayIntensity);
  } else if (current >= sunsetStart && current < nightStart) {
    applyTransition(current, sunsetStart, nightStart, cfg.daySpectrum, cfg.dayIntensity, cfg.sunsetWarm, cfg.dayIntensity / 3);
  }
}

String htmlPage() {
  String page = R"rawliteral(
<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family:sans-serif;max-width:840px;margin:auto;padding:1rem}fieldset{margin:1rem 0}label{display:block;margin:.5rem 0}</style></head>
<body><h2>Panel LED Acuario - ESP8266</h2>
<form method='POST' action='/save'>
<fieldset><legend>Horarios</legend>
<label>Amanecer inicio <input name='sunriseStart' value='%SUNRISE%'></label>
<label>Modo día inicio <input name='dayStart' value='%DAY%'></label>
<label>Nublado inicio <input name='cloudyStart' value='%CLOUDY%'></label>
<label>Descanso inicio <input name='breakStart' value='%BREAKSTART%'></label>
<label>Descanso fin <input name='breakEnd' value='%BREAKEND%'></label>
<label>Atardecer inicio <input name='sunsetStart' value='%SUNSET%'></label>
<label>Noche inicio <input name='nightStart' value='%NIGHT%'></label>
</fieldset>
<fieldset><legend>Intensidades (máx %MAXI% por fuente 5V/8A)</legend>
<label>Luna (0-%MAXI%) <input type='number' name='moonIntensity' max='%MAXI%' value='%MOONI%'></label>
<label>Día (0-%MAXI%) <input type='number' name='dayIntensity' max='%MAXI%' value='%DAYI%'></label>
<label>Nublado (0-%MAXI%) <input type='number' name='cloudyIntensity' max='%MAXI%' value='%CLOUDYI%'></label>
</fieldset>
<label>Timezone POSIX <input name='tz' value='%TZ%'></label>
<button type='submit'>Guardar</button>
</form>
<form method='POST' action='/manual'>
<fieldset><legend>Control manual</legend>
<label>Activar manual <input type='checkbox' name='enabled'></label>
<label>R <input type='number' name='r' min='0' max='255' value='255'></label>
<label>G <input type='number' name='g' min='0' max='255' value='255'></label>
<label>B <input type='number' name='b' min='0' max='255' value='255'></label>
<label>Intensidad (0-%MAXI%) <input type='number' name='intensity' min='0' max='%MAXI%' value='64'></label>
<button type='submit'>Aplicar</button>
</fieldset></form>
<p><a href='/status'>Estado JSON</a></p>
</body></html>)rawliteral";

  page.replace("%SUNRISE%", cfg.sunriseStart);
  page.replace("%DAY%", cfg.dayStart);
  page.replace("%CLOUDY%", cfg.cloudyStart);
  page.replace("%BREAKSTART%", cfg.breakStart);
  page.replace("%BREAKEND%", cfg.breakEnd);
  page.replace("%SUNSET%", cfg.sunsetStart);
  page.replace("%NIGHT%", cfg.nightStart);
  page.replace("%MOONI%", String(cfg.moonIntensity));
  page.replace("%DAYI%", String(cfg.dayIntensity));
  page.replace("%CLOUDYI%", String(cfg.cloudyIntensity));
  page.replace("%TZ%", timezonePosix);
  page.replace("%MAXI%", String(MAX_SAFE_INTENSITY));
  return page;
}

void handleSave() {
  cfg.sunriseStart = server.arg("sunriseStart");
  cfg.dayStart = server.arg("dayStart");
  cfg.cloudyStart = server.arg("cloudyStart");
  cfg.breakStart = server.arg("breakStart");
  cfg.breakEnd = server.arg("breakEnd");
  cfg.sunsetStart = server.arg("sunsetStart");
  cfg.nightStart = server.arg("nightStart");
  cfg.moonIntensity = min(static_cast<uint8_t>(server.arg("moonIntensity").toInt()), MAX_SAFE_INTENSITY);
  cfg.dayIntensity = min(static_cast<uint8_t>(server.arg("dayIntensity").toInt()), MAX_SAFE_INTENSITY);
  cfg.cloudyIntensity = min(static_cast<uint8_t>(server.arg("cloudyIntensity").toInt()), MAX_SAFE_INTENSITY);

  String tz = server.arg("tz");
  if (tz.length() > 0) {
    timezonePosix = tz;
    setenv("TZ", timezonePosix.c_str(), 1);
    tzset();
  }

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Guardado");
}

void handleManual() {
  manualOverride = server.hasArg("enabled");
  if (server.hasArg("r")) manualColor.r = static_cast<uint8_t>(server.arg("r").toInt());
  if (server.hasArg("g")) manualColor.g = static_cast<uint8_t>(server.arg("g").toInt());
  if (server.hasArg("b")) manualColor.b = static_cast<uint8_t>(server.arg("b").toInt());
  if (server.hasArg("intensity")) manualIntensity = min(static_cast<uint8_t>(server.arg("intensity").toInt()), MAX_SAFE_INTENSITY);

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Manual actualizado");
}

void handleStatus() {
  time_t now = time(nullptr);
  struct tm localTm;
  localtime_r(&now, &localTm);

  String json = "{";
  json += "\"manual\":" + String(manualOverride ? "true" : "false") + ",";
  json += "\"time\":\"" + String(localTm.tm_hour) + ":" + String(localTm.tm_min) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", htmlPage()); });
  server.on("/save", HTTP_POST, handleSave);
  server.on("/manual", HTTP_POST, handleManual);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
  }
}

void setupTime() {
  setenv("TZ", timezonePosix.c_str(), 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void setup() {
  strip.begin();
  strip.clear();
  strip.show();

  connectWiFi();
  setupTime();
  setupWeb();
}

void loop() {
  server.handleClient();

  if (manualOverride) {
    applyManual();
  } else {
    runScheduler();
  }

  strip.show();
  delay(200);
}
