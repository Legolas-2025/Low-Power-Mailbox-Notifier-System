# 🔧 Troubleshooting Guide

This comprehensive guide helps diagnose and resolve common issues with the Low-Power Mailbox Notifier system.

---

## 🔍 **Identify Your Setup**

**Before troubleshooting, identify your firmware approach:**

### 📋 **Firmware Type Check**

| Firmware Type | File Names | Programming Method | Interface |
|---------------|------------|-------------------|-----------|
| **Standalone Arduino** | `.ino` files | Arduino IDE | Serial Monitor |
| **ESPHome Integration** | `.yaml` files | ESPHome/HA | Home Assistant Dashboard |

**When seeking help, mention your firmware type for more accurate assistance.**

---

## 🚨 **Quick Diagnostics**

### ⚡ **Power System Issues**

#### 🔋 **Battery Drains Too Quickly**

**Symptoms:**
- Transmitter battery depletes in days instead of months
- High sleep current measurement (>50µA)
- System constantly active

**Diagnosis Steps:**
```cpp
// Test sleep current
void measureSleepCurrent() {
    Serial.println("Measuring sleep current...");
    delay(1000);
    Serial.println("Entering deep sleep in 5 seconds...");
    delay(5000);
    
    // Measure current here with multimeter
    ESP.deepSleep(1000000); // 1 second sleep
}
```

**Solutions:**
1. **Check DC-DC Converter Mode:**
   - Verify PS pin connected to GND (PFM mode)
   - Confirm EN pin connected to V_BAT
   - Test with different converter if needed

2. **Remove Hall Sensor LED:**
   - Desolder power LED on KY-024 module
   - Verify sensor still functions
   - Measure current reduction

3. **Verify Deep Sleep:**
   - Check ESP8266 reset pin connection (D0 to RST)
   - Confirm wake timer configuration
   - Monitor serial output for sleep/wake cycles

#### ⚡ **Voltage Reading Errors**

**Symptoms:**
- Incorrect battery percentage
- "Charge battery" message with good battery
- Voltage readings inconsistent

**Diagnosis:**
```cpp
// Voltage divider verification
void verifyVoltageDivider() {
    Serial.println("Testing voltage divider...");
    
    // Test known voltage sources
    float test_voltages[] = {3.3, 3.7, 4.0, 4.2};
    
    for (float test_v : test_voltages) {
        int raw = analogRead(A0);
        float calculated = ((float)raw / 1024.0) / VOLTAGE_DIVIDER_RATIO;
        
        Serial.printf("Test: %.2fV, Raw: %d, Calculated: %.2fV\n", 
                     test_v, raw, calculated);
        delay(1000);
    }
}
```

**Solutions:**
1. **Verify Resistor Values:**
   - Measure with multimeter
   - Check for tolerance issues (>5% can cause errors)
   - Replace if values are incorrect

2. **Check Wiring:**
   - Verify voltage divider connected to battery (before DC-DC)
   - Check for loose connections
   - Ensure proper ground reference

3. **Calibrate ADC:**
   - Use known reference voltages
   - Adjust VOLTAGE_DIVIDER_RATIO in code if needed
   - Consider ESP8266 ADC accuracy limitations

---

### 📡 **Communication Issues**

#### 📶 **HC-12 Range Problems**

**Symptoms:**
- Intermittent communication
- ACK timeouts
- No signal at expected distance

**Diagnosis:**
```cpp
// Range test function
void testHC12Range() {
    Serial.println("Testing HC-12 range...");
    
    for (int distance = 10; distance <= 500; distance += 50) {
        Serial.printf("Testing at %d meters...\n", distance);
        
        // Send test packet
        MailboxData testData;
        strcpy(testData.message, "RANGE_TEST");
        testData.battery_percent = 100;
        
        hc12.write((byte*)&testData, sizeof(testData));
        hc12.flush();
        
        // Wait for ACK
        unsigned long start = millis();
        bool ack_received = false;
        
        while (millis() - start < 1000) {
            if (hc12.available()) {
                char ack = hc12.read();
                if (ack == 'K') {
                    ack_received = true;
                    break;
                }
            }
        }
        
        Serial.printf("Result: %s\n", ack_received ? "SUCCESS" : "FAILED");
        delay(2000); // Wait before next test
    }
}
```

**Solutions:**
1. **Antenna Optimization:**
   - Position antennas vertically
   - Avoid metal objects nearby
   - Maximize height difference between units
   - Consider external antennas for extended range

2. **Channel Selection:**
   - Use channel 001 (default) or 100 (less crowded)
   - Test different channels for interference
   - Verify both modules on same channel

3. **Power Settings:**
   - Set to maximum power (AT+FU4)
   - Check for power supply issues
   - Verify HC-12 module authenticity (fake modules have poor performance)

