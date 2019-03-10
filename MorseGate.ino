#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Thread.h>
#include <Morse.h>
#include <FS.h>

const int STATUS_READY_PIN = D7;
const int STATUS_BUSY_PIN = D6;
const int MSG_PIN = D5;
const float WPM = 14.13;
const byte DNS_PORT = 53;
const String apTitle = "Morse Gate";

IPAddress apIP(192, 168, 96, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

Thread timerThread = Thread();
Thread statusBusyThread = Thread();
Thread msgThread = Thread();
Morse morse(MSG_PIN, WPM);

String contentType(String filename) {
  if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".svg")) {
    return "image/svg+xml";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else {
    return "text/plain";
  }
}

bool processRequest(String path) {
  Serial.println("Requested path: " + path);
  boolean isFolder = path.endsWith("/");
  if (isFolder) {
    path = path + "index.html";
  };
  if (SPIFFS.exists(path)) {
    Serial.println("Serving file from SPIFFS!");
    String mime = contentType(path);
    File file = SPIFFS.open(path, "r");
    size_t sent = webServer.streamFile(file, mime);
    file.close();
    return true;
  }
  Serial.println("File not found, returning false");
  return false;
}

void updateMorseTask() {
  morse.update();
}

void statusBusyTask() {
  if (morse.busy) {
    digitalWrite(STATUS_BUSY_PIN, HIGH);
    digitalWrite(STATUS_READY_PIN, LOW);
  } else {
    digitalWrite(STATUS_BUSY_PIN, LOW);
    digitalWrite(STATUS_READY_PIN, HIGH);
  }
}

void msgTask() {
  
  if (!morse.busy) {
    digitalWrite(STATUS_BUSY_PIN, HIGH);
    //morse.send("SMS000000 SMS111111 SMS222222 SMS333333 SMS444444 SMS555555 SMS666666 SMS777777 SMS888888 SMS999999 0123");
    //morse.send("SMS000000 SMS111111 SMS222222 SMS333333 SMS444444 SMS555555 SMS666666 SMS777777 SMS888888 SMS999999 ");
    morse.send("SMS 0123456789");
    Serial.println("Morse message sent.");
  } else {
    Serial.println("Morse system was busy, message not sent.");  
  }
}

void setup() {
  Serial.begin(9600);
  Serial.print("\n\n");

  pinMode(STATUS_READY_PIN, OUTPUT);
  pinMode(STATUS_BUSY_PIN, OUTPUT);
  timerThread.onRun(updateMorseTask);
  timerThread.setInterval(1);
  statusBusyThread.onRun(statusBusyTask);
  statusBusyThread.setInterval(10);
  msgThread.onRun(msgTask);
  msgThread.setInterval(30000);
  Serial.println("Threads initialized");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apTitle);

  if (SPIFFS.begin()) {
    Serial.println("SPIFFS initialised");
    FSInfo fs_info;
    boolean infoOk = SPIFFS.info(fs_info);
    if (infoOk) {
      Serial.printf("total kbytes:    %6d\n", fs_info.totalBytes / 1024);
      Serial.printf("used kbytes:     %6d\n", fs_info.usedBytes / 1024);
      Serial.printf("max open files:  %6d\n", fs_info.maxOpenFiles);
      Serial.printf("max path length: %6d\n", fs_info.maxPathLength);
      Serial.print("\n");
      Serial.println("Content of root folder:");
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        Serial.print(dir.fileName());
        File file = dir.openFile("r");
        Serial.printf(" (%6d bytes)\n", file.size());
        file.close();
      }
    } else {
      Serial.println("Errors while getting SPIFFS info");
    }
  } else {
    Serial.println("Errors while initialising SPIFFS");
  }

  // replies for all domains
  if (dnsServer.start(DNS_PORT, "*", apIP)) {
    Serial.println("DNS server started");
  } else {
    Serial.println("Errors while starting DNS server");
  }

  // TODO: add support for POST request with message to encode
  webServer.onNotFound([]() {
    String path = webServer.uri();
    boolean ok = processRequest(path);
    if (!ok) {
      webServer.send(404, "text/plain", "404: Page Not Found");
    }
  });

  webServer.begin();
  Serial.println("HTTP server started");
}

void loop() {
  if (timerThread.shouldRun()) {
    timerThread.run();
  }
  if (statusBusyThread.shouldRun()) {
    statusBusyThread.run();
  }
  if (msgThread.shouldRun()) {
    msgThread.run();
  }
  dnsServer.processNextRequest();
  webServer.handleClient();
}
