#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <MCP3XXX.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <wxsta.h>  // configuration loaded from header

#define SEALEVELPRESSURE_HPA 1013.25
#define WAKEUP_INTERVAL_US 6e7 //9e8  // 15 minutes


Adafruit_BME280 bme;
MCP3002 mcp3002;
WiFiClient wifiSerial;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


void setup()
{ 
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.hostname("wxsta");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
  digitalWrite(LED_BUILTIN, HIGH);
  if (!wifiSerial.connected()) {
    wifiSerial.stop();
    wifiSerial.connect("fossil.local", 3000);
  }

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.begin("wxsta");
  MDNS.addService("http", "tcp", 80);

  bme.begin(0x76);
  bme.setSampling(  // use recommended settings for low-power weather monitoring
    bme.MODE_FORCED,  // sleep after each reading
    bme.SAMPLING_X1,  // temperature 1x oversample
    bme.SAMPLING_X1,  // pressure 1x oversample
    bme.SAMPLING_X1,  // humidity 1x oversample
    bme.FILTER_OFF,   // no IIR filtering
    bme.STANDBY_MS_1000  // 1 sec standby duration (don't think this matters in forced mode?)
  );
  
  mcp3002.begin(0);  // move SPI CS pin to GPIO0 so it can pull high (GPIO15 must pull low)
  
  timeClient.begin();

  float temp = bme.readTemperature();
  float press = bme.readPressure() / 100.f;
  float humidity = bme.readHumidity();
  
  float ch0 = mcp3002.analogRead(0) / 1023.f * 100.f;
  float ch1 = mcp3002.analogRead(1) / 1023.f * 100.f;

  timeClient.update();
  uint32 epochTime = timeClient.getEpochTime();
  rst_info *resetInfo = ESP.getResetInfoPtr();
  uint32 sleptFor = 0;
  uint32 lastTime;
  if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE) 
  {
     ESP.rtcUserMemoryRead(0, &lastTime, sizeof(lastTime));
     sleptFor = epochTime - lastTime - millis()/1000.;
  }
  ESP.rtcUserMemoryWrite(0,  &epochTime, sizeof(epochTime));
  wifiSerial.println("********************");
  wifiSerial.println(timeClient.getFormattedTime());
  wifiSerial.printf("CH0      = %.2f%%\n", ch0);
  wifiSerial.printf("CH1      = %.2f%%\n", ch1);
  wifiSerial.printf("Temp.    = %.2f *C\n", temp);
  wifiSerial.printf("Pressure = %.2f hPa\n", press); 
  wifiSerial.printf("Humidity = %.2f%%\n", humidity);
  wifiSerial.printf("Slept for %d\n", sleptFor); // TODO
  wifiSerial.stop();
  delay(250); // this delay seems to solve recon issues, check for serial stop instead?
  //httpServer.handleClient();
  //delay(10 * 1000);
  // TODO calibrate?
  ESP.deepSleep(WAKEUP_INTERVAL_US - micros());
}

void loop() {};