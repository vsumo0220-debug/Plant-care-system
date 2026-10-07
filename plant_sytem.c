const int SENSOR_PIN = A0;
const int RELAY_PIN  = 3;

// Most cheap relay boards switch ON when the pin is LOW.
// If your pump runs when it should be off, change this to false.
const bool RELAY_ACTIVE_LOW = true;

// ---------- CALIBRATION ----------
// Watch the RAW: values in Serial Monitor (9600 baud):
//  - sensor in dry air     -> put that number in DRY_RAW
//  - sensor in a glass of water -> put that number in WET_RAW
const int DRY_RAW = 1000;
const int WET_RAW = 350;

// ---------- PUMP SETTINGS ----------
// "Water amount 10%" from the website = 10 * this many milliseconds
// (10% -> 3 seconds). Tune this to your pump.
const unsigned long PUMP_MS_PER_PERCENT = 300;
const unsigned long MAX_PUMP_MS   = 10000;   // safety cap
const unsigned long COOLDOWN_MS   = 30000;   // wait for water to soak in
const unsigned long REPORT_MS     = 1000;    // send sensor data every 1s

// ---------- STATE ----------
int  threshold    = 30;
int  waterPercent = 10;
bool autoMode     = true;

bool pumping = false;
bool hasWatered = false;
unsigned long pumpStart = 0;
unsigned long pumpDuration = 0;
unsigned long lastPumpEnd = 0;
unsigned long lastReport = 0;

int moisturePercent = 0;
String inputLine = "";

// ---------- RELAY ----------
void setRelay(bool on) {
  digitalWrite(RELAY_PIN, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void startPump(int percent) {
  if (pumping) return;
  percent = constrain(percent, 1, 100);
  pumpDuration = min((unsigned long)percent * PUMP_MS_PER_PERCENT, MAX_PUMP_MS);
  pumpStart = millis();
  pumping = true;
  setRelay(true);
  Serial.println("PUMP:ON");
}

void stopPump() {
  setRelay(false);
  pumping = false;
  hasWatered = true;
  lastPumpEnd = millis();
  Serial.println("PUMP:OFF");
}

// ---------- SENSOR ----------
int readRaw() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(SENSOR_PIN);
    delay(5);
  }
  return sum / 10;
}

int rawToPercent(int raw) {
  int pct = map(raw, DRY_RAW, WET_RAW, 0, 100);
  return constrain(pct, 0, 100);
}

// ---------- COMMANDS FROM WEBSITE ----------
void handleCommand(String cmd) {
  cmd.trim();

  if (cmd.startsWith("PUMP:")) {
    startPump(cmd.substring(5).toInt());
  }
  else if (cmd.startsWith("THRESHOLD:")) {
    threshold = constrain(cmd.substring(10).toInt(), 0, 100);
  }
  else if (cmd.startsWith("WATER:")) {
    waterPercent = constrain(cmd.substring(6).toInt(), 1, 100);
  }
  else if (cmd == "MODE:AUTO") {
    autoMode = true;
  }
  else if (cmd == "MODE:MANUAL") {
    autoMode = false;
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else if (c != '\r' && inputLine.length() < 32) {
      inputLine += c;
    }
  }
}

// ---------- MAIN ----------
void setup() {
  digitalWrite(RELAY_PIN, (RELAY_ACTIVE_LOW) ? HIGH : LOW); // start OFF
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);

  Serial.begin(9600);
}

void loop() {
  readSerialCommands();

  // Stop pump when time is up
  if (pumping && millis() - pumpStart >= pumpDuration) {
    stopPump();
  }

  // Report + automatic watering once per second
  if (millis() - lastReport >= REPORT_MS) {
    lastReport = millis();

    int raw = readRaw();
    moisturePercent = rawToPercent(raw);

    Serial.print("RAW:");
    Serial.println(raw);
    Serial.print("MOISTURE:");
    Serial.println(moisturePercent);

    bool cooledDown = !hasWatered || (millis() - lastPumpEnd >= COOLDOWN_MS);

    if (autoMode && !pumping && cooledDown && moisturePercent < threshold) {
      startPump(waterPercent);
    }
  }
}
