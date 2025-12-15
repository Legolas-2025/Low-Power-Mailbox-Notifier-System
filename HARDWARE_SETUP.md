# 🔧 Hardware Setup Guide

This guide provides detailed instructions for assembling and configuring the Low-Power Mailbox Notifier hardware.

---

## 📋 **Firmware Options**

**Choose your firmware approach before starting hardware assembly:**

### 🔧 **Standalone Arduino Approach**
- **Files:** `Low-Power-Mailbox-Notifier_Transmitter.ino` + `Low-Power-Mailbox-Notifier_Receiver_Gateway.ino`
- **Programming:** Arduino IDE
- **Best for:** Users preferring direct control without smart home integration
- **LCD Display:** ✅ Fully supported in receiver code

### 🏠 **ESPHome Integration Approach**  
- **Files:** `mailbox_receiver.yaml` + Arduino transmitter
- **Programming:** ESPHome YAML configuration
- **Best for:** Smart home enthusiasts wanting automation
- **LCD Display:** ✅ Supported via ESPHome components

**Hardware assembly is identical for both approaches.** Only the firmware programming differs.

---

## 📦 **Component Assembly**

### ⚡ **Power System Setup**

#### 🔋 **18650 Li-ion Configuration**
```
Battery (+) ──[390kΩ]──┬── ESP8266 A0
                       │
                  [100kΩ] ── GND
                       │
Battery (-) ──────────── GND
```

**Components:**
- Battery: 18650 Li-ion (3.0V - 4.2V)
- DC-DC Converter: TPS63070
- Voltage Divider: 390kΩ + 100kΩ

**DC-DC Converter Wiring:**
- **V_IN**: Battery positive
- **V_OUT**: ESP8266 3.3V pin
- **GND**: Common ground
- **PS Pin**: Connect to GND (forces PFM mode)
- **EN Pin**: Connect to V_BAT (always enabled)

#### 🔋 **NiMH Configuration**
```
Battery (+) ──[220kΩ]──┬── ESP8266 A0
                       │
                  [100kΩ] ── GND
                       │
Battery (-) ──[Second AA] ── GND
```

**Components:**
- Battery: 2× AA NiMH (2.0V - 2.8V)
- DC-DC Converter: TPS63020 (required for low startup voltage)
- Voltage Divider: 220kΩ + 100kΩ

---

### 📡 **HC-12 Module Configuration**

**Critical: Set both modules to identical settings**

1. **Upload HC-12 Configuration Sketch:**
```cpp
// HC-12 Configuration Tool
#include <SoftwareSerial.h>

SoftwareSerial hc12(2, 3); // RX, TX

void setup() {
    Serial.begin(115200);
    hc12.begin(9600);
    Serial.println("HC-12 Configuration Tool");
    Serial.println("Available commands:");
    Serial.println("AT - Test connection");
    Serial.println("AT+BAUD4 - Set 9600 baud");
    Serial.println("AT+C001 - Set channel 001");
    Serial.println("AT+FU4 - Set full power");
    Serial.println("AT+DEFAULT - Reset to defaults");
}

void loop() {
    if (Serial.available()) {
        hc12.write(Serial.read());
    }
    if (hc12.available()) {
        Serial.write(hc12.read());
    }
}
```

2. **Execute Configuration Commands:**
```
AT          // Test connection - should respond "OK"
AT+BAUD4    // Set 9600 baud rate
AT+C001     // Set channel 001 (both modules must match)
AT+FU4      // Set full power (4)
AT          // Verify settings
```

---

### 📏 **HC-SR04P Ultrasonic Sensor**

**Mounting Guidelines:**
- **Position**: Inside mailbox, pointing toward mail slot
- **Distance**: 10-50cm from expected mail position
- **Mounting**: Use non-conductive foam tape to reduce vibrations
- **Weatherproofing**: Consider silicone sealant for outdoor exposure

**Pin Connections:**
```
HC-SR04P    ESP8266
VCC    →    3.3V
GND    →    GND
Trig   →    D1 (GPIO5)
Echo   →    D2 (GPIO4)
```

---

### 🚪 **Hall Sensor (KY-024) Modification**

**Critical Power Optimization:**
1. **Locate the power LED** on the KY-024 module
2. **Desolder or cut the LED** connection
3. **Test operation** to ensure sensor still works
4. **Verify low current draw** with multimeter

**Connection:**
```
KY-024      ESP8266
VCC    →    3.3V
GND    →    GND
Digital →   D3 (GPIO0)
```

---

### 📱 **LCD Display Setup**

**I2C Address Detection:**
```cpp
// I2C Scanner to find LCD address
#include <Wire.h>

void setup() {
    Wire.begin();
    Serial.begin(115200);
    Serial.println("I2C Scanner");
}

void loop() {
    byte error, address;
    int nDevices;
    
    Serial.println("Scanning...");
    nDevices = 0;
    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
    }
    delay(5000);
}
```

**Common Addresses:**
- 0x27 (most common)
- 0x3F (alternative)
- 0x20 (older modules)

---

### 💡 **LED Connections**

#### 🔆 **Status LED (Transmitter)**
```
LED (+) ──[330Ω]── ESP8266 D8 (GPIO15)
LED (-) ──────────── GND
```

#### 💫 **Breathing LED (Receiver)**
```
LED (+) ──[330Ω]── ESP8266 D7 (GPIO13)
LED (-) ──────────── GND
```

