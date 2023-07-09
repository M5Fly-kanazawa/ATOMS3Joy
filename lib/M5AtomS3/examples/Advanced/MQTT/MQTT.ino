/*
*******************************************************************************
* Copyright (c) 2021 by M5Stack
*                  Equipped with M5AtomS3 sample source code
*                          配套  M5AtomS3 示例源代码
* Visit for more information: https://docs.m5stack.com/en/core/AtomS3
* 获取更多资料请访问: https://docs.m5stack.com/zh_CN/core/AtomS3
*
* Describe: MQTT.
* Date: 2022/12/19
*******************************************************************************
*/
#include "M5AtomS3.h"
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

// Configure the name and password of the connected wifi and your MQTT Serve
// host.  配置所连接wifi的名称、密码以及你MQTT服务器域名
const char* ssid        = "FS309";
const char* password    = "FS808808";
const char* mqtt_server = "mqtt.m5stack.com";

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setupWifi();
void callback(char* topic, byte* payload, unsigned int length);
void reConnect();

void setup() {
    M5.begin(true, true, true, false);
    setupWifi();
    client.setServer(mqtt_server,
                     1883);  // Sets the server details.  配置所连接的服务器
    client.setCallback(
        callback);  // Sets the message callback function.  设置消息回调函数
}

void loop() {
    if (!client.connected()) {
        reConnect();
    }
    client.loop();  // This function is called periodically to allow clients to
                    // process incoming messages and maintain connections to the
                    // server.
    //定期调用此函数，以允许主机处理传入消息并保持与服务器的连接

    unsigned long now =
        millis();  // Obtain the host startup duration.  获取主机开机时长
    if (now - lastMsg > 2000) {
        lastMsg = now;
        ++value;
        snprintf(msg, MSG_BUFFER_SIZE, "hello world #%ld",
                 value);  // Format to the specified string and store it in MSG.
                          // 格式化成指定字符串并存入msg中
        USBSerial.print("Publish message: ");
        USBSerial.println(msg);
        client.publish("M5Stack", msg);  // Publishes a message to the specified
                                         // topic.  发送一条消息至指定话题
    }
}

void setupWifi() {
    delay(10);
    M5.Lcd.print("Connecting to Network...");
    USBSerial.printf("Connecting to %s", ssid);
    WiFi.mode(
        WIFI_STA);  // Set the mode to WiFi station mode.  设置模式为WIFI站模式
    WiFi.begin(ssid, password);  // Start Wifi connection.  开始wifi连接

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        USBSerial.print(".");
    }
    USBSerial.printf("\nSuccess\n");
    M5.Lcd.println("Success");
    M5.Lcd.println("For communication information see serial port");
}

void callback(char* topic, byte* payload, unsigned int length) {
    USBSerial.print("Message arrived [");
    USBSerial.print(topic);
    USBSerial.print("] ");
    for (int i = 0; i < length; i++) {
        USBSerial.print((char)payload[i]);
    }
    USBSerial.println();
}

void reConnect() {
    while (!client.connected()) {
        USBSerial.print("Attempting MQTT connection...");
        // Create a random client ID.  创建一个随机的客户端ID
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.  尝试重新连接
        if (client.connect(clientId.c_str())) {
            USBSerial.println("connected");
            // Once connected, publish an announcement to the topic.
            // 一旦连接，发送一条消息至指定话题
            client.publish("M5Stack", "hello world");
            // ... and resubscribe.  重新订阅话题
            client.subscribe("M5Stack");
        } else {
            USBSerial.print("failed, rc=");
            USBSerial.print(client.state());
            USBSerial.println("try again in 5 seconds");
            delay(5000);
        }
    }
}
