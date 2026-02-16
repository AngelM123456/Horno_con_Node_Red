#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>

// ========================= CONFIGURACIÓN =========================
#define DS_PIN 4
#define LAMPS 5
int RELAYS[LAMPS] = {19, 18, 5, 17, 16};
const int BTN_WAKE = 13, BTN_COUNT = 14;
const int BTN_UP = 27, BTN_DOWN = 26; 
const bool REL_LOW = true; 

// === OBJETOS GLOBALES ===
OneWire oneWire(DS_PIN);
DallasTemperature sensors(&oneWire);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ========================= VARIABLES DE CONTROL =========================
float setPoint = 107.0f;
float alertTemp = 120.0f; // Default Alerta
float temp = 0;
int targetLamps = 0;
int errorCounter = 0;
bool sensorError = false; 

// Estados del Sistema
bool countdownActive = false; 
bool locked = false;          // Bloqueo por fin de cuenta regresiva
bool forcedOff = false;       // Bloqueo manual (Comando "apagar")
bool alertTriggered = false;  // Bloqueo por sobrecalentamiento crítico

bool oledOn = true;
unsigned long countEnd = 0;
unsigned long oledTimer = 0;

// Variables para "Spam" (Reporte automático)
bool spamEnabled = true; // Spam activo por defecto (30 min)
unsigned long spamInterval = 30 * 60 * 1000UL; // 30 minutos en ms
unsigned long lastSpamTime = 0;

// Interrupciones
volatile bool flagWake = false, flagCount = false, flagUp = false, flagDown = false;
void IRAM_ATTR isrWake() { flagWake = true; }
void IRAM_ATTR isrCount() { flagCount = true; flagWake = true; }
void IRAM_ATTR isrUp() { flagUp = true; flagWake = true; }
void IRAM_ATTR isrDown() { flagDown = true; flagWake = true; }

// ========================= FUNCIONES AUXILIARES =========================

void shuffleRelays() {
  randomSeed(analogRead(0) + ESP.getCycleCount());
  for (int i = 0; i < LAMPS; i++) {
    int tempPin = RELAYS[i];
    int randomIndex = random(i, LAMPS);
    RELAYS[i] = RELAYS[randomIndex];
    RELAYS[randomIndex] = tempPin;
  }
}

// =================== REEMPLAZA ESTA FUNCIÓN COMPLETA ===================
void sendStatusJSON(String tipoMensaje) {
  // Calculamos tiempo restante en SEGUNDOS
  long remSeconds = 0;
  if (countdownActive && countEnd > millis()) {
    remSeconds = (countEnd - millis()) / 1000;
  }

  // Construcción del JSON
  String json = "{";
  json += "\"type\":\"" + tipoMensaje + "\",";
  json += "\"temp\":" + String(temp, 1) + ",";
  json += "\"setpoint\":" + String(setPoint, 1) + ",";
  json += "\"lamps\":" + String(targetLamps) + ",";
  json += "\"alert_threshold\":" + String(alertTemp, 1) + ",";
  json += "\"spam\":" + String(spamEnabled ? "true" : "false") + ","; // Nuevo campo
  
  String st = "STANDBY";
  if (alertTriggered) st = "ALERTA_CRITICA";
  else if (sensorError) st = "ERROR_SENSOR";
  else if (forcedOff) st = "APAGADO_MANUAL";
  else if (locked) st = "LOCKED_TIMER";
  else if (countdownActive) st = "RUNNING";
  
  json += "\"state\":\"" + st + "\",";
  json += "\"rem_seconds\":" + String(remSeconds); // Enviamos segundos exactos
  json += "}";
  
  Serial.println(json); 
}

// =================== REEMPLAZA ESTA FUNCIÓN COMPLETA ===================
void processSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase(); 

    if (input == "estatus") {
      sendStatusJSON("status_request");
    } 
    else if (input == "apagar") {
      forcedOff = true;
      targetLamps = 0;
      sendStatusJSON("cmd_ack");
    } 
    else if (input == "encender") {
      forcedOff = false;
      alertTriggered = false; 
      sendStatusJSON("cmd_ack");
    }
    else if (input == "spam on") {
      spamEnabled = true;
      lastSpamTime = millis(); 
      sendStatusJSON("update"); // CORREGIDO: Envía estatus completo, no "undefined"
    }
    else if (input == "spam off") {
      spamEnabled = false;
      sendStatusJSON("update"); // CORREGIDO
    }
    else if (input == "count down disabled") {
      countdownActive = false;
      locked = false; 
      sendStatusJSON("update");
    }
    // Comandos con parámetros
    else if (input.startsWith("setpoint ")) {
      float val = input.substring(9).toFloat();
      if (val > 0) setPoint = val;
      sendStatusJSON("update");
    }
    else if (input.startsWith("freq spam ")) {
      int val = input.substring(10).toInt();
      if (val > 0) spamInterval = val * 60 * 1000UL;
      sendStatusJSON("update");
    }
    else if (input.startsWith("count down ")) {
      int val = input.substring(11).toInt();
      if (val > 0) {
        countdownActive = true;
        countEnd = millis() + (val * 60000UL);
        locked = false;
      }
      sendStatusJSON("update");
    }
    else if (input.startsWith("set alert ")) {
      float val = input.substring(10).toFloat();
      if (val > 0) alertTemp = val;
      sendStatusJSON("update");
    }
  }
}
void updateHardware(int n) {
  static int current = 0;
  static unsigned long lastMove = 0;
  
  // Lógica de BLOQUEO MAESTRO
  if (locked || sensorError || forcedOff || alertTriggered) n = 0;

  if (n < current) {
    for (int i = 0; i < LAMPS; i++) {
      bool on = (i < n);
      digitalWrite(RELAYS[i], REL_LOW ? !on : on);
    }
    current = n;
  } else if (n > current && millis() - lastMove >= 1000) {
    digitalWrite(RELAYS[current], REL_LOW ? LOW : HIGH);
    current++;
    lastMove = millis();
  }
}

