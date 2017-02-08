/*******************************************************************************
#                                                                              #
#      LYT-SCRIPTED, an alternative firmware for LYT8266 RGBW LED lamps        #
#                                                                              #
#                                                                              #
#      Copyright (C) 2017 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <ESP8266HTTPClient.h>
#include <Ticker.h>

Ticker timer;
bool checkURL = true;

/******************************************************************************
Description.: read config file from SPIFFS
Input Value.: -
Return Value: true if config read, false in case of error
******************************************************************************/
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  
  if (!configFile || (configFile.size() > 1024)) {
    g_hostname = "LYT8266";
    g_remoteurl = "http://lyt.naaa.de";
    state = CONSTANTCOLOR;
    g_red = 255;
    g_green = 0;
    g_blue = 0;
    
    return false;
  }

  int size = configFile.size();

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    return false;
  }

  g_hostname = strdup(json["hostname"]);
  g_remoteurl = strdup(json["remoteurl"]);
  if( !strcmp(json["state"], "remotecontrol") ) {
    state = REMOTEURL;
  } else {
    state = CONSTANTCOLOR;
  }
  g_red = json["r"];
  g_green = json["g"];
  g_blue = json["b"];

  return true;
}

/******************************************************************************
Description.: Access an URL and set the passed variables according to the
              values specified at the URL
Input Value.: references to RGB variables, the variables are set if URL is read
Return Value: true if URL read, false in case of errors
******************************************************************************/
bool loadURL(String URL, uint8_t& r, uint8_t& g, uint8_t& b) {
  HTTPClient http;
  http.begin(URL.c_str());
  int httpCode = http.GET();
  String payload;
  
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Log("[HTTP] GET... code: " + String(httpCode));

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      Log("Payload: " + payload);
    }
  } else {
    Log("[HTTP] GET... failed, error: " + http.errorToString(httpCode));
  }

  http.end();
 
  DynamicJsonBuffer jsonBuffer;
  // accessing the char array is not intended use, but ArduinoJSON works with the string in place
  // so it should be fine
  JsonObject& json = jsonBuffer.parseObject(payload.c_str());

  if (!json.success()) {
    return false;
  }

  r = json["r"];
  g = json["g"];
  b = json["b"];

  return true;
}

/******************************************************************************
Description.: prepares everything, sets the light up first and then does the
              more time consuming tasks
Input Value.: -
Return Value: -
******************************************************************************/
void setup(void){
  state = BOOTUP;
  SPIFFS.begin();
  //SPIFFS.format();
  
  Serial.begin(115200);
  Serial.println("ESP Controller starting up\nComplied at: " __DATE__ " - " __TIME__);

  log_messages.resize(LOG_LENGTH, "-");
  Log("ESP Controller starting up");
  Log("Complied at: " __DATE__ " - " __TIME__);

  // read configuration file
  bool r = loadConfig();
  Log("loadConfig(): "+String(r));

  // apply hostname
  wifi_station_set_hostname((char *)g_hostname.c_str());
  
  setup_LEDs();
  setup_wifi();
  setup_webserver();

  // setup the animation as timer (Ticker)
  // create a new frame every 60 seconds
  timer.attach(60.0, [](){checkURL = true;});
}

/******************************************************************************
Description.: execute the different subtasks, none may block or not return
Input Value.: -
Return Value: -
******************************************************************************/
void loop(void){
  loop_wifi();
  loop_webserver();
  loop_LEDs();

  if(state == REMOTEURL && checkURL) {
    loadURL(g_remoteurl, g_red, g_green, g_blue);
    checkURL = false;
  }

  delay(1);
}