#### 📨 **No ACK Received**

**Symptoms:**
- Transmitter retries constantly
- "Transmission failed" in logs
- No receiver response

**Diagnosis:**
```cpp
// ACK diagnostic function
void diagnoseACK() {
    Serial.println("Diagnosing ACK issue...");
    
    // Test HC-12 communication
    hc12.println("AT");
    delay(1000);
    
    if (hc12.available()) {
        String response = hc12.readString();
        Serial.print("HC-12 Response: ");
        Serial.println(response);
    } else {
        Serial.println("No response from HC-12");
    }
    
    // Test receiver ping
    hc12.println("PING");
    delay(1000);
    
    unsigned long start = millis();
    while (millis() - start < 5000) {
        if (hc12.available()) {
            Serial.print("Receiver response: ");
            Serial.println(hc12.readString());
            break;
        }
    }
}
```

**Solutions:**
1. **HC-12 Configuration:**
   - Verify baud rate match (9600 default)
   - Check channel settings
   - Reset to defaults: AT+DEFAULT

2. **Physical Connections:**
   - Verify RX/TX connections (crossed correctly)
   - Check for loose wiring
   - Ensure proper ground connection

3. **Receiver Issues:**
   - Check receiver power supply
   - Verify LCD and LED connections
   - Monitor receiver serial output

---

### 📱 **Display and Interface Issues**

#### 🖥️ **LCD Display Problems**

**Symptoms:**
- No display or blank screen
- Garbled characters
- Display not updating

**Diagnosis:**
```cpp
// LCD diagnostic
void testLCD() {
    Serial.println("Testing LCD...");
    
    lcd.init();
    lcd.backlight();
    lcd.clear();
    
    // Test each position
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 16; col++) {
            lcd.setCursor(col, row);
            lcd.print("X");
            delay(100);
        }
    }
    
    // Test specific characters
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LCD Test");
    lcd.setCursor(0, 1);
    lcd.print("1234567890123456");
}
```

**Solutions:**
1. **I2C Address Issues:**
   - Run I2C scanner to find correct address
   - Common addresses: 0x27, 0x3F, 0x20
   - Check wiring (SDA/SCL connections)

2. **Power Issues:**
   - Verify 5V supply for LCD
   - Check backlight connection
   - Ensure proper grounding

3. **Library Compatibility:**
   - Use LiquidCrystal_I2C library
   - Check for version conflicts
   - Verify display type (16x2 standard)

#### 💡 **LED Issues**

**Symptoms:**
- LED not working
- Wrong brightness
- No breathing effect

**Solutions:**
1. **Hardware Check:**
   - Verify LED polarity
   - Check resistor value (330Ω for 3.3V)
   - Test with known working LED

2. **PWM Configuration:**
   - Ensure GPIO pin supports PWM
   - Check frequency settings (1000Hz default)
   - Verify analogWrite function usage

3. **Power Issues:**
   - Check LED current requirements
   - Verify power supply capacity
   - Consider MOSFET driver for high-power LEDs

---

### 🌐 **Network and Integration Issues**

#### 📶 **WiFi Connection Problems**

**Symptoms:**
- Can't connect to WiFi
- Frequent disconnections
- NTP sync failures

**Diagnosis:**
```cpp
// WiFi diagnostic
void diagnoseWiFi() {
    Serial.println("WiFi Diagnostic...");
    
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    
    // Test NTP
    Serial.println("Testing NTP sync...");
    configTime(tzInfo, ntpServer);
    delay(2000);
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println("NTP sync successful");
    } else {
        Serial.println("NTP sync failed");
    }
}
```

**Solutions:**
1. **Connection Issues:**
   - Verify SSID and password
   - Check for special characters in credentials
   - Test with different network

2. **Signal Quality:**
   - Move closer to access point
   - Check for interference sources
   - Consider WiFi extender for weak signals

3. **Power Management:**
   - Disable WiFi power saving
   - Increase WiFi timeout settings
   - Monitor power consumption impact

#### 🏠 **Home Assistant Integration**

**Symptoms:**
- Device not appearing in HA
- Sensors not updating
- MQTT connection failures

**Solutions:**
1. **ESPHome Issues:**
   - Verify ESPHome version compatibility
   - Check API key configuration
   - Monitor ESPHome logs for errors

2. **MQTT Problems:**
   - Verify broker is running
   - Check username/password
   - Test with MQTT client tool

3. **Network Issues:**
   - Ensure devices on same network
   - Check firewall settings
   - Verify mDNS resolution

---

## 🔧 **Advanced Diagnostics**

### 📊 **Performance Monitoring**

