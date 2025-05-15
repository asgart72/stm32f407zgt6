#define TFT_Class GxTFT
#include <GxTFT.h>
#include <GxIO/STM32MICRO/GxIO_STM32F407ZET6_FSMC/GxIO_STM32F407ZET6_FSMC.h>
#include <GxCTRL/GxCTRL_ILI9341/GxCTRL_ILI9341.h>

// FSMC Display Setup
GxIO_STM32F407ZET6_FSMC io;
GxCTRL_ILI9341 controller(io);
TFT_Class tft(io, controller, 320, 240);

#include <MPU6050.h>
#include <MechaQMC5883.h>
/*
Dieser Kompass und Horizont wurde mit hilfe von ChatGPT und anderen erstellt. Dieses Programm ist eine Demonstration 
für STM32F407ZGT6 BlackBoard mit 16bit ili9341 Display, Gyro 6050, QMC5883 in Arduino gefertigt!!
für Anregungen sind alle erfreut. Last es um die Welt gehen!!!!    Keine gewähr auf vollständigkeit!!!!  Alles unter GNU Lizenz. */

// Sensoren
MPU6050 mpu;
MechaQMC5883 compass;

#define GxTFT_BLACK   0x0000
#define GxTFT_WHITE   0xFFFF
#define GxTFT_RED     0xF800
#define GxTFT_GREEN   0x07E0
#define GxTFT_BLUE    0x001F
#define GxTFT_YELLOW  0xFFE0
#define GxTFT_BROWN   0xA145
#define CALIBRATION_PIN PC9

// Kalibrierwerte
float zero_ax = 0, zero_ay = 0;

void setup() {
  Wire.begin();
  delay(100);

  // MPU6050 Setup
  mpu.initialize();
  if (!mpu.testConnection()) {
    while (1); // Hängt hier, wenn nicht verbunden
  }
 pinMode(CALIBRATION_PIN, INPUT_PULLUP);
  // Kompass Setup
  compass.init();
  compass.setMode(Mode_Continuous,ODR_200Hz,RNG_2G,OSR_256);
  
  // TFT Setup
  tft.init();
  tft.setRotation(3); // horizontal
  tft.fillScreen(GxTFT_BLACK);

  // Begrüßung
  tft.setTextColor(GxTFT_WHITE);
  tft.setCursor(10, 10);
  tft.setTextSize(1);
  tft.print("Kunstfluganzeige mit Kompass");
  delay(4000);
  // Erste Kalibrierung
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  zero_ax = ax;
  zero_ay = ay;
}

void loop() {
  // Tasterabfrage für Kalibrierung
  if (digitalRead(CALIBRATION_PIN) == LOW) {
    int16_t ax_raw, ay_raw, az;
    mpu.getAcceleration(&ax_raw, &ay_raw, &az);
    zero_ax = ax_raw;
    zero_ay = ay_raw;

    // Optional: Feedback auf dem Display
    tft.setTextColor(GxTFT_GREEN, GxTFT_BLACK);
    tft.setCursor(10, 220);
    tft.print("Kalibriert     ");
    delay(500); // Entprellung & kurze Anzeige
  }
  
  // === MPU6050 auslesen ===
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  ax -= zero_ax;
  ay -= zero_ay;

  float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  float roll  = atan2(ay, az) * 180.0 / PI;

  // === Kompass auslesen ===
 int x, y, z;
  compass.read(&x, &y, &z);
  float heading = atan2((float)y, (float)x) * 180.0 / PI;
  if (heading < 0) heading += 360;

  // === Grafik aktualisieren ===
  drawHorizon(pitch, roll);
  drawCompass(heading);

  delay(200);
}

