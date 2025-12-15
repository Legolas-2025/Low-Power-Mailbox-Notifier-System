# 🏠 Home Assistant Integration Guide

This guide covers integrating the Low-Power Mailbox Notifier system with Home Assistant for seamless home automation.

---

## 📋 **Prerequisites**

### ✅ **Required Components**
- Home Assistant instance (HassOS or Supervised)
- ESPHome add-on installed
- MQTT broker (Mosquitto add-on recommended)
- Network access to Home Assistant

### 🔧 **Software Requirements**
```yaml
# Minimum Home Assistant version
home_assistant: "2023.1.0"

# Required add-ons
addons:
  - ESPHome
  - Mosquitto broker
```

---

## 🔧 **ESPHome Configuration**

### 1️⃣ **Install ESPHome**

**Via Home Assistant Add-ons:**
1. Go to **Settings** → **Add-ons**
2. Search for **ESPHome**
3. Install the ESPHome add-on
4. Start the add-on

**Via Command Line (Advanced):**
```bash
pip install esphome
```

### 2️⃣ **Configure ESPHome Project**

**Create ESPHome Directory:**
```bash
mkdir -p /config/esphome/
cd /config/esphome/
```

**Copy Configuration Files:**
```bash
cp mailbox_receiver.yaml .
cp secrets_template.yaml secrets.yaml
```

### 3️⃣ **Edit Secrets File**

**Update `/config/esphome/secrets.yaml`:**
```yaml
# WiFi Configuration
wifi_ssid: "Your_WiFi_Network"
wifi_password: "Your_WiFi_Password"

# Home Assistant API (auto-generated when adding device)
home_assistant_api_key: "YOUR_API_KEY"

# OTA Password
ota_password: "mailbox_ota_2025"

# Web Server Password
web_password: "mailbox_web_2025"

# MQTT Configuration
mqtt_broker: "core-mosquitto"
mqtt_port: 1883
mqtt_username: "homeassistant"
mqtt_password: "YOUR_MQTT_PASSWORD"
```

### 4️⃣ **Compile and Flash**

**Compile Configuration:**
```bash
esphome run mailbox_receiver.yaml
```

**Flash Options:**
1. **USB Upload**: Connect device via USB
2. **OTA Update**: Upload over network (if device already running ESP8266)

---

## 📱 **Home Assistant Device Setup**

### 🔗 **Automatic Discovery**

After flashing ESPHome:
1. **Home Assistant will auto-discover** the device
2. **Notification will appear** in Home Assistant
3. **Click "Configure"** to add device

### 🛠️ **Manual Configuration**

**If auto-discovery fails:**

1. **Go to Settings** → **Devices & Services**
2. **Click "Add Integration"**
3. **Search for "ESPHome"**
4. **Enter device details:**
   - Host: IP address of ESP8266
   - Port: 6053 (default ESPHome port)

---

## 📊 **Sensor Configuration**

### 📈 **Available Entities**

**Text Sensors:**
- `sensor.mailbox_status` - Current mailbox state
- `sensor.mailbox_last_reception_time` - Last reception timestamp

**Sensors:**
- `sensor.mailbox_battery_level` - Transmitter battery percentage
- `sensor.mailbox_receiver_wifi_signal` - Gateway WiFi signal strength

**Binary Sensors:**
- `binary_sensor.mailbox_presence_detected` - Motion detection
- `binary_sensor.mailbox_reset_button` - Manual reset button

**Switches:**
- `switch.mailbox_indicator_led` - Manual LED control
- `switch.mailbox_lcd_backlight` - Display backlight control

### 🎛️ **Control Entities**

**Buttons:**
- `button.mailbox_reset_notification` - Clear notifications

**Lights:**
- `light.mailbox_breathing_led` - Breathing LED effect

---

## 🎯 **Automation Examples**

### 📮 **Basic Mail Notification**

```yaml
# automations.yaml
- id: mailbox_notification
  alias: "Mailbox Notification"
  description: "Send notification when mail arrives"
  trigger:
    - platform: state
      entity_id: sensor.mailbox_status
      to: "Mail available"
  condition: []
  action:
    - service: notify.mobile_app_iphone
      data:
        title: "📮 New Mail"
        message: "Mail has been delivered to your mailbox"
        data:
          image: "/api/camera_proxy/camera.mailbox_cam"  # If you have a camera
    - service: persistent_notification.create
      data:
        title: "Mailbox Update"
        message: "Mail detected at {{ now().strftime('%H:%M') }}"
        notification_id: mailbox_update

- id: mailbox_cleared
  alias: "Mail Cleared"
  description: "Send notification when mail is picked up"
  trigger:
    - platform: state
      entity_id: sensor.mailbox_status
      to: "Mail picked up"
  action:
    - service: notify.mobile_app_iphone
      data:
        title: "📫 Mail Collected"
        message: "Mail has been picked up from the mailbox"
```

