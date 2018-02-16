#include <SoftwareSerial.h>
#include <Adafruit_FONA.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_MQTT_FONA.h>

static const char kCodeCR = '\r';
static const char kCodeLF = '\n';

#define APN "soracom.io"
#define USERNAME "sora"
#define PASSWORD "sora"

#define MILKCOCOA_APP_ID "maxj066rj51"
#define MILKCOCOA_DATASTORE "testNotif"
#define MILKCOCOA_SERVERPORT  1883

#define LED 13
#define FONA_RX 5
#define FONA_TX 4
#define FONA_RST 2

// milkcocoa Setting
const char MQTT_SERVER[] = "beam.soracom.io";
const char MQTT_CLIENTID[] = __TIME__ MILKCOCOA_APP_ID;

// Serial Setting
SoftwareSerial fonaSS = SoftwareSerial(FONA_RX, FONA_TX);
SoftwareSerial *fonaSerial = &fonaSS;

// 3G Setting
Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

// mqtt Setting
Adafruit_MQTT_FONA mqtt(&fona, MQTT_SERVER, MILKCOCOA_SERVERPORT, MQTT_CLIENTID, "sora", "sora");
char gpsData[64] = "\0";
char imsi[16];

boolean isNetOpen = false;

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);

  delay(50);
  Serial.println();
  Serial.println("!start!");
  fonaSerial->begin(115200);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  uint8_t len = fona.getSIMIMSI(imsi);
  if (len <= 0) {
    Serial.println("failed to read sim");
    while(1);
  }
  Serial.println(F("FONA is OK"));
  // net open
  delay(500);
  fona.setGPRSNetworkSettings(F(APN), F(USERNAME), F(PASSWORD));
  while(!isNetOpen) {
    if (!fona.enableGPRS(true)) {
      Serial.println(F("Failed to enable 3G"));
      delay(100);
    } else {
      isNetOpen = true;
    }
  }
  fona.enableGPS(true);
  // テスト用LED点灯
  digitalWrite(LED, HIGH);
}

unsigned long current = 0;
boolean isOn = false;
boolean isSend = false;

void loop()
{
  // LED　チェック
  if (millis() - current > 5000) {
    current = millis();
    float lat = 0.0f;
    float lon = 0.0f;
    if (fona.getGPS(&lat, &lon) > 0) {
      sendGPSData(lat, lon);
      if (isSend) {
        Serial.println("shutdown...");
        delay(1000);
        fona.shutdown(true);
      }
    } else {
      Serial.println("GPS GET Failed");
    }
  }
  if (!isSend) {
    if (!MQTT_connect(5)) {
      return;
    }
    push("/test/mqtt", "From SIM5320");
    isSend = true;
  }
}

/**
 * GPS　データをpushする
 **/
void sendGPSData(float& lat, float& lon) {
  char latStr[16] = "\0";
  char lonStr[16] = "\0";
  char temp[16];
  char* p;
  dtostrf(lat, 15, 6, temp);
  p = strtok(temp, " ");
  strncpy(latStr, p, strlen(p));
  dtostrf(lon, 15, 6, temp);
  p = strtok(temp, " ");
  strncpy(lonStr, p, strlen(p));
  
  sprintf(gpsData, "%s,%s", latStr, lonStr);
  char topic[64] = "/test/mqtt/gps/";
  strcat(topic, imsi);
  push(topic, gpsData);
}

boolean MQTT_connect(uint8_t tryCount) {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return true;
  }

  Serial.print("Connecting to MQTT... ");

  while (tryCount > 0) { // connect will return 0 for connected
    if (mqtt.connect() == 0) {
      Serial.println("MQTT Connected!");
      return true;
    }
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    --tryCount;
  }
  return false;
}

boolean push(const char *topic, const char* message) {
  bool ret;
  Adafruit_MQTT_Publish pushPublisher = Adafruit_MQTT_Publish(&mqtt, topic);
  ret = pushPublisher.publish(message);
  return ret;
}