**Create monitoring script:**
```cpp
void performanceMonitor() {
    static unsigned long lastCheck = 0;
    static int messageCount = 0;
    static float avgBattery = 0;
    
    if (millis() - lastCheck >= 60000) { // Every minute
        lastCheck = millis();
        
        // Count messages
        Serial.printf("Messages in last minute: %d\n", messageCount);
        
        // Calculate average battery
        avgBattery = (avgBattery + rxData.battery_percent) / 2;
        Serial.printf("Average battery: %.1f%%\n", avgBattery);
        
        // Reset counters
        messageCount = 0;
    }
    
    messageCount++;
}
```

### 🔍 **Signal Quality Analysis**

**RSSI Monitoring:**
```cpp
void monitorSignalQuality() {
    static unsigned long lastRSSI = 0;
    
    if (millis() - lastRSSI >= 10000) { // Every 10 seconds
        lastRSSI = millis();
        
        // Read signal strength (if available)
        // This would require custom HC-12 monitoring
        
        Serial.printf("Signal quality check at %lu\n", millis());
    }
}
```

---

## 📋 **Systematic Troubleshooting Process**

### 1️⃣ **Initial Assessment**
1. **Power Status:** Check all power connections
2. **Basic Communication:** Verify HC-12 link
3. **Sensor Operation:** Test individual components
4. **Network Status:** Confirm WiFi/MQTT connectivity

### 2️⃣ **Component Isolation**
1. **Test each component individually**
2. **Swap known-good components**
3. **Use serial monitoring for diagnostics**
4. **Check voltage levels with multimeter**

### 3️⃣ **Environmental Factors**
1. **Temperature effects** on components
2. **Electromagnetic interference**
3. **Power supply noise**
4. **Physical damage or wear**

### 4️⃣ **Software Verification**
1. **Code compilation** without errors
2. **Configuration** parameters correct
3. **Library versions** compatible
4. **EEPROM data** integrity

---

## 📞 **Getting Help**

### 🔍 **Before Asking for Help**

**Collect this information:**
1. **Hardware configuration** (battery type, components)
2. **Software version** (ESP8266 core, libraries)
3. **Error messages** from serial monitor
4. **Network details** (WiFi, MQTT setup)
5. **Steps to reproduce** the issue

### 📋 **Diagnostic Data Collection**

**Generate diagnostic report:**
```cpp
void generateDiagnosticReport() {
    Serial.println("=== DIAGNOSTIC REPORT ===");
    Serial.println();
    
    // System info
    Serial.println("System Information:");
    Serial.printf("ESP8266 Chip: %s\n", ESP.getChipId());
    Serial.printf("Flash Size: %d bytes\n", ESP.getFlashChipSize());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Uptime: %lu ms\n", millis());
    Serial.println();
    
    // Power system
    Serial.println("Power System:");
    Serial.printf("Battery Voltage: %.2fV\n", readBatteryVoltage());
    Serial.printf("Battery Percentage: %d%%\n", readBatteryPercentage(readBatteryVoltage()));
    Serial.println();
    
    // Network
    Serial.println("Network Status:");
    Serial.printf("WiFi SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("WiFi Signal: %d dBm\n", WiFi.RSSI());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.println();
    
    // Hardware status
    Serial.println("Hardware Status:");
    Serial.printf("HC-12 Available: %s\n", hc12.available() ? "Yes" : "No");
    Serial.printf("LCD Address: 0x27 (assumed)\n");
    Serial.printf("Reset Button: %s\n", digitalRead(RESET_BUTTON_PIN) ? "Released" : "Pressed");
    Serial.println();
    
    Serial.println("=== END REPORT ===");
}
```

### 💬 **Support Channels**

1. **GitHub Issues** - Technical problems and bug reports
2. **ESPHome Community** - Configuration and integration help
3. **Home Assistant Forum** - Automation and UI questions
4. **Reddit r/homeautomation** - General discussion and ideas

---

## 📚 **Prevention and Maintenance**

### 🔧 **Regular Maintenance Tasks**

**Monthly:**
- Check battery voltage levels
- Verify signal strength
- Test manual reset function
- Clean contacts and connections

**Quarterly:**
- Replace battery if below 50%
- Check antenna connections
- Update firmware if available
- Verify calibration accuracy

**Annually:**
- Complete system test
- Replace worn components
- Update documentation
- Review power consumption

### 📊 **Performance Optimization**

**Battery Life:**
- Monitor average power consumption
- Optimize sleep intervals if needed
- Consider solar charging for extended life

**Range Optimization:**
- Periodically test maximum range
- Adjust antenna positioning
- Monitor interference sources

**Reliability:**
- Implement redundant sensors
- Add signal quality monitoring
- Plan for component failures

This completes the comprehensive troubleshooting guide. Use it systematically to identify and resolve issues with your mailbox notifier system.