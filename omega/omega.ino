#include <ModbusMaster.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ================= KONFIGURASI FIREBASE =================
// Ganti dengan Project ID Firebase kamu (dari Firebase Console > Project Settings)
#define FIREBASE_PROJECT_ID "iot-monitor-3904b"   // <-- GANTI INI
const char* firestoreUrl = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/sensor_logs";

// ================= WIFI ROUTER =================
const char* ssid = "Tselhome-16A5";
const char* password = "80739392";

// ================= HOTSPOT ESP32 =================
const char* ap_ssid = "PANEL_MONITOR";
const char* ap_password = "12345678";
int ap_channel = 1;   // ubah untuk eksperimen (1 / 6 / 11)

// ================= DHT11 =================
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ================= RS485 =================
#define MAX485_DE 4
#define MAX485_RE 4
#define RXD2 16
#define TXD2 17

// ================= IR SENSOR =================
#define IR1 32
#define IR2 33
#define IR3 25
#define IR4 26

Adafruit_BME280 bme;
ModbusMaster node;

// ================= RS485 CONTROL =================
void preTransmission() {
  digitalWrite(MAX485_RE, HIGH);
  digitalWrite(MAX485_DE, HIGH);
}

void postTransmission() {
  digitalWrite(MAX485_RE, LOW);
  digitalWrite(MAX485_DE, LOW);
}

// ================= NTP / TIMESTAMP =================
void setupNTP() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+7 WIB
  Serial.print("⏰ Sinkronisasi NTP");
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (retry < 10) {
    Serial.println(" ✅ Waktu sinkron!");
  } else {
    Serial.println(" ⚠ NTP gagal.");
  }
}

String getISOTimestamp() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
  }
  return "1970-01-01T00:00:00Z"; // fallback jika NTP belum sync
}

// ================= NETWORK METRICS =================
int totalPacketsSent   = 0;
int totalPacketsFailed = 0;

struct NetMetrics {
  float delay_ms;
  float throughput_kbps;
  float packet_loss;
};

NetMetrics getNetworkMetrics(int payloadSizeBytes) {
  NetMetrics m;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5);

  unsigned long t0 = millis();
  bool ok = client.connect("firestore.googleapis.com", 443);
  unsigned long t1 = millis();

  totalPacketsSent++;

  if (ok) {
    m.delay_ms        = t1 - t0;
    m.throughput_kbps = (m.delay_ms > 0) ? (payloadSizeBytes * 8.0 / m.delay_ms) : 0;
    client.stop();
  } else {
    m.delay_ms        = -1;
    m.throughput_kbps = 0;
    totalPacketsFailed++;
  }

  m.packet_loss = (totalPacketsFailed * 100.0) / totalPacketsSent;
  return m;
}

