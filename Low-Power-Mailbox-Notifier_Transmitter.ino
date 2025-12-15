/*
 * PROJECT: Mailbox Notifier - Transmitter (Low Power End Node)
 * DEVICE: WeMos D1 Mini ESP8266 ESP-12F
 * RF MODULE: HC-12 433MHz Serial Wireless Module
 *
 * FUNCTIONALITY:
 * 1. Deep Sleep: The device spends most of its time in deep sleep to conserve battery.
 * 2. Low-Voltage Cutoff: Sends "Charge battery" message and enters permanent sleep if voltage drops below safe threshold.
 * 3. Wakeup Source (Primary - Periodic Check): Wakes up every 1 second via timer to read the HC-SR04P sensor.
 * 4. Trigger Logic (Mail Received): Compares current distance to a stored stable distance.
 * 5. Trigger Logic (Mail Picked Up - OPTIONAL): If the Hall sensor is ENABLED, detects door opening and resets baseline.
 * 6. Battery Monitoring: Reads battery voltage and includes this percentage in the transmitted packet.
 * 7. Transmission: Sends a structured packet (message, battery %) via HC-12, **requiring ACK from receiver.**
 *
 * HARDWARE AND CONFIGURATION NOTES:
 * --------------------------------------------------------------------------------------
 * *** 1. BATTERY TYPE CONFIGURATION ***
 * - Uncomment ONE of the following lines to set the correct voltage thresholds 
 * and provide the correct voltage divider components:
 * --------------------------------------------------------------------------------------
#define BATTERY_TYPE_18650 // 1x Li-ion 18650 (3.0V - 4.2V)
// #define BATTERY_TYPE_NIMH    // 2x AA NiMH in series (2.0V - 2.8V)
// --------------------------------------------------------------------------------------

 * *** 2. LOW-POWER COMPONENTS (MANDATORY) ***
 * - DC-DC CONVERTER: 
 * - **IF 18650 (Li-ion) is selected:** Use **TPS63070** (Ultra-low quiescent current of 7 µA, startup voltage 2.8 V).
 * - **IF NiMH is selected:** Use **TPS63020** (Required for low startup voltage 1.8 V).
 * - **WIRING:** Connect the regulated 3.3V output of the converter directly to the **3.3V pin** (NOT the 5V/USB pin) of the Wemos D1 Mini.
 * - **Mode Configuration:** Connect the **PS pin to GND** (0V) to force PFM (Power-Saving Mode) for highest efficiency at light loads (deep sleep). The **EN pin must be connected to V_BAT** to keep the chip always enabled.
 * - HALL SENSOR: KY-024 module, but **physically disable the always-on Power Indicator LED** to prevent battery drain.

 * *** 3. VOLTAGE DIVIDER REQUIRED for BATTERY_PIN (A0) ***
 * The ADC input limit on the ESP8266 is 1.0V. The voltage divider ensures the battery voltage is safely scaled down.
 * * IF using BATTERY_TYPE_18650 (1x Li-ion, V_max=4.2V):
 * - Use R1 = 390kΩ (from V_bat to A0)
 * - Use R2 = 100kΩ (from A0 to GND)
 * * IF using BATTERY_TYPE_NIMH (2x NiMH, V_max=2.8V):
 * - Use R1 = 220kΩ (from V_bat to A0)
 * - Use R2 = 100kΩ (from A0 to GND)
 *
 * --- PINOUT INSTRUCTIONS & HARDWARE CONNECTIONS ---
 * - BATTERY_PIN: A0 (Analog Input) - Connected to V_bat BEFORE the TPS63070/converter.
 * - HC-SR04P Trig Pin: D1 (GPIO 5).
 * - HC-SR04P Echo Pin: D2 (GPIO 4).
 * - HC-12 TX Pin: D6 (GPIO 12).
 * - HC-12 RX Pin: D5 (GPIO 14).
 * - STATUS_LED_PIN: D8 (GPIO 15) - Status LED (active HIGH).
 * - TEST_BUTTON_PIN: D7 (GPIO 13) - Test Button (active LOW, connected to GND).
 */

