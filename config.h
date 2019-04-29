// You can add as many remote control emulators as you want by adding elements to the "remotes" vector
// The id and mqtt_topic can have any value but must be unique
// default_rolling_code can be any unsigned int, usually leave it at 1
// eeprom_address must be incremented by 4 for each remote

// Once the programe is uploaded on the ESP32:
// - Long-press the program button of YOUR ACTUAL REMOTE until your blind goes up and down slightly
// - send 'p' using MQTT on the corresponding topic
// - You can use the same remote control emulator for multiple blinds, just repeat these steps.
//
// Then:
// - u will make it go up
// - s make it stop
// - d will make it go down


//                                 id            mqtt_topic     default_rolling_code     eeprom_address
std::vector<REMOTE> const remotes = {{0x184623, "home/nodemcu/somfy/[DEVICE ID YOU USE]", 1, 0}
                                    };

// Change reset_rolling_codes to true to clear the rolling codes stored in the non-volatile storage
// The default_rolling_code will be used

const bool reset_rolling_codes = false;

const char*        wifi_ssid = "[SSID]";
const char*    wifi_password = "[SSID Password]";

const char*      mqtt_server = "[MQTT HOST Server]";
const unsigned int mqtt_port = 1883;
const char*        mqtt_user = "username";
const char*    mqtt_password = "secretPassword5678";
const char*          mqtt_id = "esp8266-somfy-remote";

const char*     status_topic = "home/nodemcu/somfy/[DEVICE ID YOU USE]/status"; // Online / offline
const char*     state_topic = "home/nodemcu/somfy/[DEVICE ID YOU USE]/state"; 
const char*        ack_topic = "home/nodemcu/somfy/[DEVICE ID YOU USE]/ack"; // Commands ack "id: 0x184623, cmd: u"

#define PORT_TX 5 // Output data on pin 23 (can range from 0 to 31). Check pin numbering on ESP8266.
