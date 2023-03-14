#include <pgmspace.h>

//Include your aws server settings here!
#define THINGNAME "THINGNAME"
#define AWS_IOT_PUBLISH_TOPIC "AWS_IOT_PUBLISH_TOPIC/"
#define AWS_IOT_SUBSCRIBE_TOPIC "AWS_IOT_SUBSCRIBE_TOPIC/"
#define MQTT_HOST "MQTT_HOST"

//AWS Region CA certificate
static const char cacert[] PROGMEM = R"EOF(

)EOF";

//Registered Client certificate
static const char client_cert[] PROGMEM = R"KEY(
)KEY";

//Registered Client private key
static const char privkey[] PROGMEM = R"KEY(

)KEY";
 
WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
PubSubClient client(net);


time_t now;
time_t nowish = 1678239474;
 
void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(-5 * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

bool setupAWS(){
  if(WiFi.status() != WL_CONNECTED){
    return false;
  }
  NTPConnect();
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
//  client.setKeepAlive(30);
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(receiveNudge);

  Serial.println("AWS Setup Complete.");
  return true;
}

unsigned long lastReconnectAttempt = 0;

void checkConnection() {
  if(WiFi.status() == WL_CONNECTED){
    if (!client.connected()) {
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        Serial.println("Connecting to AWS IOT");
        if (client.connect(THINGNAME)) {
          // Once connected, resubsribe
          client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC "#");
          Serial.println("AWS IoT Connected!");
        }
        if (client.connected()) {
          lastReconnectAttempt = 0;
        }else{
          Serial.println("AWS IoT Timeout!");
          Serial.println(client.state());
        }
      }
    }
  }
}