#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// --------------------------------------------------------------------------------------
// *** BATTERY TYPE SELECTION ***
// Select ONE of the options below. This sets the safe voltage thresholds.
// --------------------------------------------------------------------------------------
#define BATTERY_TYPE_18650 
// #define BATTERY_TYPE_NIMH
// --------------------------------------------------------------------------------------

// Set the critical voltage based on the selection
#ifdef BATTERY_TYPE_18650
    // 18650 Li-ion safe minimum (3.0V) + buffer.
    const float LOW_VOLTAGE_SHUTDOWN = 3.2; 
    const float V_MAX_BATT = 4.2;   // Full charge voltage
    // Divider: R1=390k, R2=100k. Ratio R2/(R1+R2) = 100/490 = 0.204
    const float VOLTAGE_DIVIDER_RATIO = 0.204; 
#elif defined(BATTERY_TYPE_NIMH)
    // 2x NiMH safe minimum (2.0V) + buffer.
    const float LOW_VOLTAGE_SHUTDOWN = 2.1; 
    const float V_MAX_BATT = 2.8;   // Full charge voltage
    // Divider: R1=220k, R2=100k. Ratio R2/(R1+R2) = 100/320 = 0.3125
    const float VOLTAGE_DIVIDER_RATIO = 0.3125;
#else
    #error "A battery type (BATTERY_TYPE_18650 or BATTERY_TYPE_NIMH) must be defined."
#endif


// --------------------------------------------------------------------------------------
// *** HALL SENSOR OPTION ***
// Uncomment this line to enable the Hall Sensor logic (Mail Picked Up detection).
#define ENABLE_HALL_SENSOR
// --------------------------------------------------------------------------------------


// --- PIN DEFINITIONS ---
#define HC12_TX_PIN D6      // GPIO12 - HC-12 RX Pin
#define HC12_RX_PIN D5      // GPIO14 - HC-12 TX Pin
#define TRIG_PIN D1         // GPIO5 - HC-SR04 Trigger Pin
#define ECHO_PIN D2         // GPIO4 - HC-SR04 Echo Pin
#define STATUS_LED_PIN D8   // GPIO15 - Status LED (active HIGH)
#define TEST_BUTTON_PIN D7  // GPIO13 - Test Button (active LOW, connected to GND)
#define BATTERY_PIN A0      // Analog Input for Battery Voltage (Uses Voltage Divider)

#ifdef ENABLE_HALL_SENSOR
#define HALL_SENSOR_PIN D3  // GPIO0 - Hall Sensor output pin
#endif

// --- CONFIGURATION ---
const float V_MIN_BATT = LOW_VOLTAGE_SHUTDOWN; // 0% battery voltage is the shutdown threshold
const int DISTANCE_TOLERANCE_CM = 1; // Tolerance for mailbox change
const long INITIAL_DISTANCE_CM = 1000; // Large value to force initial reading/setting
const float ADC_MAX_V = 1.0; // ESP8266 max ADC input voltage

// --- ACKNOWLEDGEMENT & RETRY CONFIG ---
const int EEPROM_SIZE = 8;        // 4 bytes for float (distance) + 4 bytes for int (retry count)
const int DISTANCE_ADDR = 0;      // EEPROM address for lastStableDistance (float)
const int RETRY_COUNT_ADDR = 4;   // EEPROM address for retryCount (int)
const int MAX_RETRIES = 5;        // Maximum number of retries before giving up on a notification
const char ACK_CHAR = 'K';        // Acknowledgment character from receiver ('K' for 'OK')
const long ACK_LISTEN_TIME_MS = 200; // How long to listen for ACK after sending

SoftwareSerial hc12(HC12_RX_PIN, HC12_TX_PIN); // RX, TX

// Data structure to be sent (MUST match receiver)
typedef struct {
    char message[20];
    int battery_percent;
} MailboxData;

