// Head tracker: reads the MPU6050 over I2C, fuses accelerometer + gyro into
// roll/pitch/yaw angles (a complementary filter), and broadcasts them over
// ESP-NOW to ../tilt-platform. Also prints CSV over serial for
// scripts/visualize.py. Uses ESP-NOW broadcast, so no MAC address pairing
// is needed on either board.
//
// Accelerometer alone gives stable roll/pitch (it tracks gravity's direction,
// settling at a steady value once you stop moving) but is noisy under real
// motion. Gyro alone gives smooth, responsive rotation rate but drifts over
// time since it has no absolute reference. The filter blends both: gyro for
// short-term smoothness, accelerometer to correct long-term drift on roll/
// pitch. Yaw has no accelerometer correction available (no magnetometer on
// this sensor), so it will still drift slowly — that's an MPU6050 hardware
// limitation, not a bug.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <esp_now.h>

Adafruit_MPU6050 mpu;

constexpr float COMPLEMENTARY_ALPHA = 0.96f;  // weight given to the gyro vs. the accelerometer

// Must match the struct in tilt-platform/src/main.cpp exactly.
struct OrientationPacket {
  float roll;
  float pitch;
  float yaw;
};

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

float rollDeg = 0.0f;
float pitchDeg = 0.0f;
float yawDeg = 0.0f;
unsigned long lastMicros = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("Looking for MPU6050...");
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found - check wiring (SDA/SCL/VCC/GND)!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("MPU6050 found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  WiFi.mode(WIFI_STA);
  Serial.print("This board's MAC address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add ESP-NOW broadcast peer");
    }
  }

  lastMicros = micros();
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  unsigned long nowMicros = micros();
  float dt = (nowMicros - lastMicros) / 1000000.0f;
  lastMicros = nowMicros;

  // Roll/pitch derived from gravity's direction (stable, but noisy alone).
  float accelRollDeg = atan2(accel.acceleration.y, accel.acceleration.z) * 180.0f / PI;
  float accelPitchDeg = atan2(-accel.acceleration.x,
                               sqrt(accel.acceleration.y * accel.acceleration.y +
                                    accel.acceleration.z * accel.acceleration.z)) *
                         180.0f / PI;

  // Gyro rates, converted from rad/s to deg/s.
  float gyroRollRateDeg = gyro.gyro.x * 180.0f / PI;
  float gyroPitchRateDeg = gyro.gyro.y * 180.0f / PI;
  float gyroYawRateDeg = gyro.gyro.z * 180.0f / PI;

  rollDeg = COMPLEMENTARY_ALPHA * (rollDeg + gyroRollRateDeg * dt) +
            (1.0f - COMPLEMENTARY_ALPHA) * accelRollDeg;
  pitchDeg = COMPLEMENTARY_ALPHA * (pitchDeg + gyroPitchRateDeg * dt) +
             (1.0f - COMPLEMENTARY_ALPHA) * accelPitchDeg;
  yawDeg += gyroYawRateDeg * dt;  // no accelerometer correction available for yaw

  OrientationPacket packet = {rollDeg, pitchDeg, yawDeg};
  esp_now_send(broadcastAddress, (uint8_t *)&packet, sizeof(packet));

  Serial.printf("%.2f,%.2f,%.2f\n", rollDeg, pitchDeg, yawDeg);

  delay(20);
}
