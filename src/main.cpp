/*
 * To start mDNS monitor (OSX) exec:   dns-sd -B _arduino._tcp
 */

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Blinker.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <PZEM004Tv30.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include "OneButton.h"
#include <WebSocketsServer.h>

#define LED_RED 13
#define LED_GREEN 14
#define LED_BLUE 2
#define BTN_FLASH 0

#define LED_ON 0
#define LED_OFF 1

#define SEND_INTERVAL 6000
#define SEND_INTERVAL_WS 1000

struct mqtt_config {
  char check[3] = "";
  char server[40] = "";
  char port[6] = "";
  char user[40] = "";
  char pass[40] = "";
  char hostname[40] = "";
};
struct mqtt_config mqtt;

unsigned long sendInterval = SEND_INTERVAL;

unsigned long previousMillis = 0;

Blinker blinker_red(LED_RED);
Blinker blinker_green(LED_GREEN);
OneButton button(BTN_FLASH, true);
PZEM004Tv30 pzem(&Serial);

WiFiClient client;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Adafruit_MQTT_Client *mqtt_client;
Adafruit_MQTT_Publish *mqtt_powerData;
Adafruit_MQTT_Publish *mqtt_status;
String getDataTopic;
String getStatusTopic;


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            if (webSocket.connectedClients(true) == 0) {
                sendInterval = SEND_INTERVAL;
            }
            break;
        case WStype_CONNECTED:
            // send data more often when WS client is connected
            sendInterval = SEND_INTERVAL_WS;
            break;
    }
}


