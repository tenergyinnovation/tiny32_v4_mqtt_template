/***********************************************************************
 * Project      :     tiny32_v4_mqtt_template
 * Description  :     Template for tiny32_v3 - tiny32_v4 connect to
 *                     mqtt server
 * Hardware     :     tiny32_v4
 * Author       :     Tenergy Innovation Co., Ltd.
 * Date         :     26/04/2024
 * Revision     :     1.0
 * Rev0.1       :     Origital
 * website      :     http://www.tenergyinnovation.co.th
 * Email        :     uten.boonliam@tenergyinnovation.co.th
 * TEL          :     +66 89-140-7205
 ***********************************************************************/
#include <Arduino.h>
#include <tiny32_v3.h>
#include <esp_task_wdt.h>
#include <stdio.h>
#include <stdint.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <EEPROM.h>

/**************************************/
/*          Firmware Version          */
/**************************************/
#define FIRMWARE_VERSION "1.0"

/**************************************/
/*          Header project            */
/**************************************/
void header_print(void)
{
    Serial.printf("\r\n***********************************************************************\r\n");
    Serial.printf("* Project      :     tiny32_v4_mqtt_template\r\n");
    Serial.printf("* Description  :     Template for tiny32_v3 - tiny32_v4 connect to mqtt server \r\n");
    Serial.printf("* Hardware     :     tiny32_v4\r\n");
    Serial.printf("* Author       :     Tenergy Innovation Co., Ltd.\r\n");
    Serial.printf("* Date         :     18/06/2024\r\n");
    Serial.printf("* Revision     :     %s\r\n", FIRMWARE_VERSION);
    Serial.printf("* website      :     http://www.tenergyinnovation.co.th\r\n");
    Serial.printf("* Email        :     uten.boonliam@tenergyinnovation.co.th\r\n");
    Serial.printf("* TEL          :     +66 89-140-7205\r\n");
    Serial.printf("***********************************************************************/\r\n");
}

/**************************************/
/*       MQTT COMMAND FROM USER       */
/**************************************/
#define RESET_CMD "reset"

/**************************************/
/*        define object variable      */
/**************************************/
tiny32_v3 mcu;
WiFiManager wm;
WiFiClient espClient;
PubSubClient client(espClient);
File file;
tm timeinfo;

/**************************************/
/*            GPIO define             */
/**************************************/

/**************************************/
/*       Constand define value        */
/**************************************/
#define WDT_TIMEOUT 120
#define WIFI_TIMEOUT 20
#define FORMAT_SPIFFS_IF_FAILED true
#define AP_TIMEOUT 300
const bool SUCCESS = 1;
const bool ERROR = 0;

// sync date-time
const char *ntpServer = "asia.pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;
uint16_t timecurrent = 0; // เวลาปัจจุบัน (timestamp)

/**************************************/
/*       eeprom address define        */
/**************************************/
#define EEPROM_SIZE 1024 // กำหนดขนาดของ eeprom *สำคัญมาก

/**************************************/
/*        define global variable      */
/**************************************/
String Json_string = "";
char unit[20];
char macid[50];
bool shouldSaveConfig = false;
long lastMsg = 0;
char msg[50];
char string[40];
uint8_t wifistatus;
int WiFi_rssi;
char mqtt_Msg[100];
char Json_Msg[1200];
String JsonStr = "";

/* mqtt constand value */
const char *mqtt_server_char = "[server ip address]"; // cmu-egat-cloud-server
const char *mqtt_user_char = "tiny32";
const char *mqtt_pass_char = "tiny32";
const char *mqtt_topic_char = "mqtt_topic";
char topic_public[50];
char topic_sub[50];

/* upload flag */
bool mysql_success = false; // ตัวแปรตรวจสอบการส่งข้อมูลไปยัง Server

StaticJsonDocument<50000> doc;
StaticJsonDocument<1000> doc1;
StaticJsonDocument<1000> doc2;

/**************************************/
/*           define function          */
/**************************************/
void saveConfigCallback();
bool wifi_config(bool upload_flag);
void saveConfigCallback();
void reconnect();
void eeprom_init();
void callback(char *topic, byte *payload, unsigned int length);
String json_mqttConvert(void);

