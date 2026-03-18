# IoT Web Monitor

Proyek **IoT Web Monitor** adalah sebuah sistem cerdas yang dibuat untuk memantau data sensor secara real-time dari perangkat ESP32 dan Arduino. Sistem ini mengirimkan data melalui koneksi internet untuk disimpan dan ditampilkan dari jarak jauh (Remote Monitoring).

## Fitur Utama

*   **Real-time Sensor Monitoring**: Pantau Suhu, Kelembapan, Tegangan, Arus, Daya, dan estimasi KWh.
*   **Keamanan Terjaga**: Implementasi Firebase Firestore dengan metode *Rest API Secure*. Kredensial tidak diumbar di dalam repositori.
*   **Smart Analytics**: Dilengkapi kemampuan grafik yang dapat dikendalikan pengguna menggunakan library Chart.js.
*   **Networking Metrik**: Pantau kualitas jaringan ESP32 langsung melalui panel, meliputi Latency, Throughput, Packet Loss, & RSSI.
*   **Export Log**: Export data monitor ke bentuk CSV atau Microsoft Excel untuk analisa lebih dalam.

## Persiapan & Instalasi (Web Dashboard)

1. Clone repositori ini atau download sebagai `.zip`:
   ```bash
   git clone https://github.com/JonathanA-P/IOTWebMonitor.git
   ```
2. Salin file template konfigurasi:
   Masuk ke folder `js/` lalu jadikan file `config.example.js` menjadi `config.js` di dalam direktori komputer Anda.
   *(File `config.js` memang di-ignore oleh gitignore, agar API Key Anda aman dari pihak luar).*
3. Edit file `js/config.js` dan isikan data sesuai dengan *Project Data* di akun Firebase Console Anda:
   ```javascript
   const firebaseConfig = {
     apiKey:            "KODE_RAHASIA",
     authDomain:        "DOMAN_ANDA",
     projectId:         "PROJECT_ANDA",
     storageBucket:     "BUCKET_ANDA",
     messagingSenderId: "ID_PENGIRIM",
     appId:             "APP_ID"
   };
   ```
4. Buka file `dashboard.html` di browser Anda (Chrome / Firefox modern).
5. Panel akan memuat data otomatis dari Firebase (pastikan Rule Database Firestore Anda mengizinkan proses Read/Write).

## Persiapan (Hardware ESP32)

1. Buka file sketsa Arduino di folder `omega/omega.ino`.
2. Ubah Konstanta berikut sesuai dengan Firebase dan Wifi Anda:
   ```cpp
   #define FIREBASE_PROJECT_ID "ganti_dengan_id_project_firebase_anda"
   const char* ssid = "NAMA_WIFI_ANDA";
   const char* password = "PASSWORD_WIFI_ANDA";
   ```
3. Compile dan upload kode ke ESP32 Anda. ESP32 otomatis mengkalkulasi *Networking Metrics* (seperti packet loss) ke Firebase di setiap pemanggilannya.
