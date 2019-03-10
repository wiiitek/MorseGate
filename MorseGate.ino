#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Thread.h>
#include <Morse.h>
#include <Regexp.h>
#include <FS.h>

const int STATUS_READY_PIN = D7;
const int STATUS_BUSY_PIN = D6;
const int MSG_PIN = D5;
const float MY_DEFAULT_WPM = 14.13;
const float MIN_WPM = 2;
const float MAX_WPM = 50;
const byte DNS_PORT = 53;
const String AP_TITLE = "Morse Gate";
const String MSG_QUERY_STRING_KEY = "msg";
const String WPM_QUERY_STRING_KEY = "wpm";
// specifies allowed characters (letters, digits and spaces)
const char* ALLOWED_MSG_REGEXP = "^[%a%d ]*$";
const int ALLOWED_MSG_LENGTH = 100;

IPAddress apIP(192, 168, 96, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

Thread timerThread = Thread();
Thread statusBusyThread = Thread();
Morse morse(MSG_PIN, MY_DEFAULT_WPM);

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

void sendMessage(char* message) {
  if (morse.busy) {
    return webServer.send(500, "text/plain", "MorseGate is currently sending a message.");
  } else {
    float wpmf;
    if (webServer.hasArg(WPM_QUERY_STRING_KEY)) {
      String wpmInput = webServer.arg(WPM_QUERY_STRING_KEY);
      wpmInput.trim();
      wpmf = wpmInput.toFloat();
      Serial.printf("Converted wpm to: %f\n", wpmf);
    } else {
      wpmf = MY_DEFAULT_WPM;
    }
    if (wpmf < MIN_WPM || wpmf > MAX_WPM) {
      char msgBuffer[70];
      sprintf(msgBuffer, "Word per minute paramater must be between %3f and %3f\n", MIN_WPM, MAX_WPM);
      Serial.printf(msgBuffer);
      return webServer.send(400, "text/plain", msgBuffer);
    } else {
      morse.setWPM(wpmf);
      Serial.printf("Converting '%s' to beeps...\n", message);
      digitalWrite(STATUS_BUSY_PIN, HIGH);
      morse.send(message);
      return webServer.send(200, "text/plain", "Message sent.");
    }
  }
}

void handleSendMessage() {
  if (webServer.args() == 0 || !webServer.hasArg(MSG_QUERY_STRING_KEY)) {
    return webServer.send(400, "text/plain", "Bad request: '" + MSG_QUERY_STRING_KEY + "' query parameter is required!");
  } else {
    String argumentValue = webServer.arg(MSG_QUERY_STRING_KEY);
    if (argumentValue.length() > ALLOWED_MSG_LENGTH) {
      char msgBuffer[60];
      sprintf(msgBuffer, "Message is too long: %d characters > %d (max allowed)\n", argumentValue.length(), ALLOWED_MSG_LENGTH);
      Serial.printf(msgBuffer);
      return webServer.send(400, "text/plain", msgBuffer);
    } else if (argumentValue.length() == 0) {
      Serial.println("Blank message - ignoring");
      return webServer.send(200, "text/plain", "Blank message. Ignoring.");
    } else {

      char message[argumentValue.length()];
      argumentValue.toCharArray(message, argumentValue.length() + 1);
      argumentValue.trim();

      MatchState ms;
      ms.Target(message);
      unsigned int count = ms.MatchCount(ALLOWED_MSG_REGEXP);
      if (count < 1) {
        return webServer.send(400, "text/plain", "Only letters, numbers and spaces are allowed as message characters.");
      } else {
        sendMessage(message);
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
    Serial.println("SPIFFS initialized");
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
        File file = dir.openFile("r");
        Serial.printf("%7d bytes: ", file.size());
        Serial.println(dir.fileName());
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
      String userAgent = webServer.header("User-Agent");
      String referer = webServer.header("Referer");
      Serial.println("Redirecting to home for User-Agent:\n\t" + userAgent + "\nand Referer:\n\t" + referer);
      boolean showHomePage = processRequest("/");
      if (!showHomePage) {
        webServer.send(404, "text/plain", "404: Page Not Found");
      }
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