/**************************************/
/*          Special funtion           */
/**************************************/
#include <tiny32_SPIFSS.h>

/***********************************************************************
 * FUNCTION:    setup
 * DESCRIPTION: setup process
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void setup()
{
    Serial.begin(115200);
    header_print();
    mcu.buzzer_beep(1);

    Serial.println("Setup: Initial eeprom ...");
    eeprom_init();

    //*** Define Unit name ***
    String _mac1 = WiFi.macAddress().c_str();
    Serial.printf("Setup: macAddress = %s\r\n", WiFi.macAddress().c_str());
    String _mac2 = _mac1.substring(9, 17);
    _mac2.replace(":", "");
    String _mac3 = "tiny32-" + _mac2;
    _mac3.toCharArray(unit, _mac3.length() + 1);
    Serial.printf("Setup: Unit ID => %s\r\n", unit);

    // Reset SPIFSS and WiFi config
    if (mcu.Sw1())
    {
        mcu.buzzer_beep(4);
        while (mcu.Sw1())
            ;
        Serial.println("Warning: Reset SPIFSS and WiFi config...");
        if (!SPIFFS.begin(true))
        {
            Serial.println("Error: An Error has occurred while mounting SPIFFS");
        }
        else
        {
            Serial.printf("Info: List file in root directory\r\n");
            listDir(SPIFFS, "/", 0);
            Serial.println();
            writeFile(SPIFFS, "/wifi.conf", "");
            writeFile(SPIFFS, "/wifi_latest.conf", "");
            wm.resetSettings();
            vTaskDelay(2000);
            ESP.restart();
        }
    }

    strcpy(macid, WiFi.macAddress().c_str());
    //*** SPIFSS inital ***
    Serial.println("Setup: SPIFSS initial ...");
    if (!SPIFFS.begin(true))
    {
        Serial.println("\tError: An Error has occurred while mounting SPIFFS");
    }
    else
    {
        Serial.printf("\tPass: List file in root directory\r\n");
        listDir(SPIFFS, "/", 0);
    }

    //*** WiFi config ***
    if (true) // ไม่เชื่อมต่อ WiFi ในฟังก์ชั่น setup
    {
        Serial.println("Setup: WiFi initial ...");
        wifi_config(true);
    }

    //*** WiFi Workwi simulation ***
    // Serial.print("Connecting to WiFi");
    // mcu.TickBuildinLED(0.1);
    // WiFi.begin("Wokwi-GUEST", "", 6);

    uint8_t _error_cnt = 0;
    while ((WiFi.status() != WL_CONNECTED) && (_error_cnt++ < 5))
    {
        delay(1000);
        Serial.print(".");
    }
    if (_error_cnt < 4)
    {

        Serial.printf("\tSuccess to connected\r\n");
        Serial.print("\tIP address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("\tSSID: %s\r\n", WiFi.SSID().c_str());
        Serial.printf("\tRSSI: %d\r\n", WiFi.RSSI());
        Serial.printf("\tMQTT Server: %s\r\n", mqtt_server_char);
        Serial.printf("\tMQTT User: %s\r\n", mqtt_user_char);
        Serial.printf("\tMQTT Pass: %s\r\n", mqtt_pass_char);
        Serial.printf("\tMQTT TOPIC: %s\r\n", mqtt_topic_char);
        mcu.TickBuildinLED(1);
    }
    else
    {
        wifi_config(true);
    }

    //*** generate topic_public ***
    String _strtmp = String(mqtt_topic_char) + "/" + String(unit);
    _strtmp.toCharArray(topic_public, _strtmp.length() + 1);
    Serial.printf("\tMQTT TOPIC-PUBLIC => %s\r\n", topic_public);

    //*** generate topic_subscribe ***
    _strtmp = String(mqtt_topic_char) + "/" + String(unit) + "/control";
    _strtmp.toCharArray(topic_sub, _strtmp.length() + 1);
    Serial.printf("\tMQTT TOPIC-SUBSCRIBE  => %s\r\n\r\n", topic_sub);

    client.setServer(mqtt_server_char, 1883);
    client.connect(topic_public, mqtt_user_char, mqtt_pass_char);
    client.setCallback(callback);
    client.subscribe(topic_sub);
    client.setBufferSize(5000);

    /* All wifi list */
    String _json_rawFile = getdataFile(SPIFFS, "/wifi.conf");
    String _json_string = "{\"wifi\": [";
    _json_string += _json_rawFile;
    _json_string += "]}";
    DeserializationError _error = deserializeJson(doc1, _json_string);

    if (_error)
    {
        Serial.printf("\tError: deserializeJson() failed: %s", _error.c_str());
    }

    else
    {
        /* แสดงค่า WiFi list ทั้งหมด */
        Serial.printf("\r\nSetup: All WiFi list =>\r\n");
        size_t _json_data_size = serializeJsonPretty(doc1, Serial);
        Serial.println("\r\n");
    }

    /* Watchdoc concig*/
    Serial.print("Setup: Configuring WDT...");
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
    esp_task_wdt_add(NULL);
    Serial.println("done");
    mcu.buzzer_beep(2);
    vTaskDelay(2);
}

