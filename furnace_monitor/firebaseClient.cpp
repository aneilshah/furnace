#include "FirebaseClient.h"

FirebaseClient::FirebaseClient() {}

bool FirebaseClient::begin(const char* apiKey, const char* dbUrl) {
  lastErr = "";

  if (!initialized) {
    fbdo.setResponseSize(2048);
    initialized = true;
  }

  config = FirebaseConfig();
  auth = FirebaseAuth();

  config.api_key = apiKey;
  config.database_url = dbUrl;

  config.timeout.serverResponse = 10 * 1000;
  config.timeout.wifiReconnect  = 10 * 1000;

  if (!Firebase.signUp(&config, &auth, "", "")) {
    // message is MB_String in Firebase_ESP_Client
    lastErr = config.signer.signupError.message.c_str();
    return false;
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  const unsigned long t0 = millis();
  while (!Firebase.ready() && millis() - t0 < 2000) delay(50);

  if (!Firebase.ready()) {
    // errorReason() returns Arduino String in most builds; if not, .c_str() still works
    lastErr = fbdo.errorReason();
    if (lastErr.length() == 0) lastErr = "Firebase not ready";
  }

  return Firebase.ready();
}

bool FirebaseClient::ready() const { return Firebase.ready(); }
String FirebaseClient::lastError() const { return lastErr; }

// Build "<base><field>" into a reusable String buffer
const String& FirebaseClient::mkKey(const String& base, const char* field) {
  const size_t need = base.length() + strlen(field);
  _keyBuf.reserve(need + 8);   // Heltec core: don't touch capacity(); reserve is fine

  _keyBuf = base;
  _keyBuf += field;
  return _keyBuf;
}

bool FirebaseClient::writeInt(const String& path, int v) {
  const bool ok = Firebase.RTDB.setInt(&fbdo, path, v);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

bool FirebaseClient::writeFloat(const String& path, float v) {
  const bool ok = Firebase.RTDB.setFloat(&fbdo, path, v);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

bool FirebaseClient::writeString(const String& path, const String& v) {
  const bool ok = Firebase.RTDB.setString(&fbdo, path, v);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

// New overloads: base + field
bool FirebaseClient::writeInt(const String& base, const char* field, int v) {
  return writeInt(mkKey(base, field), v);
}

bool FirebaseClient::writeFloat(const String& base, const char* field, float v) {
  return writeFloat(mkKey(base, field), v);
}

bool FirebaseClient::writeString(const String& base, const char* field, const String& v) {
  return writeString(mkKey(base, field), v);
}

bool FirebaseClient::writeJSON(const String& path, FirebaseJson& json) {
  String out;
  json.toString(out, true);
  Serial.printf("JSON bytes: %u\n", (unsigned)out.length());

  const bool ok = Firebase.RTDB.updateNode(&fbdo, path, &json);
  if (!ok) lastErr = fbdo.errorReason();
  return ok;
}

// Optional convenience exposure
const String& FirebaseClient::mkKeyPublic(const String& base, const char* field) {
  return mkKey(base, field);
}