MailboxData txData;
float lastStableDistance = 0.0; // Stored in EEPROM (Address 0)
int retryCount = 0;             // Stored in EEPROM (Address 4)

// Function prototypes
void goToDeepSleep();

// ---------------------- EEPROM UTILITIES ----------------------

void loadEepromData() {
    EEPROM.get(DISTANCE_ADDR, lastStableDistance);
    EEPROM.get(RETRY_COUNT_ADDR, retryCount);

    if (isnan(lastStableDistance) || lastStableDistance > 400.0) {
        lastStableDistance = INITIAL_DISTANCE_CM;
        Serial.println("EEPROM distance initialized/reset.");
    }
    if (retryCount < 0 || retryCount > MAX_RETRIES) {
        retryCount = 0;
        Serial.println("EEPROM retryCount reset.");
    }
    Serial.printf("EEPROM Loaded: Dist=%.1f cm, Retries=%d\n", lastStableDistance, retryCount);
}

void saveEepromData() {
    EEPROM.put(DISTANCE_ADDR, lastStableDistance);
    EEPROM.put(RETRY_COUNT_ADDR, retryCount);
    EEPROM.commit();
}


// ---------------------- UTILITIES ----------------------

// Reads the raw battery voltage using the voltage divider ratio
float readBatteryVoltage() {
    int raw = analogRead(BATTERY_PIN); 
    // Convert ADC raw value (0-1024) to the actual voltage measured at A0 (0-1.0V)
    float adcVoltage = ((float)raw / 1024.0) * ADC_MAX_V;
    
    // Scale the voltage measured at A0 back up to the actual battery voltage (V_bat)
    // V_bat = V_adc / VOLTAGE_DIVIDER_RATIO
    float batteryVoltage = adcVoltage / VOLTAGE_DIVIDER_RATIO;
    
    Serial.printf("Raw ADC: %d, V_ADC: %.3fV, V_Batt: %.3fV\n", raw, adcVoltage, batteryVoltage);
    return batteryVoltage;
}

// Calculates battery percentage (0-100)
int readBatteryPercentage(float voltage) {
    // Linear map from V_MIN_BATT (0%) to V_MAX_BATT (100%)
    int percent = map(voltage * 100, V_MIN_BATT * 100, V_MAX_BATT * 100, 0, 100);
    
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    return percent;
}

// Measures distance using the HC-SR04P
float readDistanceCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    // Speed of sound constant (0.0343 cm/µs) / 2
    float distanceCm = duration * 0.034 / 2;
    
    if (distanceCm == 0 || distanceCm > 400) return INITIAL_DISTANCE_CM; // Handle bad reads

    return distanceCm;
}

// ---------------------- TRANSMISSION ----------------------

// Transmits the current status and waits for an ACK. Returns true if ACK received, false otherwise.
bool transmitMailStatus(const char* status) {
    float battV = readBatteryVoltage();
    bool ackReceived = false;
    
    // 1. Prepare data
    strncpy(txData.message, status, 20);
    txData.battery_percent = readBatteryPercentage(battV);

    // 2. Transmit via HC-12 (Fire)
    hc12.write((byte*)&txData, sizeof(txData));
    hc12.flush(); // Wait for transmission to complete

    Serial.printf("TX: %s | Batt: %.2fV / %d%%\n", status, battV, txData.battery_percent);

    // 3. Status indication (brief flash while listening)
    digitalWrite(STATUS_LED_PIN, HIGH);

    // 4. Listen for ACK (Wait)
    unsigned long startTime = millis();
    while (millis() - startTime < ACK_LISTEN_TIME_MS) {
        if (hc12.available()) {
            char ackChar = hc12.read();
            if (ackChar == ACK_CHAR) {
                ackReceived = true;
                Serial.println("ACK received! Transmission successful.");
                break;
            }
        }
    }
    
    // 5. Cleanup
    digitalWrite(STATUS_LED_PIN, LOW);
    hc12.flush(); // Clear any remaining data in the buffer
    
    return ackReceived;
}