void draw() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14_tr);
  char b[25];

  if (alertTriggered) {
    u8g2.drawStr(0, 14, "!! ALERTA !!");
    snprintf(b, 25, "TEMP > %.0f", alertTemp); 
    u8g2.drawStr(0, 32, b);
  } else if (sensorError) {
    u8g2.drawStr(0, 14, "! ERROR SENSOR !");
  } else if (forcedOff) {
    u8g2.drawStr(0, 14, "SISTEMA APAGADO");
    u8g2.drawStr(0, 28, "POR COMANDO");
  } else {
    snprintf(b, 25, "T:%.1f Set:%.0f", temp, setPoint); 
    u8g2.drawStr(0, 14, b);
    snprintf(b, 25, "Lamps ON: %d", targetLamps); 
    u8g2.drawStr(0, 28, b);
    
    if (locked) u8g2.drawStr(0, 42, "State: LOCKED");
    else u8g2.drawStr(0, 42, countdownActive ? "Run" : "Stdby");

    if (countdownActive) {
      long rem = (countEnd - millis()) / 1000;
      snprintf(b, 25, "OFF: %02ld:%02ld", rem/60, rem%60);
      u8g2.drawStr(0, 56, b);
    }
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  shuffleRelays();
  
  int pins[] = {BTN_WAKE, BTN_COUNT, BTN_UP, BTN_DOWN};
  void (*isrs[])() = {isrWake, isrCount, isrUp, isrDown};
  for(int i=0; i<4; i++) { pinMode(pins[i], INPUT_PULLUP); attachInterrupt(pins[i], isrs[i], FALLING); }

  for (int i = 0; i < LAMPS; i++) { pinMode(RELAYS[i], OUTPUT); digitalWrite(RELAYS[i], REL_LOW ? HIGH : LOW); }
  
  sensors.begin();
  u8g2.begin();
  oledTimer = millis();
  lastSpamTime = millis();
}

void loop() {
  unsigned long now = millis();

  // 1. LEER COMANDOS DE TELEGRAM/PC
  processSerialCommands();

  // 2. Sensor y Lógica
  static unsigned long tM = 0;
  if (now - tM >= 1000) {
    sensors.requestTemperatures();
    float lectura = sensors.getTempCByIndex(0);
    
    if (lectura <= -100.0 || lectura >= 150.0) {
      errorCounter++;
      if (errorCounter >= 3) { sensorError = true; targetLamps = 0; }
    } else {
      errorCounter = 0;
      sensorError = false;
      temp = lectura;
    }

    // CHECK DE ALERTA DE TEMPERATURA
    if (!sensorError && temp >= alertTemp && !alertTriggered) {
      alertTriggered = true;
      targetLamps = 0;
      sendStatusJSON("alert_triggered"); // Avisar inmediatamente a Telegram
    }
    
    // SPAM AUTOMÁTICO
    if (spamEnabled && (now - lastSpamTime >= spamInterval)) {
      sendStatusJSON("spam_update");
      lastSpamTime = now;
    }

    tM = now;
  }

  // 3. Botones (Hardware)
  if (flagWake) { if(!oledOn){ u8g2.setPowerSave(0); oledOn=true; } oledTimer=now; flagWake=false; }
  if (flagCount && !forcedOff && !alertTriggered) {
    // Lógica original del botón Count
    if (locked) { locked = false; countdownActive = false; }
    else if (!sensorError) {
      if (!countdownActive) { countdownActive = true; countEnd = now + 15000UL; } // 15s default por botón
      else if ((countEnd - now) < (12 * 15000UL)) countEnd += 15000UL;
      else countdownActive = false;
    }
    flagCount = false;
  }
  if (flagUp) { setPoint += 1.0f; flagUp = false; }
  if (flagDown) { setPoint -= 1.0f; flagDown = false; }

  // 4. Histéresis con Shuffle
  static bool reachedTop = false;
  if (temp >= setPoint) {
    if (!reachedTop) { shuffleRelays(); reachedTop = true; }
    targetLamps = 0; 
  } else if (!locked && !sensorError && !forcedOff && !alertTriggered) {
    if (temp < (setPoint - 3)) reachedTop = false;
    if (temp < (setPoint - 7)) targetLamps = 5;      
    else if (temp < (setPoint - 6) && targetLamps < 4) targetLamps = 4;
    else if (temp < (setPoint - 5) && targetLamps < 3) targetLamps = 3;
    else if (temp < (setPoint - 4) && targetLamps < 2) targetLamps = 2;
    else if (temp < (setPoint - 3) && targetLamps < 1) targetLamps = 1;
  }

  // 5. Salidas
  if (countdownActive && now >= countEnd) { countdownActive = false; locked = true; targetLamps = 0; }
  updateHardware(targetLamps);
  
  if (oledOn) {
    draw();
    if (now - oledTimer >= 10000) { u8g2.setPowerSave(1); oledOn = false; }
  }
}
