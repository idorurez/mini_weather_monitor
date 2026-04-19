// Returns true if associated, false otherwise. Caller decides what to render.
bool wifiConnect() {
  const unsigned long perAttemptMs = 20000;
  const int maxRetries = 5;

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.setHostname(config.hostname);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.pass);
  Serial.printf("Connecting to %s\n", config.ssid);

  unsigned long lastAttempt = millis();
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    if ((millis() - lastAttempt) > perAttemptMs) {
      retries++;
      Serial.printf("Retry %d/%d\n", retries, maxRetries);
      WiFi.disconnect();
      WiFi.begin(config.ssid, config.pass);
      lastAttempt = millis();
    }
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