// ================= KIRIM DATA KE FIREBASE =================
// Perubahan dari versi sebelumnya:
//   - serverName (HTTP lokal) diganti ke Firestore REST API (HTTPS)
//   - Format JSON disesuaikan ke format Firestore (setiap field dibungkus tipe data)
//   - Menggunakan WiFiClientSecure untuk koneksi HTTPS
//   - Menambahkan field timestamp, kwh_hari, kwh_bulan langsung di payload
//   - Menambahkan field rssi, delay_ms, throughput, packet_loss
void kirimDataKeFirebase(float suhu, float humid, float tegangan, float arus, float daya, int ir1, int ir2, int ir3, int ir4) {

  if (WiFi.status() == WL_CONNECTED) {

    // Hitung estimasi kWh
    float kwh_per_hari  = (daya * 24.0) / 1000.0;
    float kwh_per_bulan = kwh_per_hari * 30.0;

    // Ukur RSSI
    int rssi = WiFi.RSSI();

    // Buat payload sementara untuk hitung ukuran (keperluan throughput)
    String jsonPayload = "{\"fields\":{\"suhu\":{\"doubleValue\":" + String(suhu, 2) + "}}}";
    int estimatedSize  = jsonPayload.length() + 400; // estimasi ukuran payload penuh

    // Ukur network metrics SEBELUM kirim data asli
    NetMetrics net = getNetworkMetrics(estimatedSize);

    // Format JSON khusus Firestore REST API
    // Setiap field harus dibungkus dengan tipe datanya: doubleValue, integerValue, dll
    jsonPayload  = "{";
    jsonPayload += "\"fields\": {";
    jsonPayload += "\"suhu\":{\"doubleValue\":"           + String(suhu, 2)               + "},";
    jsonPayload += "\"humid\":{\"doubleValue\":"          + String(humid, 2)              + "},";
    jsonPayload += "\"tegangan\":{\"doubleValue\":"       + String(tegangan, 2)           + "},";
    jsonPayload += "\"arus\":{\"doubleValue\":"           + String(arus, 2)               + "},";
    jsonPayload += "\"daya\":{\"doubleValue\":"           + String(daya, 2)               + "},";
    jsonPayload += "\"kwh_hari\":{\"doubleValue\":"       + String(kwh_per_hari, 3)       + "},";
    jsonPayload += "\"kwh_bulan\":{\"doubleValue\":"      + String(kwh_per_bulan, 3)      + "},";
    jsonPayload += "\"ir1\":{\"integerValue\":\""         + String(ir1)                   + "\"},";
    jsonPayload += "\"ir2\":{\"integerValue\":\""         + String(ir2)                   + "\"},";
    jsonPayload += "\"ir3\":{\"integerValue\":\""         + String(ir3)                   + "\"},";
    jsonPayload += "\"ir4\":{\"integerValue\":\""         + String(ir4)                   + "\"},";
    jsonPayload += "\"rssi\":{\"integerValue\":\""        + String(rssi)                  + "\"},";
    jsonPayload += "\"delay_ms\":{\"doubleValue\":"       + String(net.delay_ms, 1)       + "},";
    jsonPayload += "\"throughput\":{\"doubleValue\":"     + String(net.throughput_kbps, 2)+ "},";
    jsonPayload += "\"packet_loss\":{\"doubleValue\":"    + String(net.packet_loss, 2)    + "},";
    jsonPayload += "\"timestamp\":{\"timestampValue\":\"" + getISOTimestamp()             + "\"}";
    jsonPayload += "}}";

    // Gunakan WiFiClientSecure karena Firestore pakai HTTPS
    WiFiClientSecure client;
    client.setInsecure(); // skip verifikasi SSL cert (cukup untuk ESP32)

    HTTPClient http;
    http.begin(client, firestoreUrl);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.printf("📡 Data terkirim ke Firebase! (%d)\n", httpResponseCode);
      Serial.printf("📶 RSSI: %d dBm | Delay: %.1f ms | Throughput: %.2f kbps | Packet Loss: %.1f%%\n",
                    rssi, net.delay_ms, net.throughput_kbps, net.packet_loss);
    }
    else {
      totalPacketsFailed++;
      Serial.printf("❌ Gagal kirim data! Kode: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
  }
  else {
    Serial.println("📴 WiFi terputus, data tidak dikirim.");
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8E1, RXD2, TXD2);

  pinMode(MAX485_RE, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE, LOW);
  digitalWrite(MAX485_DE, LOW);

  pinMode(IR1, INPUT_PULLUP);
  pinMode(IR2, INPUT_PULLUP);
  pinMode(IR3, INPUT_PULLUP);
  pinMode(IR4, INPUT_PULLUP);

  Wire.begin(21, 22);

  if (!bme.begin(0x76)) {
    Serial.println("Gagal deteksi BME280!");
  }

  dht.begin();
  Serial.println("✅ DHT11 Siap.");

  node.begin(1, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // ================= MODE WIFI =================
  WiFi.mode(WIFI_AP_STA);

  // ================= HOTSPOT ESP32 =================
  WiFi.softAP(ap_ssid, ap_password, ap_channel);

  Serial.println("=================================");
  Serial.println("📡 HOTSPOT ESP32 AKTIF");
  Serial.print("SSID : ");
  Serial.println(ap_ssid);
  Serial.print("Channel : ");
  Serial.println(ap_channel);
  Serial.print("IP Hotspot : ");
  Serial.println(WiFi.softAPIP());
  Serial.println("=================================");

  // ================= CONNECT ROUTER =================
  Serial.printf("🔗 Menghubungkan ke %s...\n", ssid);

  WiFi.begin(ssid, password);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Router Terhubung!");
    Serial.print("📡 IP ESP32 (Router): ");
    Serial.println(WiFi.localIP());

    // Sync waktu via NTP setelah WiFi terhubung
    // Dibutuhkan agar timestamp yang dikirim ke Firebase akurat
    setupNTP();
  }
  else {
    Serial.println("\n⚠ Gagal konek router, tetap aktif sebagai hotspot.");
    Serial.println("⚠ Data tidak akan dikirim ke Firebase tanpa internet.");
  }
}

// ================= LOOP =================
void loop() {

  uint8_t result;
  uint16_t data, rawLow, rawHigh;
  uint32_t combined;
  float value;

  int s1 = digitalRead(IR1);
  int s2 = digitalRead(IR2);
  int s3 = digitalRead(IR3);
  int s4 = digitalRead(IR4);

  float temp = bme.readTemperature();
  float press = bme.readPressure() / 100.0F;

  Serial.println("======================================");
  Serial.printf("IR: %d %d %d %d\n", s1, s2, s3, s4);
  Serial.printf("Suhu: %.2f °C | Tekanan: %.2f hPa\n", temp, press);
  Serial.println("======================================\n");

  delay(100);

  result = node.readInputRegisters(1123, 2);

  if (result == node.ku8MBSuccess) {
    data = node.getResponseBuffer(0);
    Serial.print("Voltage (V) = ");
    Serial.println(data);
  }

  delay(200);

  result = node.readInputRegisters(1099, 2);

  if (result == node.ku8MBSuccess) {

    rawLow = node.getResponseBuffer(0);
    rawHigh = node.getResponseBuffer(1);

    combined = ((uint32_t)rawHigh << 16) | rawLow;

    memcpy(&value, &combined, 4);

    Serial.print("Current (A): ");
    Serial.println(value, 2);
  }

  delay(200);

  result = node.readInputRegisters(1139, 2);

  if (result == node.ku8MBSuccess) {

    rawLow = node.getResponseBuffer(0);
    rawHigh = node.getResponseBuffer(1);

    combined = ((uint32_t)rawHigh << 16) | rawLow;

    memcpy(&value, &combined, 4);

    Serial.print("Daya (W): ");
    Serial.println(value, 2);
  }

  float tegangan = data;
  float arus = value;

  float daya = tegangan * arus;

  float kwh_per_hari = (daya * 24.0) / 1000.0;
  float kwh_per_bulan = kwh_per_hari * 30.0;

  Serial.printf("⚡ Estimasi KWh per hari: %.3f kWh\n", kwh_per_hari);
  Serial.printf("📆 Estimasi KWh per bulan: %.3f kWh\n\n", kwh_per_bulan);

  delay(1000);

  float suhuDHT = dht.readTemperature();
  float humidDHT = dht.readHumidity();

  if (!isnan(suhuDHT) && !isnan(humidDHT)) {

    Serial.printf("🌡 DHT11 => Suhu: %.2f°C | Kelembapan: %.2f%%\n", suhuDHT, humidDHT);

    // Nama fungsi diubah dari kirimDataKeServer → kirimDataKeFirebase
    // Parameter tetap sama persis
    kirimDataKeFirebase(
      suhuDHT,
      humidDHT,
      tegangan,
      arus,
      daya,
      s1,
      s2,
      s3,
      s4
    );
  }
  else {
    Serial.println("⚠ DHT11 tidak terbaca!");
  }

  delay(3000);
}