// ======= Anzeige künstlicher Horizont (links) =======
void drawHorizon(float pitch, float roll) {
  static float last_pitch = 999;
  static float last_roll = 999;

  if (abs(pitch - last_pitch) < 1 && abs(roll - last_roll) < 1) return;

  last_pitch = pitch;
  last_roll = roll;

  const int cx = 80;  // Zentrum links
  const int cy = 120;
  const int r = 70;   // Radius der Kugelanzeige

  // Anzeigebereich löschen
  tft.fillRect(0, 0, 160, 240, GxTFT_BLACK);

  // Hintergrund-Kreis zeichnen (Rahmen)
  tft.drawCircle(cx, cy, r, GxTFT_WHITE);
  tft.drawCircle(cx, cy, r, GxTFT_WHITE); // Rahmen
 
  // === Künstlicher Horizont: Himmel und Boden auf "Kugel" ===
  float roll_rad = roll * PI / 180.0;
  float pitch_offset = pitch * 1.0;

  for (int y = -r; y <= r; y++) {
    for (int x = -r; x <= r; x++) {
      if (x * x + y * y > r * r) continue;

      // Transformation: drehen um Roll und verschieben um Pitch
      float xf = x * cos(roll_rad) - (y - pitch_offset) * sin(roll_rad);
      float yf = x * sin(roll_rad) + (y - pitch_offset) * cos(roll_rad);

      uint16_t color = (yf > 0) ? GxTFT_GREEN : GxTFT_BLUE;
      tft.drawPixel(cx + x, cy + y, color);
    }
  }

  // === Horizontlinie ===
  int dx = r * sin(roll_rad);
  int dy = pitch_offset * 1.0;
  tft.drawLine(cx - dx, cy + dy, cx + dx, cy - dy, GxTFT_WHITE);

  // === Pitch-Skala ===
  drawPitchScale(pitch, roll, cx, cy, r);

  // === Flugzeugsymbol ===
  tft.drawLine(cx - 10, cy, cx + 10, cy, GxTFT_YELLOW);
  tft.drawLine(cx, cy - 10, cx, cy + 10, GxTFT_YELLOW);
}


void drawPitchScale(float pitch, float roll, int cx, int cy, int r) {
  float roll_rad = roll * PI / 180.0;

  for (int deg = -60; deg <= 60; deg += 10) {
    if (deg == 0) continue; // Horizontlinie ist separat

    float y_offset = (deg - pitch) * 1.0;  // Pitch-Verschiebung
    if (abs(y_offset) > r) continue; // außerhalb der Kugel

    // Position der Linie berechnen (vor Rotation)
    int len = (deg % 20 == 0) ? 20 : 10;  // Längere Linien bei 20°
    int x1 = -len;
    int x2 = len;

    int y = y_offset;

    // Rotation auf Linie anwenden
    int rx1 = cx + x1 * cos(roll_rad) - y * sin(roll_rad);
    int ry1 = cy + x1 * sin(roll_rad) + y * cos(roll_rad);

    int rx2 = cx + x2 * cos(roll_rad) - y * sin(roll_rad);
    int ry2 = cy + x2 * sin(roll_rad) + y * cos(roll_rad);

    // Linie zeichnen
    tft.drawLine(rx1, ry1, rx2, ry2, GxTFT_WHITE);

    // Textanzeige daneben (z.B. -10, +10)
    tft.setTextSize(1);
    tft.setTextColor(GxTFT_WHITE);

    char label[5];
    sprintf(label, "%d", deg);

    int tx = cx + (len + 5) * cos(roll_rad) - y * sin(roll_rad);
    int ty = cy + (len + 5) * sin(roll_rad) + y * cos(roll_rad);

    tft.setCursor(tx - 6, ty - 3); // Zentrieren
    tft.print(label);
  }
}

// ======= Anzeige Kompass (rechts) =======
void drawCompass(float heading) {
  int cx = 240; // Mitte rechts
  int cy = 120;
  int r = 60;

  static float last_heading = 999;
  if (abs(heading - last_heading) < 2) return;
  last_heading = heading;

  // Bereich löschen
  tft.fillRect(160, 0, 160, 240, GxTFT_BLACK);

  // Kompasskreis
  tft.drawCircle(cx, cy, r, GxTFT_WHITE);

  // Richtungspfeil
  float angle = heading * PI / 180.0;
  int x = cx + r * sin(angle);
  int y = cy - r * cos(angle);

  tft.drawLine(cx, cy, x, y, GxTFT_RED);
  tft.fillCircle(x, y, 4, GxTFT_RED);

  // Buchstaben anzeigen
  tft.setTextColor(GxTFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(cx - 4, cy - r - 10); tft.print("N");
  tft.setCursor(cx + r + 2, cy - 4);  tft.print("E");
  tft.setCursor(cx - 4, cy + r + 2);  tft.print("S");
  tft.setCursor(cx - r - 8, cy - 4);  tft.print("W");

  // Grad anzeigen
  tft.setCursor(cx - 15, cy + r + 16);
  tft.setTextColor(GxTFT_YELLOW);
  tft.print((int)heading);
  tft.print((char)176); // °
}
