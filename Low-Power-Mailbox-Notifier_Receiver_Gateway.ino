/*
 * PROJECT: Mailbox Notifier - Receiver (Always-On Gateway)
 * DEVICE: WeMos D1 Mini ESP8266 ESP-12F
 * RF MODULE: HC-12 433MHz Serial Wireless Module
 *
 * FUNCTIONALITY:
 * 1. Always On: Powered via USB, maintains constant operation.
 * 2. Time Synchronization (Mandatory): Connects to WiFi, synchronizes date/time via NTP.
 * 3. HC-12 Listener: Continuously listens for incoming packets from the transmitter.
 * 4. LCD Display: Shows standby status ("No mail yet" + current time) or received message status (message + reception time).
 * 5. Indicator LED: Starts a "breathing" PWM effect on reception, indicating a new notification.
 * 6. MQTT Gateway (Optional): Publishes the received message, reception time, and battery level to MQTT.
 * 7. **ACK SENDER**: Sends an Acknowledgment ('K') back to the transmitter upon successful reception.
 *
 * PINOUT INSTRUCTIONS & HARDWARE CONNECTIONS:
 * - POWER INPUT: Constant 5V power adapter connected to the Wemos D1 Mini USB port.
 * - HC-12 TX Pin: D6 (GPIO 12). HC-12 RX -> Wemos D6/GPIO 12.
 * - HC-12 RX Pin: D5 (GPIO 14). HC-12 TX -> Wemos D5/GPIO 14.
 * - I2C LCD SDA: D2 (GPIO 4).
 * - I2C LCD SCL: D1 (GPIO 5).
 * - INDICATOR_LED_PIN: D7 (GPIO 13) - Breathing LED (PWM capable) - Requires external LED connected to 3.3V/GND with resistor.
 * - RESET_BUTTON_PIN: D8 (GPIO 15) - TTP223 Touch Button Output connected here. Uses INPUT_PULLUP.
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// --------------------------------------------------------------------------------------
// *** RECEIVER FEATURE CONFIGURATION ***
// 
// WiFi and NTP time synchronization are mandatory for time display on the LCD.
// Uncomment the line below ONLY if you want to enable MQTT publishing to Home Assistant.
// --------------------------------------------------------------------------------------
//#define ENABLE_MQTT
// --------------------------------------------------------------------------------------


// --- WIFI & MQTT CONFIGURATION ---
// WiFi is required for NTP Time Synchronization
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

#ifdef ENABLE_MQTT
const char* MQTT_SERVER = "YOUR_MQTT_BROKER_IP";
const char* MQTT_TOPIC_BASE = "home/mailbox";
const int MQTT_PORT = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
#endif

// --- PIN DEFINITIONS (Wemos D1 Mini) ---
#define HC12_TX_PIN D6      // GPIO12 - HC-12 RX Pin
#define HC12_RX_PIN D5      // GPIO14 - HC-12 TX Pin
#define LCD_SDA_PIN D2      // GPIO4 - I2C Data
#define LCD_SCL_PIN D1      // GPIO5 - I2C Clock
#define INDICATOR_LED_PIN D7 // GPIO13 - Breathing LED (PWM capable)
#define RESET_BUTTON_PIN D8 // GPIO15 - TTP223 Output (active LOW if configured that way, using INPUT_PULLUP)

// --- COMMUNICATION CONFIG ---
const char ACK_CHAR = 'K'; // Acknowledgment character sent back to transmitter

// --- COMPONENT OBJECTS ---
SoftwareSerial hc12(HC12_RX_PIN, HC12_TX_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Common I2C address (0x27)

// --- DATA STRUCTURES & STATE ---
typedef struct {
    char message[20];
    int battery_percent;
} MailboxData;

MailboxData rxData;

enum Status { STANDBY, NOTIFICATION };
Status currentStatus = STANDBY;

int lastBatteryPercent = -1; // Stores last received battery level
unsigned long statusResetTime = 0; // Timer for battery display mode

// --- TIME CONFIG (Mandatory) ---
const char* ntpServer = "pool.ntp.org";

// TZ string for Central European Time (CET) - Handles DST automatically
const char* tzInfo = "CET-1CEST,M3.5.0,M10.5.0/3";


// ---------------------- UTILITIES & WIRING ----------------------

void setup_wifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lcd.clear();
    lcd.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lcd.print(".");
    }
    Serial.println("WiFi connected.");
    
    // Initialize Time (NTP) using the TZ string for automatic DST handling
    configTime(tzInfo, ntpServer);
    Serial.println("Time synchronized with DST rules.");
}

#ifdef ENABLE_MQTT
void mqttReconnect() {
    while (!client.connected()) {
        String clientId = "WemosD1Mini-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            Serial.println("MQTT connected.");
        } else {
            Serial.print("MQTT failed, rc=");
            Serial.print(client.state());
            delay(5000);
        }
    }
}
#endif // ENABLE_MQTT

String getCurrentTime() {
    struct tm timeinfo;
    
    // Attempt to get local time after TZ setup
    if(!getLocalTime(&timeinfo)){
        // If time sync fails (e.g., WiFi drops), show a message
        if (WiFi.status() == WL_CONNECTED) {
            configTime(tzInfo, ntpServer); 
        }
        return "Time Error"; 
    }
    
    char timeStr[20];
    // DD.MM.YY, HH:MM
    strftime(timeStr, 20, "%d.%m.%y, %H:%M", &timeinfo);
    return String(timeStr);
}

// ---------------------- DISPLAY & LED CONTROL ----------------------

void displayStandby() {
    lcd.setCursor(0, 0);
    lcd.print("No mail yet     ");
    lcd.setCursor(0, 1);
    // Mandatory use of getCurrentTime() requires WiFi/NTP
    lcd.print(getCurrentTime());
}

void displayNotification() {
    String receptionTime = getCurrentTime();
    
    lcd.setCursor(0, 0);
    lcd.print(rxData.message);
    lcd.print("           "); // Clear the rest of the line
    
    lcd.setCursor(0, 1);
    lcd.print(receptionTime);
    
    // Start the breathing effect
    currentStatus = NOTIFICATION;
}

void displayBatteryStatus() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Battery status:");
    lcd.setCursor(0, 1);
    if (lastBatteryPercent != -1) {
        lcd.printf("%d%% (%s)", lastBatteryPercent, (lastBatteryPercent > 10) ? "OK" : "LOW");
    } else {
        lcd.print("No data received");
    }
    statusResetTime = millis();
}

void handleBreathingLED() {
    static int brightness = 0;
    static int fadeAmount = 5;
    
    if (currentStatus == NOTIFICATION) {
        // Breathing logic 
        brightness += fadeAmount;
        if (brightness <= 0 || brightness >= 255) {
            fadeAmount = -fadeAmount;
        }
        analogWrite(INDICATOR_LED_PIN, brightness);
    } else {
        analogWrite(INDICATOR_LED_PIN, 0); // Off in STANDBY mode
    }
}

// ---------------------- INTERRUPT & HANDLERS ----------------------

// Handler for the TTP223 Touch Button
void handleResetButton() {
    // Read the actual pin state to ensure it's not a bounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        currentStatus = STANDBY;
        displayBatteryStatus(); // Display battery status for 5 seconds
    }
}


// ---------------------- COMMUNICATION HANDLERS ----------------------

void handleHc12Receive() {
    while (hc12.available() >= sizeof(MailboxData)) {
        // Read the packet
        hc12.readBytes((byte*)&rxData, sizeof(MailboxData));
        Serial.print("HC-12 Received: ");
        Serial.println(rxData.message);
        
        lastBatteryPercent = rxData.battery_percent;
        
        // 1. Update Display & LED
        displayNotification();
        
        // 2. Publish to MQTT (Conditional)
        #ifdef ENABLE_MQTT
        if (!client.connected()) {
            mqttReconnect();
        }
        
        String timeStr = getCurrentTime();
        
        // Message Payload
        String payload = String(rxData.message);
        client.publish( (String(MQTT_TOPIC_BASE) + "/message").c_str(), payload.c_str());
        
        // Battery Payload
        char batteryPayload[5];
        sprintf(batteryPayload, "%d", rxData.battery_percent);
        client.publish( (String(MQTT_TOPIC_BASE) + "/battery").c_str(), batteryPayload);
        
        // Time Payload
        client.publish( (String(MQTT_TOPIC_BASE) + "/reception_time").c_str(), timeStr.c_str());
        
        Serial.println("MQTT publish complete.");
        #else
        Serial.println("MQTT disabled. Skipping MQTT publish.");
        #endif // ENABLE_MQTT

        // 3. Send Acknowledgment (ACK) back to the transmitter
        hc12.write(ACK_CHAR); 
        hc12.flush();
        Serial.printf("ACK sent to transmitter ('%c').\n", ACK_CHAR);
    }
}

// ---------------------- SETUP & LOOP ----------------------

void setup() {
    Serial.begin(115200);
    hc12.begin(9600);
    
    // Initialize Pins
    pinMode(INDICATOR_LED_PIN, OUTPUT);
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RESET_BUTTON_PIN), handleResetButton, FALLING);
    
    // LCD Setup
    lcd.init();
    lcd.backlight();
    lcd.print("Mailbox Gateway");
    
    // WiFi & NTP Setup (Mandatory for time display)
    setup_wifi();
    
    #ifdef ENABLE_MQTT
    // MQTT Setup (Optional)
    client.setServer(MQTT_SERVER, MQTT_PORT);
    #else
    lcd.setCursor(0, 1);
    lcd.print("MQTT Disabled");
    delay(1500);
    #endif
}

void loop() {
    // MQTT Maintenance (Conditional)
    #ifdef ENABLE_MQTT
    if (!client.connected()) {
        mqttReconnect();
    }
    client.loop();
    #endif
    
    // HC-12 Receive Check
    handleHc12Receive();
    
    // Display & LED Management
    handleBreathingLED();
    
    if (currentStatus == STANDBY) {
        if (statusResetTime == 0 || millis() - statusResetTime >= 5000) {
            // After 5 seconds of showing battery status, return to Standby
            statusResetTime = 0;
            displayStandby();
        }
    }
    
    delay(50); // Loop delay
}