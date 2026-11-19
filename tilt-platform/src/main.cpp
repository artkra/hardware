// Tilt platform: receives head orientation from ../head-tracker over
// ESP-NOW and drives the pan/tilt servos to match — yaw controls pan,
// pitch controls tilt. Uses ESP-NOW broadcast, so no MAC address pairing
// is needed; any board broadcasting the matching packet struct on the
// same WiFi channel is picked up.
#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_now.h>

// Servo pin assignment — must match the wiring in README.md
constexpr int PAN_PIN = 12;
constexpr int TILT_PIN = 13;

// Conservative default clear of most mounts' hard stops — narrow further
// once you've confirmed the exact range your bracket can clear without straining.
constexpr int ANGLE_MIN = 30;
constexpr int ANGLE_MAX = 150;
constexpr int CENTER_ANGLE = 90;

constexpr unsigned long UPDATE_INTERVAL_MS = 20;   // matches the servo's own ~50Hz update rate
constexpr int MAX_STEP_DEGREES = 2;                // slew limit per tick — eases toward the target
constexpr unsigned long SIGNAL_TIMEOUT_MS = 500;   // recenter if the head-tracker goes silent

// Must match the struct in head-tracker/src/main.cpp exactly.
struct OrientationPacket {
  float roll;
  float pitch;
  float yaw;
};

Servo panServo;
Servo tiltServo;

int panAngle = CENTER_ANGLE;
int tiltAngle = CENTER_ANGLE;

volatile float latestPitch = 0.0f;
volatile float latestYaw = 0.0f;
volatile unsigned long lastPacketMs = 0;

int stepToward(int current, int target, int maxStep) {
  if (target > current) {
    return min(current + maxStep, target);
  }
  if (target < current) {
    return max(current - maxStep, target);
  }
  return current;
}

void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int len) {
  if (len != sizeof(OrientationPacket)) {
    return;
  }
  OrientationPacket packet;
  memcpy(&packet, data, sizeof(packet));
  latestPitch = packet.pitch;
  latestYaw = packet.yaw;
  lastPacketMs = millis();
}

void setup() {
  Serial.begin(115200);

  // Allow allocation of all timers so both servos can run independently
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  panServo.setPeriodHertz(50);
  tiltServo.setPeriodHertz(50);
  panServo.attach(PAN_PIN, 500, 2400);
  tiltServo.attach(TILT_PIN, 500, 2400);

  WiFi.mode(WIFI_STA);
  Serial.print("This board's MAC address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("Waiting for head-tracker packets...");
}

void loop() {
  static unsigned long lastUpdateMs = 0;
  unsigned long now = millis();
  if (now - lastUpdateMs < UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdateMs = now;

  int panTarget = CENTER_ANGLE;
  int tiltTarget = CENTER_ANGLE;

  bool signalFresh = lastPacketMs != 0 && (now - lastPacketMs) < SIGNAL_TIMEOUT_MS;
  if (signalFresh) {
    panTarget = constrain(CENTER_ANGLE + (int)latestYaw, ANGLE_MIN, ANGLE_MAX);
    tiltTarget = constrain(CENTER_ANGLE + (int)latestPitch, ANGLE_MIN, ANGLE_MAX);
  }

  panAngle = stepToward(panAngle, panTarget, MAX_STEP_DEGREES);
  tiltAngle = stepToward(tiltAngle, tiltTarget, MAX_STEP_DEGREES);

  panServo.write(panAngle);
  tiltServo.write(tiltAngle);

  Serial.printf("pan: %d  tilt: %d  (yaw: %.1f pitch: %.1f, signal: %s)\n", panAngle, tiltAngle,
                latestYaw, latestPitch, signalFresh ? "ok" : "lost");
}
