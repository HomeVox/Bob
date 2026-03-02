// ==========================================
// Bob Advanced Sensors Library
// LTR-553, BMM150, AXP2101 proper implementation
// ==========================================

#ifndef BOB_SENSORS_ADVANCED_H
#define BOB_SENSORS_ADVANCED_H

#include <Wire.h>

// ==========================================
// LTR-553 Proximity & Light Sensor
// ==========================================

#define LTR553_ADDR 0x23
#define LTR553_CONTR 0x80
#define LTR553_MEAS_RATE 0x85
#define LTR553_PS_LED 0x82
#define LTR553_PS_N_PULSES 0x83
#define LTR553_PS_MEAS_RATE 0x84
#define LTR553_ALS_CONTR 0x80
#define LTR553_PS_DATA_0 0x8D
#define LTR553_PS_DATA_1 0x8E
#define LTR553_ALS_DATA_0 0x88
#define LTR553_ALS_DATA_1 0x89

class LTR553Sensor {
private:
  uint8_t addr = LTR553_ADDR;
  bool initialized = false;
  
  void writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }
  
  uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }
  
public:
  bool begin() {
    // Check if device responds
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) {
      Serial.println("LTR553: Not found");
      return false;
    }
    
    // Reset device
    writeReg(0x80, 0x02);
    delay(10);
    
    // Configure PS mode
    writeReg(0x81, 0x03);  // PS active mode, 16-bit
    writeReg(0x82, 0x7F);  // LED pulse freq 60kHz, 100% duty
    writeReg(0x83, 0x08);  // 8 pulses
    writeReg(0x84, 0x02);  // PS meas rate 100ms
    
    // Configure ALS
    writeReg(0x80, 0x01);  // ALS active
    writeReg(0x85, 0x03);  // ALS+PS 100ms
    
    delay(50);
    
    initialized = true;
    Serial.println("LTR553: Initialized ");
    return true;
  }
  
  uint16_t readProximity() {
    if (!initialized) return 0;
    
    uint8_t lsb = readReg(LTR553_PS_DATA_0);
    uint8_t msb = readReg(LTR553_PS_DATA_1);
    return (msb << 8) | lsb;
  }
  
  uint16_t readAmbientLight() {
    if (!initialized) return 0;
    
    uint8_t lsb = readReg(LTR553_ALS_DATA_0);
    uint8_t msb = readReg(LTR553_ALS_DATA_1);
    return (msb << 8) | lsb;
  }
  
  bool isAvailable() { return initialized; }
};

// ==========================================
// BMM150 Magnetometer
// ==========================================

#define BMM150_ADDR 0x13
#define BMM150_CHIP_ID 0x40
#define BMM150_CHIP_ID_REG 0x40
#define BMM150_PWR_CTRL 0x4B
#define BMM150_OP_MODE 0x4C
#define BMM150_DATA_X_LSB 0x42

class BMM150Sensor {
private:
  uint8_t addr = BMM150_ADDR;
  bool initialized = false;
  
  void writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }
  
  uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }
  
public:
  bool begin() {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) {
      Serial.println("BMM150: Not found");
      return false;
    }
    
    // Check chip ID
    uint8_t chipId = readReg(BMM150_CHIP_ID_REG);
    if (chipId != BMM150_CHIP_ID) {
      Serial.printf("BMM150: Wrong chip ID 0x%02X\n", chipId);
      return false;
    }
    
    // Power up
    writeReg(BMM150_PWR_CTRL, 0x01);
    delay(10);
    
    // Normal mode, ODR 10Hz
    writeReg(BMM150_OP_MODE, 0x00);
    delay(10);
    
    initialized = true;
    Serial.println("BMM150: Initialized ");
    return true;
  }
  
  void readMagnetometer(float* x, float* y, float* z) {
    if (!initialized) {
      *x = *y = *z = 0;
      return;
    }
    
    Wire.beginTransmission(addr);
    Wire.write(BMM150_DATA_X_LSB);
    Wire.endTransmission(false);
    Wire.requestFrom(addr, (uint8_t)8);
    
    if (Wire.available() >= 8) {
      int16_t rawX = Wire.read() | (Wire.read() << 8);
      int16_t rawY = Wire.read() | (Wire.read() << 8);
      int16_t rawZ = Wire.read() | (Wire.read() << 8);
      Wire.read(); Wire.read(); // Hall resistance
      
      // Convert to ÂµT (microtesla)
      *x = rawX * 0.3f;
      *y = rawY * 0.3f;
      *z = rawZ * 0.3f;
    }
  }
  
  float readHeading(float* x, float* y, float* z) {
    readMagnetometer(x, y, z);
    
    // Calculate heading (0-360Â°)
    float heading = atan2(*y, *x) * 180.0f / M_PI;
    if (heading < 0) heading += 360.0f;
    
    return heading;
  }
  
  bool isAvailable() { return initialized; }
};

// ==========================================
// AXP2101 Power Management
// ==========================================

#define AXP2101_ADDR 0x34
#define AXP2101_TEMP_H 0x58
#define AXP2101_TEMP_L 0x59

class AXP2101Sensor {
private:
  uint8_t addr = AXP2101_ADDR;
  bool initialized = false;
  
  uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }
  
public:
  bool begin() {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) {
      Serial.println("AXP2101: Not found");
      return false;
    }
    
    initialized = true;
    Serial.println("AXP2101: Initialized ");
    return true;
  }
  
  float readTemperature() {
    if (!initialized) return 25.0f;
    
    uint8_t tempH = readReg(AXP2101_TEMP_H);
    uint8_t tempL = readReg(AXP2101_TEMP_L);
    
    int16_t rawTemp = (tempH << 8) | tempL;
    
    // Convert to Celsius
    // Formula from AXP2101 datasheet
    float temp = -267.0f + (rawTemp * 0.1f);
    
    return temp;
  }
  
  bool isAvailable() { return initialized; }
};

#endif // BOB_SENSORS_ADVANCED_H