// ---------------------- SHUTDOWN LOGIC ----------------------

void checkBatteryAndShutdown() {
    float currentBatteryVoltage = readBatteryVoltage();
    
    if (currentBatteryVoltage < LOW_VOLTAGE_SHUTDOWN) {
        Serial.printf("\nCRITICAL: Battery voltage %.2fV is below shutdown threshold (%.2fV).\n", 
                      currentBatteryVoltage, LOW_VOLTAGE_SHUTDOWN);
        
        // 1. Send final "Charge battery" warning (We don't care about ACK here, this is the final cry)
        // Set the retry count to MAX_RETRIES + 1 to force permanent sleep immediately after this transmission
        retryCount = MAX_RETRIES + 1; 
        saveEepromData();
        
        transmitMailStatus("Charge battery"); // Attempts transmission once
        
        // 2. Enter permanent deep sleep
        Serial.println("Entering permanent deep sleep (ESP.deepSleep(0)) to protect battery.");
        ESP.deepSleep(0); 
        delay(100); // Wait for sleep to engage
    }
}


// ---------------------- EVENT HANDLERS ----------------------

// Handles the result of a transmission and manages retry count.
void handleTransmissionResult(bool success) {
    if (success) {
        // Transmission successful: Clear retry status
        retryCount = 0;
        Serial.println("Transmission success. Retry count cleared.");
        saveEepromData();
    } else {
        // Transmission failed: Increment retry count (if below max)
        if (retryCount < MAX_RETRIES) {
            retryCount++;
            Serial.printf("Transmission failed. Incrementing retry count to %d.\n", retryCount);
            saveEepromData();
        } else {
            // Max retries reached
            retryCount = 0;
            Serial.printf("Max retries (%d) reached. Aborting until next event.\n", MAX_RETRIES);
            saveEepromData();
        }
    }
    goToDeepSleep();
}

// Function to handle transmission attempts for any event
void attemptTransmission(const char* status, bool isNewEvent) {
    bool tx_success = transmitMailStatus(status);
    
    // If this is a new event (not a retry), we save the status message to EEPROM
    // so we know what to retry next time. We use the message buffer for this.
    if (isNewEvent) {
         // Since we already copied to txData.message, we'll store this status.
         // For simplicity and minimal EEPROM use, we only store the retryCount,
         // and rely on the event handler (Mail/Hall) to reconstruct the message
         // if the next wake-up is due to retryCount > 0.
         // Since we wake up every second, we rely on the state of the distance or Hall sensor
         // to determine the message if the retry is active.

         // We just set the retry count to 1 (failed on first attempt)
         if (!tx_success) {
            retryCount = 1;
         }
         // Distance/state is handled by the event functions.
    }
    
    handleTransmissionResult(tx_success);
}


// ---------------------- SETUP & LOOP ----------------------