### 🔋 **Battery Monitoring**

```yaml
# automations.yaml
- id: mailbox_battery_low
  alias: "Mailbox Battery Low"
  description: "Alert when transmitter battery is low"
  trigger:
    - platform: numeric_state
      entity_id: sensor.mailbox_battery_level
      below: 20
      above: 0
  condition:
    - condition: state
      entity_id: person.your_name
      state: "home"
  action:
    - service: notify.mobile_app_iphone
      data:
        title: "🔋 Low Battery Warning"
        message: "Mailbox transmitter battery is at {{ states('sensor.mailbox_battery_level') }}%"
    - service: persistent_notification.create
      data:
        title: "Mailbox Battery Alert"
        message: "Battery level: {{ states('sensor.mailbox_battery_level') }}%"
        notification_id: mailbox_battery

- id: mailbox_battery_critical
  alias: "Mailbox Battery Critical"
  description: "Critical battery alert"
  trigger:
    - platform: numeric_state
      entity_id: sensor.mailbox_battery_level
      below: 5
  action:
    - service: notify.mobile_app_iphone
      data:
        title: "⚠️ CRITICAL: Mailbox Battery"
        message: "URGENT: Transmitter battery is critically low ({{ states('sensor.mailbox_battery_level') }}%)"
        priority: high
```

### 💡 **Smart Lighting Integration**

```yaml
# automations.yaml
- id: mailbox_presence_lighting
  alias: "Mailbox Presence Lighting"
  description: "Turn on lights when presence detected"
  trigger:
    - platform: state
      entity_id: binary_sensor.mailbox_presence_detected
      to: "on"
  condition:
    - condition: sun
      after: sunset
      before: sunrise
  action:
    - service: light.turn_on
      entity_id: light.entrance_lights
      data:
        brightness: 128
        color_temp: 300
    - delay: "00:05:00"  # 5 minutes
    - service: light.turn_off
      entity_id: light.entrance_lights
```

### 📊 **Daily Summary**

```yaml
# automations.yaml
- id: mailbox_daily_summary
  alias: "Mailbox Daily Summary"
  description: "Send daily mailbox summary"
  trigger:
    - platform: time
      at: "18:00:00"
  condition:
    - condition: state
      entity_id: person.your_name
      state: "home"
  action:
    - service: script.turn_on
      entity_id: script.mailbox_daily_report
```

---

## 📋 **Configuration Files**

### 🔧 **Customization Script**

**Create `/config/scripts/mailbox_summary.yaml`:**
```yaml
mailbox_daily_report:
  alias: "Mailbox Daily Report"
  sequence:
    - service: notify.mobile_app_iphone
      data:
        title: "📊 Mailbox Daily Report"
        message: |
          📮 Status: {{ states('sensor.mailbox_status') }}
          🔋 Battery: {{ states('sensor.mailbox_battery_level') }}%
          📡 Signal: {{ states('sensor.mailbox_receiver_wifi_signal') }} dBm
          🕐 Last Update: {{ states('sensor.mailbox_last_reception_time') }}
```

### 📱 **Dashboard Card**

**Create Custom Dashboard Card:**
```yaml
# views.yaml
title: Mailbox System
path: mailbox
icon: mdi:mailbox

cards:
  - type: entities
    title: Mailbox Status
    entities:
      - sensor.mailbox_status
      - sensor.mailbox_battery_level
      - sensor.mailbox_last_reception_time
      - sensor.mailbox_receiver_wifi_signal

  - type: picture-elements
    image: /local/mailbox_icon.png
    elements:
      - type: state-icon
        entity: sensor.mailbox_status
        icon: mdi:mailbox
        style:
          top: 50%
          left: 50%

  - type: markdown
    content: |
      ## 📊 Mailbox System Status
      
      **Current Status:** {{ states('sensor.mailbox_status') }}
      
      **Battery Level:** {{ states('sensor.mailbox_battery_level') }}%
      
      **Last Reception:** {{ states('sensor.mailbox_last_reception_time') }}
      
      **WiFi Signal:** {{ states('sensor.mailbox_receiver_wifi_signal') }} dBm
```

---

## 🔧 **Advanced Configuration**

### 📡 **MQTT Integration (Alternative)**

**If not using ESPHome integration:**