/***********************************************************************
 * FUNCTION:    loop
 * DESCRIPTION: loop process
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void loop()
{
    static long _interval_sensor = 0;    // เวลาในการวนรอบอ่านเซนเซอร์
    static long _interval_mqtt = 0;      // เวลาในการวนรอบส่งค่า  mqtt server
    static long _interval_wificheck = 0; // เวลาในการวยรอบตรวจสอบการเชื่อมต่อ WiFi
    static int _error_cnt = 0;
    static int _error_mqtt_cnt = 0;
    static bool _error_flag = false;
    String _JsonStr;

    // Check mqtt connection
    if (!client.connected())
    {
        _error_mqtt_cnt++;
        Serial.printf("Error: Can't connect main mqtt server\r\n");
        vTaskDelay(1000);
        if (_error_mqtt_cnt > 60)
        {
            mcu.buzzer_beep(4);
            ESP.restart();
        }
        reconnect();
    }
    else
    {
        _error_mqtt_cnt = 0;
    }
    client.loop();

    // Check WiFi connection
    if ((millis() - _interval_wificheck) > 5000)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            _error_cnt = 0;
            if (_error_flag == true)
            {
                mcu.TickBuildinLED(1);
                mcu.TickBlueLED(1);
            }

            // Send command to mqtt server every 5 second
            {
                // Update Total AmbientIn
                _JsonStr = json_mqttConvert();
                _JsonStr.toCharArray(Json_Msg, _JsonStr.length() + 1);
                Serial.printf("Info: Json size = %d\r\n", _JsonStr.length());
                Serial.printf("%s\r\n\r\n", Json_Msg);
                client.publish(topic_public, Json_Msg); // don't update mqtt when set time = 0
                vTaskDelay(100);
            }
        }
        else
        {
            if (_error_flag == false)
            {
                mcu.TickBuildinLED(0.1);
                mcu.TickBlueLED(0.1);
            }
            _error_flag = true;
            _error_cnt++;

            if (_error_cnt > 120)
            {
                mcu.buzzer_beep(4);
                ESP.restart();
            }
        }
        _interval_wificheck = millis();
    }

    esp_task_wdt_reset();
    vTaskDelay(10);
}

/***********************************************************************
 * FUNCTION:    wifi_config
 * DESCRIPTION: WiFi config function for check and upload data to server
 * PARAMETERS:  upload_flag (TRUE/FALSE)
 * RETURNED:    nothing
 ***********************************************************************/
