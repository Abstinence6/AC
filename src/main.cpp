#include <Arduino.h>

#include <IRsend.h>
#include <ir_Gree.h>
//#include <IRGree.h>

#include <ArduinoHA.h>

#include <ESP8266WiFi.h>  
#include <ESP8266HTTPClient.h>

#include "ArduinoOTA.h"

#include <Wire.h>

#include "EEPROM.h"

const char* ssid = "Mi Wi-Fi";
const char* password = "12345678";
byte mac[] = {0x40, 0xf5, 0x20, 0x33, 0x82, 0xff};

WiFiClient client;
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);

#define BROKER_ADDR     IPAddress(192,168,1,159)

IRGreeAC irg(D8, gree_ac_remote_model_t::YAW1F , false , true);

HAHVAC hvac("Gree", HAHVAC::DefaultFeatures | HAHVAC::ActionFeature | HAHVAC::PowerFeature | HAHVAC::FanFeature | HAHVAC::ModesFeature | HAHVAC::TargetTemperatureFeature | HAHVAC::SwingFeature);

HASensorNumber Temp("Temp", HASensorNumber::PrecisionP1);
HASensorNumber Hum("Hum", HASensorNumber::PrecisionP1);

#define Addr 0x45

unsigned long lastTempPublishAt = 0;
float lastTemp = 0;

void SetState(){
  mqtt.loop();
  HAHVAC::Mode mode = hvac.getCurrentMode();
  if (mode == HAHVAC::OffMode) {
    irg.off();
    Serial.println("off");
  } else if (mode == HAHVAC::AutoMode) {
    irg.setMode(kGreeAuto);
    Serial.println("auto");
  } else if (mode == HAHVAC::CoolMode) {
    irg.setMode(kGreeCool);
    Serial.println("cool");
  } else if (mode == HAHVAC::HeatMode) {
    irg.setMode(kGreeHeat);
    Serial.println("heat");
  } else if (mode == HAHVAC::DryMode) {
    irg.setMode(kGreeDry);
    Serial.println("dry");
  } else if (mode == HAHVAC::FanOnlyMode) {
    irg.setMode(kGreeFan);
    Serial.println("fan only");
  } else {
    Serial.println("No mode");
  }

  HAHVAC::FanMode FanMode  = hvac.getCurrentFanMode();
  if (FanMode == HAHVAC::AutoFanMode) {
    irg.setFan(kGreeFanAuto);
    Serial.println("AutoFanMode");
  } else if (FanMode == HAHVAC::LowFanMode) {
    irg.setFan(kGreeFanMin);
    Serial.println("LowFanMode");
  } else if (FanMode == HAHVAC::MediumFanMode) {
    irg.setFan(kGreeFanMed);
    Serial.println("MediumFanMode");
  } else if (FanMode == HAHVAC::HighFanMode) {
    irg.setFan(kGreeFanMax);
    Serial.println("HighFanMode");
  } else{
    Serial.println("No FanMode");
  }

  irg.setTemp(hvac.getCurrentTargetTemperature().toUInt16()/10,false); 
  Serial.print("Temperature: ");
  Serial.println(hvac.getCurrentTargetTemperature().toUInt16()/10);

  irg.setSwingVertical(hvac.getCurrentSwingMode() == HAHVAC::OnSwingMode,0);
  Serial.print("OnSwingMode: ");
  Serial.println(hvac.getCurrentSwingMode() == HAHVAC::OnSwingMode);
  
  irg.setPower(hvac.getCurrentMode() != HAHVAC::OffMode);
  Serial.print("Power: ");
  Serial.println(hvac.getCurrentMode() != HAHVAC::OffMode);
}

void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender) {
    sender->setTargetTemperature(temperature); 
    irg.on();
    SetState();
    irg.send();

    EEPROM.put(sizeof(HAHVAC::Mode),(int16_t)(temperature.toUInt16()/10)); 
    EEPROM.commit();

    mqtt.loop();
}

