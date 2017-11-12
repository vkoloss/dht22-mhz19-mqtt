#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <SoftwareSerial.h>

#define wifi_ssid "viksun"
#define wifi_password "password"

#define mqtt_server "192.168.100.7"
#define mqtt_user ""
#define mqtt_password ""

#define DHT_PIN D4
#define DHT_VERSION DHT22
#define MH_Z19_RX D7
#define MH_Z19_TX D8

String temp_hum_idx = "3";
String co2_idx = "4";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHT_VERSION);
SoftwareSerial co2Serial(MH_Z19_RX, MH_Z19_TX);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  dht.begin();
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to wi-fi");
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

int readCO2()
{
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  byte response[9]; // for answer

  co2Serial.write(cmd, 9); //request PPM CO2

  // The serial stream can get out of sync. The response starts with 0xff, try to resync.
  while (co2Serial.available() > 0 && (unsigned char)co2Serial.peek() != 0xFF) {
    co2Serial.read();
  }

  memset(response, 0, 9);
  co2Serial.readBytes(response, 9);

  Serial.println(response[0]);
    Serial.println(response[1]);
  if (response[1] != 0x86) {
    Serial.println("Invalid response from co2 sensor!");
    return -1;
  }

  byte crc = 0;
  for (int i = 1; i < 8; i++) {
    crc += response[i];
  }
  crc = 255 - crc + 1;

  if (response[8] == crc) {
    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;
    return ppm;
  } else {
    Serial.println("CRC error!");
    return -1;
  }
}

long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 1.0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    if (!isnan(temp) && !isnan(hum)) {
      Serial.println("temperature = " + String(temp));
      Serial.println("humidity = " + String(hum));
      client.publish("domoticz/in", ("{\"idx\": "+ temp_hum_idx + ", \"svalue\": \""+String(temp)+";"+String(hum)+";0\"}").c_str(), true);
    } else {
      Serial.println("DHT error!");
    }
    
    int ppm = readCO2();
    if (ppm > 400) {
      Serial.println("PPM = " + String(ppm));
      client.publish("domoticz/in", ("{\"idx\": "+ co2_idx + ", \"nvalue\": "+ppm+"}").c_str(), true);
    }
  }
}