bool wifi_config(bool upload_flag)
{
    if (upload_flag)
    {
        Serial.println("\tInfo: Wifi config ...");
        mcu.TickBuildinLED(0.1);

        file = SPIFFS.open("/wifi.conf");
        if (!file)
        {
            Serial.println("Error: Failed to open file 'wifi.config'");
            return false;
        }
        else
        {
            /* All wifi list */
            String _json_rawFile = getdataFile(SPIFFS, "/wifi.conf");
            String _json_string = "{\"wifi\": [";
            _json_string += _json_rawFile;
            _json_string += "]}";
            DeserializationError _error = deserializeJson(doc1, _json_string);

            /* latest wifi */
            String _json_rawFile2 = getdataFile(SPIFFS, "/wifi_latest.conf");
            String _json_string2 = "{\"wifi\": [";
            _json_string2 += _json_rawFile2;
            _json_string2 += "]}";
            DeserializationError _error2 = deserializeJson(doc2, _json_string2);

            if (_error)
            {
                Serial.printf("Error: deserializeJson() failed: %s", _error.c_str());
                return false;
            }
            else if (_error2)
            {
                Serial.printf("Error: deserializeJson() failed: %s", _error2.c_str());
                return false;
            }
            else
            {
                bool _wifiConnected = false; // ตัวแปรตรวจสอบการเช่ือมต่อ WiFi

                /* แสดงค่า WiFi list ทั้งหมด */
                Serial.printf("\r\nInfo: All WiFi list =>\r\n");
                size_t _json_data_size = serializeJsonPretty(doc1, Serial);
                Serial.println("\r\n");
                Serial.printf("Info: Data Size => %d bytes\r\n", _json_data_size);
                int _json_data_number = doc1["wifi"].size();
                Serial.printf("Info: Number of WiFi list => %d\r\n\r\n", _json_data_number);

                /* แสดงค่า WiFi ล่าสุดที่เชื่อมต่อ */
                Serial.printf("\r\nInfo: Latest WiFi =>\r\n");
                size_t _json_data_size2 = serializeJsonPretty(doc2, Serial);
                int _json_data_number2 = doc2["wifi"].size();
                Serial.println("\r\n");

                /* เชื่อมต่อกับ WiFi ตัวล่าสุด */
                String _ssid_string2 = doc2["wifi"][0]["ssid"];
                String _pass_string2 = doc2["wifi"][0]["pass"];
                char _ssid_char2[30];
                char _pass_char2[30];
                _ssid_string2.toCharArray(_ssid_char2, _ssid_string2.length() + 1);
                _pass_string2.toCharArray(_pass_char2, _pass_string2.length() + 1);
                Serial.printf("Info: WiFi Connecting to.. \r\n");
                Serial.printf("ssid[latest]: %s\r\n", _ssid_char2);
                Serial.printf("password[latest]: %s\r\n", _pass_char2);
                uint8_t _timeoutCount2 = 0;
                Serial.printf("");

                // check emtry ssid and password from config file
                bool _hasNull = false;
                for (int i = 0;; ++i)
                {
                    if (_ssid_char2[i] == '\0')
                    {
                        _hasNull = true;
                        Serial.printf("Debug: found null\r\n");
                        break;
                    }
                }

                if (!_hasNull)
                {
                    WiFi.begin(_ssid_char2, _pass_char2); // ทำการเชื่อมต่อ WiFi ตัวล่าสุด
                    while ((WiFi.status() != WL_CONNECTED) && (_timeoutCount2 < WIFI_TIMEOUT))
                    {
                        delay(1000);
                        Serial.print(".");
                        _timeoutCount2++;
                    }
                }

                if ((WiFi.status() == WL_CONNECTED)) // เชื่อมต่อ WiFi ตัวล่าสุดสำเร็จ
                {
                    Serial.println("\n\rWiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());
                    Serial.printf("SSID: %s\r\n", WiFi.SSID().c_str());
                    Serial.printf("RSSI: %d\r\n", WiFi.RSSI());
                    _wifiConnected = true;
                    mcu.TickBuildinLED(1.0);
                    return true;
                }
                else // พยายามเชื่อมต่อ WiFi ที่อยู่ใน list ของไฟล์ 'wifi.conf'
                {
                    Serial.println();
                    Serial.printf("Info: WiFi Connecting to \r\n");
                    for (int i = 0; i < _json_data_number; i++)
                    {
                        String _ssid_string = doc1["wifi"][i]["ssid"];
                        String _pass_string = doc1["wifi"][i]["pass"];
                        char _ssid_char[30];
                        char _pass_char[30];
                        _ssid_string.toCharArray(_ssid_char, _ssid_string.length() + 1);
                        _pass_string.toCharArray(_pass_char, _pass_string.length() + 1);
                        Serial.printf("ssid[%d]: %s\r\n", i, _ssid_char);
                        Serial.printf("password[%d]: %s\r\n", i, _pass_char);
                        WiFi.begin(_ssid_char, _pass_char);
                        uint8_t _timeoutCount = 0;
                        Serial.printf("");
                        while ((WiFi.status() != WL_CONNECTED) && (_timeoutCount < WIFI_TIMEOUT))
                        {
                            delay(1000);
                            Serial.print(".");
                            _timeoutCount++;
                        }

                        if (WiFi.status() == WL_CONNECTED)
                        {
                            Serial.println("\n\rWiFi connected");
                            Serial.print("IP address: ");
                            Serial.println(WiFi.localIP());
                            Serial.printf("SSID: %s\r\n", WiFi.SSID().c_str());
                            Serial.printf("RSSI: %d\r\n", WiFi.RSSI());
                            _wifiConnected = true;

                            // record latest wifi to 'wifi_latest.conf'
                            String _json_OverwriteFile = "";
                            _json_OverwriteFile += "{";
                            _json_OverwriteFile += "\"ssid\":\"" + wm.getWiFiSSID() + "\",";
                            _json_OverwriteFile += "\"pass\":\"" + wm.getWiFiPass() + "\"";
                            _json_OverwriteFile += "}";
                            writeFile(SPIFFS, "/wifi_latest.conf", _json_OverwriteFile.c_str());

                            _wifiConnected = true;
                            mcu.TickBuildinLED(1.0);
                            mcu.TickBlueLED(0);
                            mcu.TickRedLED(0);
                            // break;
                            return true;
                        }
                    }

                    /* ถ้าไม่มีการเชื่อมต่อ WiFi จะเข้าสู่ AP Mode สำหรับเชื่อมต่อ และบันทึกค่า WiFi */
                    if (!_wifiConnected)
                    {
                        wm.setConfigPortalTimeout(300); // sets timeout before AP,webserver loop ends and exits even if there has been no setup.
                        mcu.TickBlueLED(0.1);
                        vTaskDelay(100);
                        mcu.TickRedLED(0.1);
                        if (wm.autoConnect(unit, "password"))
                        {
                            Serial.printf("Success to connected\r\n");
                            Serial.print("IP address: ");
                            Serial.println(WiFi.localIP());
                            Serial.printf("SSID: %s\r\n", WiFi.SSID().c_str());
                            Serial.printf("RSSI: %d\r\n", WiFi.RSSI());

                            /* record ssid and password to 'wifi.conf' */
                            String _json_appendFile = "";
                            if (_json_data_number == 0)
                            {
                                _json_appendFile += "{";
                            }
                            else
                            {
                                _json_appendFile += ",{";
                            }
                            _json_appendFile += "\"ssid\":\"" + wm.getWiFiSSID() + "\",";
                            _json_appendFile += "\"pass\":\"" + wm.getWiFiPass() + "\"";
                            _json_appendFile += "}";
                            appendFile(SPIFFS, "/wifi.conf", _json_appendFile.c_str());

                            /* record latest wifi to 'wifi_latest.conf' */
                            String _json_OverwriteFile = "";
                            _json_OverwriteFile += "{";
                            _json_OverwriteFile += "\"ssid\":\"" + wm.getWiFiSSID() + "\",";
                            _json_OverwriteFile += "\"pass\":\"" + wm.getWiFiPass() + "\"";
                            _json_OverwriteFile += "}";
                            writeFile(SPIFFS, "/wifi_latest.conf", _json_OverwriteFile.c_str());

                            mcu.TickBuildinLED(1.0);
                            mcu.TickBlueLED(0);
                            mcu.TickRedLED(0);

                            return true;
                        }
                        else
                        {
                            Serial.println("Error: Failed to connect wifi");
                            Serial.println("System is resetting");
                            mcu.buzzer_beep(3);
                            // ESP.restart();
                            return false;
                        }
                    }
                }
            }
        }
    }
    else
    {
        Serial.printf("\tInfo: No wifi config");
        return false;
    }
    return true;
}

/***********************************************************************
 * FUNCTION:    saveConfigCallback
 * DESCRIPTION: saveConfigCallback for check save config after AP mode
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

/***********************************************************************
 * FUNCTION:    callback
 * DESCRIPTION: callback function for mqtt receive message
 * PARAMETERS:  char* topic, byte* message, unsigned int length
 * RETURNED:    nothing
 ***********************************************************************/
void callback(char *topic, byte *payload, unsigned int length)
{
    char _sensor_char[length];
    char _value_char[length];
    String _sensor = "";
    String _value = "";
    int _index = 0;
    char _hour_char[4];
    char _min_char[4];
    String _hour;
    String _min;
    Serial.println("Info: Message Receive");
    Serial.print("\tmsg.topic => ");
    Serial.println(topic);

    Serial.print("\tmsg.payload =>");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
        // Serial.printf("\t\tpayload[%d] => %c\r\n", i, payload[i]);
        _value_char[i] = (char)payload[i];
    }
    Serial.println();
    Serial.printf("\tmsg.length => %d\r\n", length);

    _value = String(_value_char);

    if (_value == "success")
    {
        mysql_success = true;
        // Serial.printf("Success: Send data to server successed\r\n");
    }
    else
    {
        mysql_success = false;
        // Serial.printf("Fail: Error, can't send data to server\r\n");
    }

    // clear buffer
    for (int i = 0; i < length; i++)
    {
        _sensor_char[i] = 0;
        _value_char[i] = 0;
    }

    //*** detect type of sensor
    do
    {
        _sensor_char[_index] = (char)payload[_index];
        _index++;
    } while ((char)payload[_index] != ':' && _index < length);

    _index++;

    //*** detect value ***
    int _index2 = 0;
    do
    {
        _value_char[_index2] = (char)payload[_index];
        _index++;
        _index2++;
    } while (_index < length);

    //*** convert array char to string
    _sensor = String(_sensor_char);
    _value = String(_value_char);
    Serial.printf("\t\tcommand => %s\r\n", _sensor);
    Serial.printf("\t\tvalue => %s\r\n", _value);

    if (_sensor == RESET_CMD) // command for reset ESP module
    {
        Serial.printf("Info: cpu reset command from mqtt user..\r\n");
        mcu.buzzer_beep(4);
        ESP.restart();
    }
}