String getContentType(String filename){
    if(server.hasArg("download")) return "application/octet-stream";
    else if(filename.endsWith(".htm")) return "text/html";
    else if(filename.endsWith(".html")) return "text/html";
    else if(filename.endsWith(".css")) return "text/css";
    else if(filename.endsWith(".js")) return "application/javascript";
    else if(filename.endsWith(".png")) return "image/png";
    else if(filename.endsWith(".jpg")) return "image/jpeg";
    else if(filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

void handleRoot()
{
    String path = "/index.html";
    String contentType = getContentType(path);
    if (SPIFFS.exists(path))
    {
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
    }
}

bool handleFileRead(String path)
{
    String contentType = getContentType(path);
    if (SPIFFS.exists(path))
    {
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect()
{
    int8_t ret;

    // Stop if already connected.
    if (mqtt_client->connected())
    {
        return;
    }

    // Serial.print("Connecting to MQTT... ");

    uint8_t retries = 5;
    while ((ret = mqtt_client->connect()) != 0)
    { // connect will return 0 for connected
        // Serial.println(mqtt_client.connectErrorString(ret));
        // Serial.println(MQTT_SERVER);
        // Serial.println("Retrying MQTT connection in 2 seconds...");
        mqtt_client->disconnect();
        for (int j = 0; j < 10; j++)
        {
            digitalWrite(LED_RED, LED_ON);
            delay(50);
            digitalWrite(LED_RED, LED_OFF);
            delay(50);
        }
        delay(1000);
        retries--;
        if (retries == 0)
        {
            // basically die and wait for WDT to reset me
            while (1)
                ;
        }
    }
    // Serial.println("MQTT Connected!");
}

void sendData()
{
    if (!mqtt_client->connected())
    {
        return;
    }
    // save the last time data was sent
    previousMillis = millis();
    digitalWrite(LED_RED, LED_ON);
    char payload[255];
    sprintf(
        payload,
        "{\"voltage\":%.1f,\"current\":%.3f,\"power\":%.1f,\"energy\":%.3f,\"frequency\":%.1f,\"pf\":%.2f}",
        pzem.voltage(),
        pzem.current(),
        pzem.power(),
        pzem.energy(),
        pzem.frequency(),
        pzem.pf());
    mqtt_powerData->publish(payload);
    webSocket.broadcastTXT(payload);
    digitalWrite(LED_RED, LED_OFF);
}

unsigned long longStart;
void longPressStart()
{
    longStart = millis();
}

void resetCounter()
{
    pzem.resetEnergy();
    sendData();
    for (int i = 0; i < 10; ++i)
    {
        digitalWrite(LED_GREEN, LED_ON);
        delay(100);
        digitalWrite(LED_GREEN, LED_OFF);
        delay(100);
    }
}

void resetWiFi()
{
    // erase WiFi settings
    //WiFi.disconnect(false);
    for (int i = 0; i < 20; ++i)
    {
        digitalWrite(LED_RED, LED_ON);
        delay(100);
        digitalWrite(LED_RED, LED_OFF);
        delay(100);
    }

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_hostname("hostname", "hostname", mqtt.hostname, 40);
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt.server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt.port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt.user, 40);
    WiFiManagerParameter custom_mqtt_pass("pass", "mqtt password", mqtt.pass, 40);

    wifiManager.addParameter(&custom_mqtt_hostname);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);

    //reset settings - for testing
    //wifiManager.resetSettings();

    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    //wifiManager.setAPCallback(configModeCallback);

    digitalWrite(LED_RED, LED_ON);
    wifiManager.setDebugOutput(false);
    if (!wifiManager.startConfigPortal("PowerMonitor")) {
      delay(1000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    // Connect to WiFi access point.
    //if you get here you have connected to the WiFi
    //read updated parameters
    strcpy(mqtt.hostname, custom_mqtt_hostname.getValue());
    strcpy(mqtt.server, custom_mqtt_server.getValue());
    strcpy(mqtt.port, custom_mqtt_port.getValue());
    strcpy(mqtt.user, custom_mqtt_user.getValue());
    strcpy(mqtt.pass, custom_mqtt_pass.getValue());
    EEPROM.put(0, mqtt);
    EEPROM.commit();

    ESP.reset();
    delay(1000);
}

void longPressStop()
{
    unsigned long duration = millis() - longStart;
    if (duration > 5000) {
        resetWiFi();
    } else {
        resetCounter();
    }
}

void setup()
{
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, LED_OFF);
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LED_OFF);
    pinMode(LED_BLUE, OUTPUT);
    digitalWrite(LED_BLUE, LED_OFF);
    pinMode(BTN_FLASH, INPUT);

    digitalWrite(LED_RED, LED_ON);
    delay(1000);
    digitalWrite(LED_RED, LED_OFF);
    digitalWrite(LED_GREEN, LED_ON);
    delay(1000);
    digitalWrite(LED_GREEN, LED_OFF);

    EEPROM.begin(sizeof(mqtt_config));
    if(EEPROM.read(0) == 'O' && EEPROM.read(1) == 'K') {
        EEPROM.get(0, mqtt);
    } else {
        resetWiFi();
    }

    if (WiFi.SSID() != "") {
        // have saved WiFi settings
        WiFi.begin();
        WiFi.setAutoReconnect(true);
        unsigned long start = millis();
        boolean keepConnecting = true;
        uint8_t status;
        while (keepConnecting) {
            status = WiFi.status();
            if (millis() > start + 5000) {
                keepConnecting = false;
            }
            if (status == WL_CONNECTED) {
                keepConnecting = false;
            }
            digitalWrite(LED_RED, LED_ON);
            delay(100);
            digitalWrite(LED_RED, LED_OFF);
            delay(100);
        }
        if (status != WL_CONNECTED) {
            // reboot
            delay(2000);
            ESP.reset();
            delay(1000);
        }
    } else {
        // no saved settings, start config portal
        resetWiFi();
    }


    digitalWrite(LED_RED, LED_OFF);

    for (int i = 0; i < 5; ++i)
    {
        digitalWrite(LED_GREEN, LED_ON);
        delay(100);
        digitalWrite(LED_GREEN, LED_OFF);
        delay(100);
    }

    ArduinoOTA.setHostname(mqtt.hostname);
    ArduinoOTA.begin();

    blinker_green.setDelay(1000, 1000);
    blinker_green.start();

    button.attachClick(sendData);
    button.attachLongPressStart(longPressStart);
    button.attachLongPressStop(longPressStop);

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    SPIFFS.begin();
    server.on("/", handleRoot);
    server.on("/index.html", handleRoot);
    server.onNotFound([]()
    {
        if (!handleFileRead(server.uri()))
            server.send(404, "text/plain", "FileNotFound");
    });
    server.begin();

    mqtt_client = new Adafruit_MQTT_Client(&client, mqtt.server, atoi(mqtt.port), mqtt.user, mqtt.pass);
    getDataTopic = String(mqtt.hostname)+"/getData";
    getStatusTopic = String(mqtt.hostname)+"/getStatus";
    mqtt_powerData = new Adafruit_MQTT_Publish(mqtt_client, getDataTopic.c_str());
    mqtt_status = new Adafruit_MQTT_Publish(mqtt_client, getStatusTopic.c_str());


    mqtt_client->will(getStatusTopic.c_str(), "offline");
    MQTT_connect();
    mqtt_status->publish("online");

    MDNS.begin(mqtt.hostname);
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
}

void loop()
{
    ArduinoOTA.handle();
    MQTT_connect();
    server.handleClient();
    webSocket.loop();
    MDNS.update();

    if (millis() - previousMillis >= sendInterval)
    {
        sendData();
    }
    blinker_red.blink();
    blinker_green.blink();
    button.tick();
    delay(1);
}