```yaml
# configuration.yaml
mqtt:
  broker: core-mosquitto
  port: 1883
  username: homeassistant
  password: YOUR_MQTT_PASSWORD

# Sensor configuration
sensor:
  - platform: mqtt
    name: "Mailbox Status"
    state_topic: "home/mailbox/message"
    
  - platform: mqtt
    name: "Mailbox Battery"
    state_topic: "home/mailbox/battery"
    unit_of_measurement: "%"
    
  - platform: mqtt
    name: "Mailbox Reception Time"
    state_topic: "home/mailbox/reception_time"

# Switch for manual reset
switch:
  - platform: mqtt
    name: "Mailbox Reset"
    command_topic: "home/mailbox/reset"
    payload_off: "reset"
```

### 🎛️ ** Lovelace UI Cards**

**Add to Dashboard:**

```yaml
# Custom card configuration
type: vertical-stack
cards:
  - type: sensor
    entity: sensor.mailbox_status
    graph: line
    
  - type: gauge
    entity: sensor.mailbox_battery_level
    min: 0
    max: 100
    severity:
      green: 50
      yellow: 25
      red: 10
      
  - type: entities
    title: Controls
    entities:
      - button.mailbox_reset_notification
      - switch.mailbox_indicator_led
      - switch.mailbox_lcd_backlight
```

---

## 🔍 **Monitoring & Maintenance**

### 📊 **System Health Monitoring**

```yaml
# Template sensors for health monitoring
template:
  - sensor:
      - name: "Mailbox System Health"
        state: >
          {% set battery = states('sensor.mailbox_battery_level') | float %}
          {% set signal = states('sensor.mailbox_receiver_wifi_signal') | float %}
          {% set status = states('sensor.mailbox_status') %}
          
          {% if battery > 20 and signal > -70 and status != 'Charge battery' %}
            Healthy
          {% elif battery < 20 or signal < -80 %}
            Warning
          {% else %}
            Critical
          {% endif %}
        attributes:
          battery_level: "{{ states('sensor.mailbox_battery_level') }}"
          signal_strength: "{{ states('sensor.mailbox_receiver_wifi_signal') }}"
          last_update: "{{ states('sensor.mailbox_last_reception_time') }}"
```

### 📱 **Mobile Notifications**

**iOS/Android Configuration:**

```yaml
# iOS notification configuration
ios:
  push:
    categories:
      - name: "Mailbox Alerts"
        identifier: "mailbox_category"
        actions:
          - identifier: "MARK_READ"
            title: "Mark as Read"
            activationMode: "background"
```

### 📊 **Statistics Tracking**

```yaml
# Recorder configuration for long-term data
recorder:
  purge_keep_days: 30
  exclude:
    entities:
      - sensor.mailbox_receiver_wifi_signal  # Too frequent updates
```

---

## 🛠️ **Troubleshooting**

### 🔧 **Common Issues**

**Device Not Appearing:**
1. Check ESPHome logs for connection errors
2. Verify WiFi credentials in secrets.yaml
3. Ensure device is powered and in range

**MQTT Connection Issues:**
1. Verify MQTT broker is running
2. Check username/password in secrets.yaml
3. Review firewall settings

**Sensor Data Missing:**
1. Check HC-12 communication between devices
2. Verify transmitter battery level
3. Monitor ESPHome logs for errors

### 📋 **Log Monitoring**

**ESPHome Logs:**
```bash
# View real-time logs
esphome logs mailbox_receiver.yaml

# Common log entries to monitor
[INFO] Starting log output from 192.168.1.100
[WARN] WiFi connection lost, attempting reconnection
[ERROR] HC-12 communication timeout
[INFO] MQTT connection established
```

**Home Assistant Logs:**
```bash
# Check HA logs for integration issues
docker logs homeassistant | grep esp
docker logs homeassistant | grep mqtt
```

---

## 📚 **Additional Resources**

### 🔗 **Helpful Links**
- [ESPHome Documentation](https://esphome.io/)
- [Home Assistant MQTT Integration](https://www.home-assistant.io/docs/mqtt/)
- [ESP8266 Pinout Reference](https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/)
- [HC-12 Module Guide](https://www.elecfreaks.com/blog/hc-12-wireless-serial-port-communication-module-configuration/)

### 💬 **Support Channels**
- [ESPHome Discord](https://discord.gg/KhXKNRD)
- [Home Assistant Community](https://community.home-assistant.io/)
- [GitHub Issues](https://github.com/Legolas-2025/Low-Power-Mailbox-Notifier/issues)

---

This completes the Home Assistant integration guide. The system should now be fully integrated with Home Assistant for comprehensive mailbox monitoring and automation.