/***********************************************************************
 * FUNCTION:    reconnect
 * DESCRIPTION: reconnect function for reconnect mqtt server
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void reconnect()
{
    int _cnt_server = 0;
    wifistatus = WiFi.status();
    if (wifistatus != WL_CONNECTED)
        return;

    //*** Loop until we're reconnected ***
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        //*** Attempt to connect ***
        if (client.connect(mqtt_topic_char, mqtt_user_char, mqtt_pass_char))
        {
            client.subscribe(topic_sub);
            Serial.println("connected");
            Serial.println("-------------------------------");
            Serial.printf("Info: Sending data to MQTT server ...");
        }
        else
        {
            if (_cnt_server++ > 5)
                break;

            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

/***********************************************************************
 * FUNCTION:    eeprom_init
 * DESCRIPTION: Read EEPROM then record to variable
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void eeprom_init()
{

    if (!EEPROM.begin(EEPROM_SIZE)) // fail eeprom
    {
        Serial.println("\tError: failed to initialise EEPROM");
        Serial.println();
    }
    else
    {
        Serial.println("\tPass: Initial eeprom complete");
    }
}

/***********************************************************************
 * FUNCTION:    json_mqttConvert
 * DESCRIPTION: generate json format
 * PARAMETERS:  nothing
 * RETURNED:    json data
 ***********************************************************************/
String json_mqttConvert(void)
{

    String _JsonStr = "";
    _JsonStr += "{\"unit\":\"" + String(unit) + "\",";
    _JsonStr += "\"fw\":\"" + String(FIRMWARE_VERSION) + "\",";
    _JsonStr += "\"ssid\":\"" + WiFi.SSID() + "\",";
    _JsonStr += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    _JsonStr += "\"rssi\":" + String(WiFi.RSSI()) + "\",";
    _JsonStr += "\"topic\":\"" + String(mqtt_topic_char) + "\"";
    _JsonStr += "}";

    return _JsonStr;
}


/****** READ ME FOR MQTT command ******/
/*

-- MQTT Sub --
mosquitto_sub -h [server ip address] -u tiny32 -P tiny32 -t 'mqtt_topic/#'

-- MQTT Public
mosquitto_pub -h [server ip address]  -u tiny32 -P tiny32 -t 'mqtt_topic/[unit-name]/control' -m 'reset:1'

*/