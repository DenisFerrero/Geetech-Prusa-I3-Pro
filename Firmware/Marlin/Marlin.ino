#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <BluetoothSerial.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h> // Inclusione della libreria Preferences

// Definisci l'oggetto Bluetooth Serial
BluetoothSerial SerialBT;

// Definisci i pin e il numero di LED per le due strisce
#define LED1 18
#define LED2 19
#define NUM_LED 16
#define DEFAULT_LUM 191  // Luminosità predefinita al 75% (255 * 0.75)

// Dichiarazione delle variabili di offset
float offsetX = 0;
float offsetY = 0;
float offsetZ = 0;

// Valori correnti dell'accelerometro
float X_ = 0;
float Y_ = 0;
float Z_ = 0;

// Soglia per il controllo di sicurezza
float asse_stop = 10.0; // Impostato inizialmente a 10

// Variabili di stato
bool stop_flag = 0;            // Variabile per gestire lo stato di stop
bool security_locked = true;   // Variabile per gestire lock/unlock
bool isChangingColor = false;  // Variabile per gestire il cambio colore

// Oggetto MPU6050
Adafruit_MPU6050 mpu;

// Crea due oggetti Adafruit_NeoPixel per le due strisce
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(NUM_LED, LED1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(NUM_LED, LED2, NEO_GRB + NEO_KHZ800);

// Inizializza le variabili del colore
int R = 255; // Rosso al 100%
int G = 0;
int B = 0;

// Definisci i colori come funzioni per evitare problemi
uint32_t getColor() {
  return strip1.Color(R, G, B);
}

uint32_t getBlack() {
  return strip1.Color(0, 0, 0);
}

uint32_t getColorf() {
  return strip1.Color(R / 2, G / 2, B / 2);
}

#define timer 20

// Variabili per la logica non bloccante
unsigned long previousMillis = 0;
const long interval = timer * 2; // Intervallo per kitt_scan

int currentPixel = 0;
bool directionForward = true;

// Oggetto Preferences
Preferences preferences;

// Funzione di controllo sicurezza
void security_control() {
  if (security_locked) { // Esegui il controllo solo se è bloccato
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    X_ = a.acceleration.x - offsetX;
    Y_ = a.acceleration.y - offsetY;
    Z_ = a.acceleration.z - offsetZ;

    Serial.printf("Accel: X=%.2f Y=%.2f Z=%.2f\n", X_, Y_, Z_);

    if(abs(Z_) >= asse_stop || abs(Y_) >= asse_stop || abs(X_) >= asse_stop){
      stop_flag = 1;
      Serial.println("Azione: Stop flag impostato da accelerometro");
      SerialBT.println("Effetto Kitt fermato da accelerometro.");
    }
  }
}

// Funzione per l'effetto Kitt iniziale
void kitt_start() {
  Serial.println("Eseguendo kitt_start...");
  
  // Aumenta gradualmente la luminosità
  for (int i = 0; i <= DEFAULT_LUM; i++) {
    strip1.setBrightness(i);
    strip2.setBrightness(i);
    strip1.fill(getColor());
    strip2.fill(getColor());
    strip1.show();
    strip2.show();
    delay(10);
  }

  // Diminuisci gradualmente la luminosità
  for (int i = DEFAULT_LUM; i >= 0; i--) {
    strip1.setBrightness(i);
    strip2.setBrightness(i);
    strip1.fill(getColor());
    strip2.fill(getColor());
    strip1.show();
    strip2.show();
    delay(10);
  }

  // Reimposta la luminosità a DEFAULT_LUM
  strip1.setBrightness(DEFAULT_LUM);
  strip2.setBrightness(DEFAULT_LUM);
  strip1.fill(getColor());
  strip2.fill(getColor());
  strip1.show();
  strip2.show();
  
  Serial.println("kitt_start completato. Luminosità reimpostata a DEFAULT_LUM.");
}

// Funzione per eseguire l'effetto scan Kitt
void kitt_scan(int pixel) {
  // Assicurati che i pixel non vadano fuori dai limiti
  if(pixel < 0 || pixel >= NUM_LED) {
    Serial.printf("Pixel %d fuori dai limiti.\n", pixel);
    return;
  }

  // Imposta il colore del pixel corrente su entrambe le strisce
  strip1.setPixelColor(pixel, getColor());
  strip2.setPixelColor(pixel, getColor()); // Rimosso NUM_LED - pixel - 1

  // Spegni i LED adiacenti su entrambe le strisce
  if(pixel - 2 >= 0) strip1.setPixelColor(pixel - 2, getBlack());
  if(pixel + 1 < NUM_LED) strip1.setPixelColor(pixel + 1, getBlack());
  if(pixel - 2 >= 0) strip2.setPixelColor(pixel - 2, getBlack());
  if(pixel + 1 < NUM_LED) strip2.setPixelColor(pixel + 1, getBlack());

  // Accendi i LED sfumati su entrambe le strisce
  if(pixel - 1 >= 0) strip1.setPixelColor(pixel - 1, getColorf());
  if(pixel - 1 >= 0) strip2.setPixelColor(pixel - 1, getColorf());

  strip1.show();
  strip2.show();
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_LED_Control"); // Inizializza Bluetooth con il nome del dispositivo
  Serial.println("Dispositivo Bluetooth pronto per l'accoppiamento");

  delay(1000); // Attendi che la seriale sia pronta

  // Inizializza Preferences
  preferences.begin("offsets", false); // Namespace "offsets"

  // Inizializza MPU6050
  if (!mpu.begin(0x68)) { // 0x68 è l'indirizzo predefinito per MPU6050
    Serial.println("Impossibile trovare un MPU6050 valido, verifica il cablaggio!");
    while (1);
  }
  Serial.println("MPU6050 inizializzato.");

  // Imposta la gamma dell'accelerometro
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Range accelerometro impostato a: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:
      Serial.println("2G");
      break;
    case MPU6050_RANGE_4_G:
      Serial.println("4G");
      break;
    case MPU6050_RANGE_8_G:
      Serial.println("8G");
      break;
    case MPU6050_RANGE_16_G:
      Serial.println("16G");
      break;
  }

  // Imposta la frequenza dei dati
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filtro impostato a: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ:
      Serial.println("260Hz");
      break;
    case MPU6050_BAND_184_HZ:
      Serial.println("184Hz");
      break;
    case MPU6050_BAND_94_HZ:
      Serial.println("94Hz");
      break;
    case MPU6050_BAND_44_HZ:
      Serial.println("44Hz");
      break;
    case MPU6050_BAND_21_HZ:
      Serial.println("21Hz");
      break;
    case MPU6050_BAND_10_HZ:
      Serial.println("10Hz");
      break;
    case MPU6050_BAND_5_HZ:
      Serial.println("5Hz");
      break;
  }

  // Carica gli offset dalla memoria, se disponibili
  if (preferences.isKey("offsetX") && preferences.isKey("offsetY") && preferences.isKey("offsetZ")) {
    offsetX = preferences.getFloat("offsetX", 0.0);
    offsetY = preferences.getFloat("offsetY", 0.0);
    offsetZ = preferences.getFloat("offsetZ", 0.0);
    Serial.println("Offsets caricati dalla memoria.");
  }
  else {
    // Calcolo dell'offset se non sono presenti nella memoria
    Serial.println("Calcolo degli offset...");
    for (int i = 0; i < 100; i++) { // Più letture per un offset più preciso
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      offsetX += a.acceleration.x;
      offsetY += a.acceleration.y;
      offsetZ += a.acceleration.z;
      delay(10);
    }
    // Calcola l'offset di ogni asse
    offsetX /= 100;
    offsetY /= 100;
    offsetZ /= 100;

    Serial.printf("Offset calcolati: X=%.2f Y=%.2f Z=%.2f\n", offsetX, offsetY, offsetZ);

    // Salva gli offset calcolati nella memoria
    preferences.putFloat("offsetX", offsetX);
    preferences.putFloat("offsetY", offsetY);
    preferences.putFloat("offsetZ", offsetZ);
    Serial.println("Offsets calcolati e salvati nella memoria.");
  }

  preferences.end(); // Chiudi Preferences

  delay(500);

  // Inizializza le due strisce
  strip1.begin();
  strip2.begin();

  // Imposta la luminosità iniziale
  strip1.setBrightness(0); // Inizia con luminosità 0%
  strip2.setBrightness(0);
  strip1.show();
  strip2.show();

  // Imposta il colore iniziale completamente rosso
  strip1.fill(getColor());
  strip2.fill(getColor());
  strip1.show();
  strip2.show();

  // Esegui l'effetto iniziale Kitt
  kitt_start();

  // Imposta le strisce LED a nero dopo l'effetto iniziale
  strip1.fill(getBlack());
  strip2.fill(getBlack());
  strip1.show();
  strip2.show();
  Serial.println("LED impostati a nero dopo l'effetto iniziale.");
}

