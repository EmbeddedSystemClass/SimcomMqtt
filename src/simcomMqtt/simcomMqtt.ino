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

#define LED 14
#define FONA_RX 5
#define FONA_TX 4
#define FONA_RST 16

// milkcocoa Setting
const char MQTT_SERVER[] = MILKCOCOA_APP_ID ".mlkcca.com";
const char MQTT_CLIENTID[] = __TIME__ MILKCOCOA_APP_ID;

// Serial Setting
SoftwareSerial fonaSS = SoftwareSerial(FONA_RX, FONA_TX);
SoftwareSerial *fonaSerial = &fonaSS;

// 3G Setting
Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

// mqtt Setting
Adafruit_MQTT_FONA mqtt(&fona, MQTT_SERVER, MILKCOCOA_SERVERPORT, MQTT_CLIENTID, "sdammy", MILKCOCOA_APP_ID);

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
  uint8_t type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case FONA800L:
      Serial.println(F("FONA 800L")); break;
    case FONA800H:
      Serial.println(F("FONA 800H")); break;
    case FONA808_V1:
      Serial.println(F("FONA 808 (v1)")); break;
    case FONA808_V2:
      Serial.println(F("FONA 808 (v2)")); break;
    case FONA3G_A:
      Serial.println(F("FONA 3G (American)")); break;
    case FONA3G_E:
      Serial.println(F("FONA 3G (European)")); break;
    default: 
      Serial.println(F("???")); break;
  }
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
    float lat;
    float lon;
    if (fona.getGPS(&lat, &lon)) {
      Serial.print("GPS:(Latitude,Longitude) = (");
      Serial.print(lat, 10);
      Serial.print(", ");
      Serial.print(lon, 10);
      Serial.println(")");
      if (isSend) {
        Serial.println("shutdown...");
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
    DataElement elem = DataElement();
    elem.setValue("sim5320", 24);
    push(MILKCOCOA_DATASTORE, &elem);
    isSend = true;
    // Serial.println("shutdown...");
    // if (!fona.shutdown()) {
    //   Serial.println("shutdown failed");
    // }
  }
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

boolean push(const char *path, DataElement *pdataelement) {
  char topic[100];
  bool ret;
  char *send_array;
  sprintf(topic, "%s/%s/push", MILKCOCOA_APP_ID, path);
  Adafruit_MQTT_Publish pushPublisher = Adafruit_MQTT_Publish(&mqtt, topic);
  send_array = pdataelement->toCharArray();
  ret = pushPublisher.publish(send_array);
  free(send_array);
  return ret;
}