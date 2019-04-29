bool to_bool(String const& s) { // thanks Chris Jester-Young from stackoverflow
     return s != "0";
}

void setup_wifi() {
  delay(10000);
  // We start by connecting to a WiFi network
  Serial.println("");
  Serial.println("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.disconnect();
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  #ifdef ESP32
     WiFi.setHostname("ESP32-somfy");
  #elif ESP8266
     WiFi.hostname("ESP8266-somfy");
  #endif
  
  // TODO: measure power consumption to see if sleep type is useful + if frequency set in platformio.ini is useful
  #ifdef ESP32
      // TODO
  #elif ESP8266
      //wifi_set_sleep_type(LIGHT_SLEEP_T);
  #endif
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