void loop() {
  // Gestisci i comandi Bluetooth
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n'); // Leggi fino al carattere di nuova linea
    command.trim(); // Rimuovi eventuali spazi bianchi all'inizio e alla fine

    Serial.print("Comando ricevuto: ");
    Serial.println(command);

    if (command.equalsIgnoreCase("stop")) {
      stop_flag = 1;
      Serial.println("Azione: Stop");
      SerialBT.println("Effetto Kitt fermato.");
    }
    else if (command.equalsIgnoreCase("start")) {
      stop_flag = 0;
      Serial.println("Azione: Start");
      SerialBT.println("Effetto Kitt avviato.");
    }
    else if (command.equalsIgnoreCase("unlock")) {
      security_locked = false;
      Serial.println("Azione: Unlock");
      SerialBT.println("Sicurezza sbloccata.");
    }
    else if (command.equalsIgnoreCase("lock")) {
      security_locked = true;
      Serial.println("Azione: Lock");
      SerialBT.println("Sicurezza bloccata.");
    }
    else if (command.startsWith("R")) {
      // Comando per cambiare il valore di R: "RX"
      if (!isChangingColor) {
        isChangingColor = true;
        stop_flag = 1; // Interrompi l'effetto Kitt

        String valueStr = command.substring(1); // Escludi "R"
        int newR = valueStr.toInt();

        // Valida il valore
        newR = constrain(newR, 0, 255);
        R = newR;

        // Aggiorna le strisce LED con il nuovo colore
        strip1.fill(getColor());
        strip2.fill(getColor());
        strip1.show();
        strip2.show();

        Serial.printf("Azione: Cambia valore R a %d\n", R);
        SerialBT.printf("Valore R aggiornato a %d\n", R);

        // Imposta le strisce LED a nero
        strip1.fill(getBlack());
        strip2.fill(getBlack());
        strip1.show();
        strip2.show();
        Serial.println("LED impostati a nero durante il cambio colore.");

        // Reimposta i parametri dell'effetto Kitt
        currentPixel = 0;
        directionForward = true;

        // Riprendi l'effetto Kitt
        stop_flag = 0;
        isChangingColor = false;
      }
    }
    else if (command.startsWith("G")) {
      // Comando per cambiare il valore di G: "GX"
      if (!isChangingColor) {
        isChangingColor = true;
        stop_flag = 1; // Interrompi l'effetto Kitt

        String valueStr = command.substring(1); // Escludi "G"
        int newG = valueStr.toInt();

        // Valida il valore
        newG = constrain(newG, 0, 255);
        G = newG;

        // Aggiorna le strisce LED con il nuovo colore
        strip1.fill(getColor());
        strip2.fill(getColor());
        strip1.show();
        strip2.show();

        Serial.printf("Azione: Cambia valore G a %d\n", G);
        SerialBT.printf("Valore G aggiornato a %d\n", G);

        // Imposta le strisce LED a nero
        strip1.fill(getBlack());
        strip2.fill(getBlack());
        strip1.show();
        strip2.show();
        Serial.println("LED impostati a nero durante il cambio colore.");

        // Reimposta i parametri dell'effetto Kitt
        currentPixel = 0;
        directionForward = true;

        // Riprendi l'effetto Kitt
        stop_flag = 0;
        isChangingColor = false;
      }
    }
    else if (command.startsWith("B")) {
      // Comando per cambiare il valore di B: "BX"
      if (!isChangingColor) {
        isChangingColor = true;
        stop_flag = 1; // Interrompi l'effetto Kitt

        String valueStr = command.substring(1); // Escludi "B"
        int newB = valueStr.toInt();

        // Valida il valore
        newB = constrain(newB, 0, 255);
        B = newB;

        // Aggiorna le strisce LED con il nuovo colore
        strip1.fill(getColor());
        strip2.fill(getColor());
        strip1.show();
        strip2.show();

        Serial.printf("Azione: Cambia valore B a %d\n", B);
        SerialBT.printf("Valore B aggiornato a %d\n", B);

        // Imposta le strisce LED a nero
        strip1.fill(getBlack());
        strip2.fill(getBlack());
        strip1.show();
        strip2.show();
        Serial.println("LED impostati a nero durante il cambio colore.");

        // Reimposta i parametri dell'effetto Kitt
        currentPixel = 0;
        directionForward = true;

        // Riprendi l'effetto Kitt
        stop_flag = 0;
        isChangingColor = false;
      }
    }
    else if (command.startsWith("LUM")) {
      // Comando per cambiare la luminosità: "LUMXX" dove XX è la percentuale (0-100)
      String valueStr = command.substring(3); // Escludi "LUM"
      int lumPercentage = valueStr.toInt();

      // Valida il valore
      lumPercentage = constrain(lumPercentage, 0, 100);

      // Converti la percentuale in valore di luminosità (0-255)
      int brightness = map(lumPercentage, 0, 100, 0, 255);

      // Imposta la luminosità
      strip1.setBrightness(brightness);
      strip2.setBrightness(brightness);
      strip1.show();
      strip2.show();

      Serial.printf("Azione: Cambia luminosità a %d%% (%d)\n", lumPercentage, brightness);
      SerialBT.printf("Luminosità aggiornata a %d%% (%d)\n", lumPercentage, brightness);
    }
    else if (command.startsWith("ASSTOP")) {
      // Comando per impostare asse_stop: "ASSTOPXX"
      String valueStr = command.substring(6); // Escludi "ASSTOP"
      float newAsseStop = valueStr.toFloat();

      if (newAsseStop > 0) { // Valore valido
        asse_stop = newAsseStop;
        Serial.printf("Azione: Imposta asse_stop a %.2f\n", asse_stop);
        SerialBT.printf("Asse di stop impostato a %.2f\n", asse_stop);
      }
      else {
        Serial.println("Formato comando ASSTOP errato.");
        SerialBT.println("Formato comando ASSTOP errato. Usa: ASSTOPXX");
      }
    }
    else {
      Serial.println("Comando sconosciuto");
      SerialBT.println("Comando sconosciuto");
    }
  }

  unsigned long currentMillis = millis();

  // Se non è in stop e non si sta cambiando colore, esegui l'effetto Kitt
  if(stop_flag == 0 && !isChangingColor){
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      if (directionForward) {
        if (currentPixel < NUM_LED) {
          kitt_scan(currentPixel);
          currentPixel++;
          Serial.printf("Direzione avanti: pixel=%d\n", currentPixel);
        }
        if (currentPixel >= NUM_LED) {
          currentPixel = NUM_LED -1;
          directionForward = false;
          Serial.println("Direzione cambiata a indietro.");
        }
      }
      else {
        if (currentPixel >= 0) {
          kitt_scan(currentPixel);
          currentPixel--;
          Serial.printf("Direzione indietro: pixel=%d\n", currentPixel);
        }
        if (currentPixel < 0) {
          currentPixel = 0;
          directionForward = true;
          Serial.println("Direzione cambiata a avanti.");
        }
      }

      // Esegui il controllo di sicurezza
      security_control();

      if(stop_flag == 1){
        // Se stop_flag è impostato, spegni i LED
        strip1.fill(getBlack());
        strip2.fill(getBlack());
        strip1.show();
        strip2.show();
        Serial.println("LED spenti a causa del flag di stop.");
      }
    }
  }
  else {
    // Se in stop o in cambiamento colore, assicurati che i LED siano spenti
    if (stop_flag == 1){
      strip1.fill(getBlack());
      strip2.fill(getBlack());
      strip1.show();
      strip2.show();
      Serial.println("LED spenti in modalità stop.");
    }
    // Non fare nulla durante il cambiamento colore
  }
}
