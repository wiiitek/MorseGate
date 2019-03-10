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
const String AP_TITLE = "Morse Gate";
const String MSG_QUERY_STRING_KEY = "msg";

IPAddress apIP(192, 168, 96, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

Thread timerThread = Thread();
Thread statusBusyThread = Thread();
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

void updateStatusTask() {
  if (morse.busy) {
    digitalWrite(STATUS_BUSY_PIN, HIGH);
    digitalWrite(STATUS_READY_PIN, LOW);
  } else {
    digitalWrite(STATUS_BUSY_PIN, LOW);
    digitalWrite(STATUS_READY_PIN, HIGH);
  }
}

boolean sendMessage(String msg) {
  if (morse.busy) {
    return false;
  } else {
    Serial.println("Converting '" + msg + "' to beeps...");
    digitalWrite(STATUS_BUSY_PIN, HIGH);
    char message[msg.length()];
    msg.toCharArray(message, msg.length() + 1);
    Serial.printf("Sending message: '%s'\n", message);
    morse.send(message);
    return true;
  }
}

void handleSendMessage() {
  if (webServer.args() == 0 || !webServer.hasArg(MSG_QUERY_STRING_KEY)) {
    return webServer.send(400, "text/plain", "Bad request: '" + MSG_QUERY_STRING_KEY + "' query parameter is required!");
  } else {
    String argumentValue = webServer.arg(MSG_QUERY_STRING_KEY);
    Serial.println("Query string value for msg: '" + argumentValue + "'");
    argumentValue.trim();
    if (argumentValue.length() == 0) {
      Serial.println("Blank message - ignoring");
      return webServer.send(200, "text/plain", "Blank message. Ignoring.");
    } else {
      if (sendMessage(argumentValue)) {
        return webServer.send(200, "text/plain", "Message sent: '" + argumentValue + "'.");
      } else {
        return webServer.send(500, "text/plain", "Error: MorseGate is currently sending a message.");
      }
    }
  }
}

void handleGetStatus() {
  if (morse.busy) {
    return webServer.send(200, "text/plain", "BUSY");
  } else {
    return webServer.send(200, "text/plain", "READY");
  }

}

void setup() {
  Serial.begin(9600);
  Serial.print("\n\n");

  pinMode(STATUS_READY_PIN, OUTPUT);
  pinMode(STATUS_BUSY_PIN, OUTPUT);
  timerThread.onRun(updateMorseTask);
  timerThread.setInterval(1);
  statusBusyThread.onRun(updateStatusTask);
  statusBusyThread.setInterval(1);
  Serial.println("Threads initialized");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_TITLE);

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
        Serial.printf("\n\t(%6d bytes)\n", file.size());
        file.close();
      }
      Serial.print("\n");
    } else {
      Serial.println("Errors while getting SPIFFS info");
    }
  } else {
    Serial.println("Errors while initializing SPIFFS");
  }

  // replies for all domains
  if (dnsServer.start(DNS_PORT, "*", apIP)) {
    Serial.println("DNS server started");
  } else {
    Serial.println("Errors while starting DNS server");
  }

  webServer.on("/msg/send", HTTP_GET, handleSendMessage);
  webServer.on("/msg/status", HTTP_GET, handleGetStatus);
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
  dnsServer.processNextRequest();
  webServer.handleClient();
}
