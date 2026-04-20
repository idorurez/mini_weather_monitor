// Returns true if associated, false otherwise. Caller decides what to render.
bool wifiConnect() {
  const unsigned long perAttemptMs = 10000;
  const int maxRetries = 3;

  bootStatus("WiFi: enter");
  if (WiFi.status() == WL_CONNECTED) {
    bootStatus("WiFi: already up");
    return true;
  }

  bootStatus("WiFi: setHostname");
  WiFi.setHostname(config.hostname);
  bootStatus("WiFi: onEvent+mode");
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  bootStatus("WiFi: begin");
  WiFi.begin(config.ssid, config.pass);
  Serial.printf("Connecting to %s\n", config.ssid);
  bootStatus("WiFi: connecting...");

  unsigned long lastAttempt = millis();
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    if ((millis() - lastAttempt) > perAttemptMs) {
      retries++;
      char buf[32];
      snprintf(buf, sizeof(buf), "WiFi: retry %d/%d", retries, maxRetries);
      bootStatus(buf);
      WiFi.disconnect();
      WiFi.begin(config.ssid, config.pass);
      lastAttempt = millis();
    }
    esp_task_wdt_reset();   // keep the WDT happy across the long associate wait
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.printf("WiFi failed after %d retries\n", maxRetries);
  return false;
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("WiFi: associated");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi: disconnected");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.printf("WiFi: got IP %s\n", WiFi.localIP().toString().c_str());
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      Serial.println("WiFi: lost IP");
      break;
    default:
      break;
  }
}