void setup() {
    Serial.begin(115200);
    hc12.begin(9600); // HC-12 default baud rate

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
    
    #ifdef ENABLE_HALL_SENSOR
    pinMode(HALL_SENSOR_PIN, INPUT_PULLUP); // Initialize Hall sensor pin
    #endif
    
    // EEPROM setup and load state
    EEPROM.begin(EEPROM_SIZE); 
    loadEepromData();
    
    // --- CHECK 0: LOW VOLTAGE SHUTDOWN ---
    // This MUST be the very first check before any other heavy operation
    checkBatteryAndShutdown();


    // --- CHECK 1: RETRY LOGIC ---
    if (retryCount > 0 && retryCount < MAX_RETRIES) {
         Serial.printf("\nWoke from Deep Sleep. Pending Retry %d/%d.\n", retryCount, MAX_RETRIES);
         
         // In a retry, we must re-evaluate the status that caused the initial transmission.
         // 1. Mailbox Status Check (Distance)
         float currentDistance = readDistanceCm();
         const char* retry_message = NULL;
         
         if (abs(currentDistance - lastStableDistance) > DISTANCE_TOLERANCE_CM && currentDistance < 400) {
             retry_message = "Mail available";
         }
         
         // 2. Hall Sensor Check (Mail Picked Up) - Priority check
         #ifdef ENABLE_HALL_SENSOR
         if (digitalRead(HALL_SENSOR_PIN) == LOW) {
            retry_message = "Mail picked up";
         }
         #endif
         
         // 3. Test Button Check (Manual Reset/Test)
         if (digitalRead(TEST_BUTTON_PIN) == LOW) {
            retry_message = "Test successful";
         }

         if (retry_message) {
            Serial.printf("Retrying transmission for status: %s\n", retry_message);
            
            // Attempt transmission (isNewEvent=false)
            bool tx_success = transmitMailStatus(retry_message);
            handleTransmissionResult(tx_success);
            
         } else {
             // Status has cleared, but retry was pending. Clear retry count and sleep.
             Serial.println("Retry pending but event has cleared. Clearing retry count.");
             retryCount = 0;
             saveEepromData();
             goToDeepSleep();
         }
    }


    // --- CHECK 2: MAIL PICKED UP (HALL SENSOR) - NEW EVENT ---
    #ifdef ENABLE_HALL_SENSOR
    if (digitalRead(HALL_SENSOR_PIN) == LOW) {
        Serial.println("HALL SENSOR TRIGGERED: Mailbox door opened for retrieval!");
        
        float currentDistance = readDistanceCm();
        if (currentDistance < 400) {
            // Update distance before transmission attempt
            lastStableDistance = currentDistance;
            saveEepromData(); // Save new distance
        }
        
        attemptTransmission("Mail picked up", true); // Handle TX and sleep
    }
    #endif


    // --- CHECK 3: DEEP SLEEP WAKE (TIMER/DISTANCE) - NEW EVENT ---
    if (ESP.getResetReason() == "Deep-Sleep Wake" && retryCount == 0) { // Only check if not in a retry sequence
        Serial.println("\nWoke from Deep Sleep (Timer). Checking sensor...");
        
        float currentDistance = readDistanceCm();
        
        // --- Distance Change Detection ---
        if (lastStableDistance == INITIAL_DISTANCE_CM || (abs(currentDistance - lastStableDistance) > DISTANCE_TOLERANCE_CM && currentDistance < 400)) {
            
            if (currentDistance < 400) {
                Serial.printf("DISTANCE CHANGE DETECTED! Old: %.1f cm, New: %.1f cm\n", lastStableDistance, currentDistance);
                
                // Update distance before transmission attempt
                lastStableDistance = currentDistance;
                saveEepromData();
                
                attemptTransmission("Mail available", true); // Handle TX and sleep
            } else {
                 Serial.println("Bad reading, maintaining previous stable distance.");
            }
        } else {
            Serial.printf("No significant change. Current: %.1f cm, Last: %.1f cm\n", currentDistance, lastStableDistance);
        }
    } 
    
    // --- CHECK 4: TEST BUTTON (EXTERNAL INTERRUPT) WAKEUP or Manual Press - NEW EVENT ---
    if (digitalRead(TEST_BUTTON_PIN) == LOW && retryCount == 0) {
         Serial.println("Test Button Triggered Wakeup or Manual Press!");
         attemptTransmission("Test successful", true);
    }
    
    // Default to sleep if no event was triggered or handled
    goToDeepSleep();
}

void loop() {
    delay(500); 
    goToDeepSleep();
}

// ---------------------- DEEP SLEEP HANDLERS ----------------------

void goToDeepSleep() {
    Serial.println("Entering Deep Sleep for 1 second...");
    // D0 (GPIO16) must be connected to RST pin for deep sleep to work on Wemos D1 Mini
    ESP.deepSleep(1000000, RF_DEFAULT); // 1 second sleep (1000000 microseconds)
    delay(100); 
}