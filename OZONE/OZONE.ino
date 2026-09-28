#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "DFRobot_OzoneSensor.h" 

#define I2C_SDA 21
#define I2C_SCL 22
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define COLLECT_NUMBER 45
#define Ozone_IICAddress OZONE_ADDRESS_0 
DFRobot_OzoneSensor Ozone;

SX1276 radio = new Module(18, 26, 14, 33);

uint64_t joinEUI = 0x1e9796173571229a;
uint64_t devEUI  = 0xe02d6ee6c5f39cfd;
uint8_t appKey[16] = { 0xde, 0xa7, 0xc5, 0x0f, 0x2e, 0x47, 0x81, 0x18, 
                       0x2d, 0x12, 0xfe, 0x46, 0x9f, 0xbd, 0x17, 0x99 };
uint8_t nwkKey[16] = { 0xde, 0xa7, 0xc5, 0x0f, 0x2e, 0x47, 0x81, 0x18, 
                       0x2d, 0x12, 0xfe, 0x46, 0x9f, 0xbd, 0x17, 0x99 };

LoRaWANNode node(&radio, &AS923, 0);
Preferences prefs;
int count = 0;

void oledPrint(const String &l1, const String &l2 = "", const String &l3 = "", const String &l4 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(l1); display.println(l2);
  display.println(l3); display.println(l4);
  display.display();
}

#define NONCES_BUF_SIZE 128

void saveNonces() {
  uint8_t buf[NONCES_BUF_SIZE];
  memcpy(buf, node.getBufferNonces(), NONCES_BUF_SIZE);
  prefs.begin("lorawan", false);
  prefs.putBytes("nonces", buf, NONCES_BUF_SIZE);
  prefs.end();
}

bool loadNonces() {
  prefs.begin("lorawan", true);
  bool hasData = prefs.isKey("nonces");
  uint8_t buf[NONCES_BUF_SIZE];
  if (hasData) prefs.getBytes("nonces", buf, NONCES_BUF_SIZE);
  prefs.end();
  
  if (hasData) {
    if (node.setBufferNonces(buf) == RADIOLIB_ERR_NONE) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Gagal Terhubung"));
  }

  if(!Ozone.begin(Ozone_IICAddress)) {
    Serial.println(F("Sensor Ozon Gagal Terhubung!"));
    oledPrint("Error Hardware", "Ozon Tidak Ditemukan", "Cek Kabel/Switch");
    while (1);
  }
  Ozone.setModes(MEASURE_MODE_PASSIVE);

  oledPrint("LilyGO LoRaWAN", "Init radio...");
  SPI.begin(5, 19, 27, 18);
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    oledPrint("Radio Init Gagal", "kode: " + String(state));
    while (true);
  }

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  loadNonces();

  oledPrint("Join network...", "(OTAA)", "mohon tunggu");
  state = node.activateOTAA();
  saveNonces();

  if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
    oledPrint("Join Gagal", "kode: " + String(state));
    while (true);
  }

  oledPrint("Join Berhasil!", "Siap kirim Ozon");
  delay(1500);
}

void loop() {
  int16_t ozone_ppb = Ozone.readOzoneData(COLLECT_NUMBER);
  uint8_t idInt   = count % 256;
  
  uint8_t payload[3];
  payload[0] = idInt;
  payload[1] = (ozone_ppb >> 8) & 0xFF; 
  payload[2] = ozone_ppb & 0xFF;        

  Serial.printf("\n[Sensor Ozon] Ozon: %d PPB\n", ozone_ppb);
  int state = node.sendReceive(payload, sizeof(payload), 1);
  String statusLoRa = (state == RADIOLIB_ERR_NONE || state > 0) ? "OK" : "ERR " + String(state);
  
  oledPrint(">> TX Ozon #" + String(count) + " <<", 
            "",
            "O3: " + String(ozone_ppb) + " PPB",
            "LoRa: " + statusLoRa);

  if (state == RADIOLIB_ERR_NONE || state > 0) {
    Serial.println(F("[Status] BERHASIL TERKIRIM"));
  } else {
    Serial.printf("[Status] GAGAL! Kode Error: %d\n", state);
  }

  count++;
  delay(15000); 
}