void onModeCommand(HAHVAC::Mode mode, HAHVAC* sender) {
  sender->setMode(mode); // report mode back to the HA panel
  irg.on();
  SetState();
  irg.send();

  EEPROM.put(0,mode);
  EEPROM.commit();

  mqtt.loop();
}

void onFanCommand(HAHVAC::FanMode mode, HAHVAC* sender) {
  sender->setFanMode(mode); 
  irg.on();
  SetState();
  irg.send();

  EEPROM.put(sizeof(HAHVAC::Mode) + sizeof(int16_t),mode); 
  EEPROM.commit();

  mqtt.loop();
}

void onSwingCommand(HAHVAC::SwingMode mode, HAHVAC* sender) {
  sender->setSwingMode(mode);
  irg.on();
  SetState();
  irg.send();

  EEPROM.put(sizeof(HAHVAC::Mode) + sizeof(int16_t) + sizeof(HAHVAC::FanMode),mode); 
  EEPROM.commit();

  mqtt.loop();
}

void onPowerCommand(bool state, HAHVAC* sender) {
  SetState();
  irg.send();
  mqtt.loop();
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.print("\nConnecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  device.setName("AC");
  device.setSoftwareVersion("1.0.0");
  device.enableSharedAvailability();
  device.enableLastWill();

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  mqtt.begin(BROKER_ADDR,1883,"HAMQTT","gisterezis");
  Serial.println("MQTT connected");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("OTA End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");

  hvac.setName("Gree");
  hvac.setMinTemp(16);
  hvac.setMaxTemp(30);
  hvac.setTempStep(1);
  hvac.onFanModeCommand(onFanCommand);
  hvac.onSwingModeCommand(onSwingCommand);
  hvac.onPowerCommand(onPowerCommand);
  hvac.onModeCommand(onModeCommand);
  hvac.onTargetTemperatureCommand(onTargetTemperatureCommand);

  Temp.setIcon("mdi:thermometer");
  Temp.setName("Temperature");

  Hum.setIcon("mdi:water-percent");
  Hum.setName("Humidity");

  irg.begin();

  Wire.begin();

  mqtt.loop();

  EEPROM.begin(32);

  HAHVAC::Mode mode;
  EEPROM.get(0,mode);
  hvac.setCurrentMode(mode);   

  int16_t temp;
  EEPROM.get(sizeof(HAHVAC::Mode),temp);
  hvac.setTargetTemperature(temp);   

  HAHVAC::FanMode Fanmode;
  EEPROM.get(sizeof(HAHVAC::Mode) + sizeof(int16_t),Fanmode);
  hvac.setCurrentFanMode(Fanmode);

  HAHVAC::SwingMode Swingmode;
  EEPROM.get(sizeof(HAHVAC::Mode) + sizeof(int16_t) + sizeof(HAHVAC::FanMode),Swingmode);
  hvac.setCurrentSwingMode(Swingmode);

  SetState();
}

void loop() {
  ArduinoOTA.handle();

  if ((millis() - lastTempPublishAt) > 3000) {
    unsigned int data[6];
  
    Wire.beginTransmission(Addr);
    Wire.write(0x2C);
    Wire.write(0x06);
    Wire.endTransmission();
    delay(10);

    Wire.requestFrom(Addr, 6);

    if (Wire.available() == 6)
    {
    data[0] = Wire.read();
    data[1] = Wire.read();
    data[2] = Wire.read();
    data[3] = Wire.read();
    data[4] = Wire.read();
    data[5] = Wire.read();
    }

    float cTemp = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;
    float humidity = ((((data[3] * 256.0) + data[4]) * 100) / 65535.0);
     
    hvac.setCurrentTemperature(cTemp);

    Temp.setValue(cTemp-2);
    Hum.setValue(humidity);
    lastTempPublishAt = millis();
  }
  mqtt.loop();
}