**LED Specifications:**
- Forward Voltage: 2.0V (Red), 3.2V (Blue/White)
- Forward Current: 20mA maximum
- Resistor: 330Ω (for 3.3V operation)

---

## 🔧 **Enclosure and Weatherproofing**

### 📦 **Transmitter Enclosure**

**Recommended Enclosure:**
- **Material**: ABS plastic or aluminum
- **Protection**: IP65 or better
- **Size**: 80×50×25mm minimum
- **Mounting**: DIN rail or wall mount

**Weatherproofing Steps:**
1. **Seal all cable entries** with silicone sealant
2. **Add desiccant packets** for humidity control
3. **Use rubber gaskets** for lid seal
4. **UV-resistant materials** for outdoor exposure

### 📦 **Receiver Enclosure**

**Indoor Installation:**
- **Placement**: Near mailbox, within HC-12 range
- **Power**: USB power supply inside enclosure
- **Display Access**: Clear window for LCD visibility
- **WiFi Range**: Ensure good WiFi signal

---

## ⚡ **Power Supply Verification**

### 🔋 **Battery Testing**

**Voltage Verification:**
```cpp
// Battery voltage test sketch
void testBattery() {
    int raw = analogRead(A0);
    float voltage = ((float)raw / 1024.0) * 1.0; // ESP8266 ADC
    voltage = voltage / VOLTAGE_DIVIDER_RATIO;   // Scale back up
    
    Serial.printf("Battery: %.2fV (Raw: %d)\n", voltage, raw);
    
    if (voltage < LOW_VOLTAGE_SHUTDOWN) {
        Serial.println("WARNING: Low battery!");
    }
}
```

**Power Consumption Testing:**
```cpp
// Current consumption measurement
void measurePowerConsumption() {
    Serial.println("Testing power consumption...");
    
    // Active mode measurement
    Serial.println("Active mode: ~80mA");
    delay(1000);
    
    // Sleep mode measurement
    Serial.println("Entering deep sleep...");
    ESP.deepSleep(1000000); // 1 second
}
```

---

## 📡 **Range Testing**

### 🧪 **Signal Quality Test**

**Outdoor Range Test:**
1. **Start at 10m** distance
2. **Increment by 50m** each test
3. **Test in different orientations**
4. **Document success/failure**

**Expected Range:**
- **Line of sight**: 300-500m
- **Urban environment**: 100-200m
- **Indoor**: 50-100m

**Optimization Tips:**
- **Antenna orientation**: Vertical for maximum range
- **Height**: Mount as high as possible
- **Obstacles**: Avoid metal objects and concrete
- **Interference**: Stay away from other 433MHz devices

---

## 🛠️ **Calibration Procedures**

### 📏 **Distance Calibration**

**Initial Setup:**
1. **Place transmitter** in mailbox
2. **Empty mailbox** state
3. **Power on system**
4. **Monitor distance readings** via Serial
5. **Record stable baseline**

**Calibration Values:**
```cpp
// In transmitter code
const float INITIAL_DISTANCE_CM = 1000; // Force first reading
const int DISTANCE_TOLERANCE_CM = 1;    // Sensitivity setting
```

### 🔋 **Battery Calibration**

**18650 Li-ion Reference:**
- **4.2V**: 100% (full charge)
- **3.7V**: 70% (nominal voltage)
- **3.2V**: 10% (low voltage cutoff)
- **3.0V**: 0% (critical cutoff)

**NiMH Reference:**
- **2.8V**: 100% (full charge)
- **2.4V**: 70% (nominal voltage)
- **2.1V**: 10% (low voltage cutoff)
- **2.0V**: 0% (critical cutoff)

---

## 🔍 **Final Testing Checklist**

### ✅ **Pre-Deployment Checklist**

**Hardware:**
- [ ] All solder joints inspected
- [ ] No short circuits detected
- [ ] Proper voltage divider ratios
- [ ] DC-DC converter in PFM mode
- [ ] Hall sensor LED removed
- [ ] HC-12 modules paired

**Software:**
- [ ] Transmitter configured for correct battery type
- [ ] Receiver WiFi credentials updated
- [ ] LCD I2C address verified
- [ ] MQTT topics configured (if enabled)
- [ ] Home Assistant integration tested

**Power:**
- [ ] Battery voltage verified
- [ ] Sleep current <20µA measured
- [ ] Active current <100mA verified
- [ ] Voltage divider accuracy confirmed

**Communication:**
- [ ] HC-12 range tested
- [ ] ACK protocol working
- [ ] Retry logic functioning
- [ ] LCD display updates confirmed

---

## 🆘 **Emergency Procedures**

### 🔋 **Battery Replacement**

**Safe Shutdown:**
1. **Remove old battery** carefully
2. **Check voltage** of replacement
3. **Verify polarity** connections
4. **Power on gradually** and monitor

### 🔧 **Module Replacement**

**ESP8266 Replacement:**
1. **Backup EEPROM data** if possible
2. **Note pin connections** before removal
3. **Install new module** carefully
4. **Restore configuration** via OTA

**HC-12 Replacement:**
1. **Note channel and baud settings**
2. **Replace module** 
3. **Restore settings** using configuration tool
4. **Test communication** range

---

This completes the hardware setup guide. For software configuration and Home Assistant integration, see the main README.md file.