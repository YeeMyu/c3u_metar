#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define LCD_ADRS 0x3E
#define LCD_LINE1 0x00+0x80 
#define LCD_LINE2 0x40+0x80 
#define SDA_PIN 1
#define SCL_PIN 0
#define ON 1
#define OFF 0

int first_flag = ON;
int ten_min_flag = OFF;
int forty_min_flag = OFF;
int getMetar_flag = OFF;

// Wifi
const char* ssid     = "SSID";
const char* password = "PASSWORD";

// NTPサーバー
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
struct tm timeinfo;

// METAR提供ホストとICAO空港コード（例: 羽田空港 = RJTT）
const char* host = "tgftp.nws.noaa.gov";
// const String icao_codes[] = {"RJTT"}; 
const String icao_codes[] = {"ROAH", "ROIG", "ROMY", "RORS", "ROMD",
                             "RJKA", "RJFG", "RJFE", "RJDT"};
const int icao_codes_num = sizeof(icao_codes)/sizeof(icao_codes[0]);
String metar_line[icao_codes_num];

void setup() {
  int i;

  // Serial.begin(115200);
  // delay(1000);

  // I2C接続開始、LCD初期化
  Wire.begin(SDA_PIN, SCL_PIN);
  init_LCD();

  // Wi-Fi接続開始
  print_LCD("Connect to WiFi");
  WiFi.begin(ssid, password);
  writeCommand(LCD_LINE2); // 2LINE TOP
  i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    writeData('.');
    ++i;
    // 右端まで到達したらクリア
    if(i == 16) {
      clear_LCDline(LCD_LINE2);
      i = 0;
    }
  }
  clear_LCDline(LCD_LINE2);
  print_LCD("Connected!");
  delay(1000);

  // 時刻同期
  writeCommand(0x01); // CLEAR
  syncTime();
  delay(1000);

}

void loop() {
  char metar_print_line[17];
  String line;

  // METAR取得
  getLocalTime(&timeinfo);
  if(first_flag == ON) {
    getMetar_flag = ON;
    first_flag = OFF;
  } else if (timeinfo.tm_min >= 10 && timeinfo.tm_min < 20) {
    if(ten_min_flag == OFF) {
      ten_min_flag = ON;
      getMetar_flag = ON;
    }
  } else if(timeinfo.tm_min >= 40 && timeinfo.tm_min < 50) {
    if(forty_min_flag == OFF) {
      forty_min_flag = ON;
      getMetar_flag = ON;
    }
  } else {
    ten_min_flag = OFF;
    forty_min_flag = OFF;
    getMetar_flag = OFF;
  }

  // Serial.printf("%d, %d, %d, %d\n", first_flag, ten_min_flag, forty_min_flag, getMetar_flag);
  writeCommand(0x01); // CLEAR
  if(getMetar_flag == ON) {
    print_LCD("Get METAR data");
    writeCommand(LCD_LINE2); // 2LINE TOP
    for(int i = 0; i < icao_codes_num; i++) {
      metar_line[i] = getMetarData(icao_codes[i]);
      writeData('.');
      // 右端まで到達したらクリア
      if(i == 16)
        clear_LCDline(LCD_LINE2);
      delay(1000);
    }
    getMetar_flag = OFF;
  }

  // METAR表示
  for(int i = 0; i < icao_codes_num; i++) {
    // Serial.println(metar_line[i]);
    writeCommand(0x01); // CLEAR
    line = metar_line[i].substring(0, 12);
    line.toCharArray(metar_print_line, 17);
    print_LCD(metar_print_line);
    // Serial.println(metar_line[i].length());
    for(int j = 13; j <= metar_line[i].length() - 16; j++) {
      writeCommand(LCD_LINE2); // 2LINE TOP
      line = metar_line[i].substring(j, j + 16);
      // Serial.println(line);
      line.toCharArray(metar_print_line, 17);
      print_LCD(metar_print_line);
      if(j == 13)
        delay(2000);
      else
        delay(200);
    }
    delay(2000);
  }
}

//データ書き込み
void writeData(byte t_data)
{
  Wire.beginTransmission(LCD_ADRS);
  Wire.write(0x40);
  Wire.write(t_data);
  Wire.endTransmission();
  delay(1);
}

//コマンド書き込み
void writeCommand(byte t_command)
{
  Wire.beginTransmission(LCD_ADRS);
  Wire.write(0x00);
  Wire.write(t_command);
  Wire.endTransmission();
  delay(10);
}

//液晶初期化
void init_LCD() {
  delay(100);
  writeCommand(0x38);
  delay(20);
  writeCommand(0x39);
  delay(20);
  writeCommand(0x14);
  delay(20);
  writeCommand(0x73);
  delay(20);
  writeCommand(0x56);
  delay(20);
  writeCommand(0x6C);
  delay(20);
  writeCommand(0x38);
  delay(20);
  writeCommand(0x01);
  delay(20);
  writeCommand(0x0C);
  delay(20);
}

void syncTime() {
  int i;

  // NTPサーバーから時刻同期を開始
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  print_LCD("Sync time...");

  // 時刻が同期されるのを待つ
  writeCommand(LCD_LINE2);
  i = 0;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    writeData('.');
    ++i;
    // 右端まで到達したらクリア
    if(i == 16) {
      writeCommand(LCD_LINE2);
      clear_LCDline(LCD_LINE2);
      writeCommand(LCD_LINE2);
      i = 0;
    }
  }
  writeCommand(LCD_LINE2);
  print_LCD("Synchronized!");
}

void clear_LCDline(byte t_row) {
  writeCommand(t_row);
  print_LCD("                ");
  writeCommand(t_row);
}

void print_LCD(char* line) {
  int len = strlen(line), i;
  for(i = 0; i < len; i++)
    writeData(line[i]);
}

String getMetarData(String icao_code) {
    String line, metar_line;
    WiFiClientSecure client;

    // ESP32-C3でのSSLルート証明書検証を省略（簡易化のため）
    client.setInsecure(); 

    if (!client.connect(host, 443)) // HTTPS
        // HTTPS接続失敗時は標準HTTPポート(80)を試す
        if (!client.connect(host, 80))
          return "NOAA connect Err";

    // NOAAのテキストMETARデータURLパスを生成
    String url = "/data/observations/metar/stations/" + String(icao_code) + ".TXT";
    
    // HTTP GETリクエストの送信
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "User-Agent: M5StampC3U\r\n" +
                "Connection: close\r\n\r\n");

    // レスポンスの待機
    while (client.connected()) {
        line = client.readStringUntil('\n');
        if (line == "\r") {
            // ヘッダーの終了（空行）を検知したらループを抜ける
            break;
        }
    }

    // ボディ（METAR生データ）の読み込みと出力
    while (client.available())
      metar_line = client.readStringUntil('\n');

  // 2行目のみ返す
  return metar_line;
}
