// Node ID: 434115122
#include "painlessMesh.h"
#include "mash_parameter.h"
#include "CRCMASH.h"

Scheduler userScheduler;

bool buttonklick = 0;

int stableButState    = HIGH;           // Стабільний стан кнопки
int lastButState      = HIGH;           // Останнє "сире" зчитування (ще не стабільне)
unsigned long lastDebounceTime = 0;     // Коли востаннє змінювався стан
const unsigned long DEBOUNCE_MS = 50;   // Антидребезг (50 мс)

void power () {
  if (buttonklick == 1) {
    buttonklick = 0;
    sendB("La0");
  } else {
    buttonklick = !buttonklick;
    sendB("La1");
  }
}

void echoSend () {
  if (buttonklick == 0) {
    sendB("La0");
  } else {
    sendB("La1");
  }
}
void powerBatt() {
  int cButState = digitalRead(5);   // Зчитуємо "сирий" стан піну

  if (cButState != lastButState) {
    // Якщо змінився "сирий" стан — це може бути дребезг:
    lastButState      = cButState; 
    lastDebounceTime  = millis();  // Починаємо відлік
  }

  // Якщо пройшло більше 50 мс після останньої зміни
  if ( (millis() - lastDebounceTime) > DEBOUNCE_MS ) {
    // Якщо стабільний стан справді відрізняється від останнього зчитування
    if (stableButState != lastButState) {
      stableButState = lastButState;  // Оновлюємо стабільний стан

      // Якщо кнопка перейшла в LOW => реальне натискання
      if (stableButState == LOW) {
        power();  // Викликаємо дію один раз
      }
    }
  }
}

void handleBody( const String &msg ) {
  String str1 = msg.c_str();
  String str2 = "lam";
  String str3 = "lamech";

  if (str1.equals(str2)) {
    power();
  }

  if (str1.equals(str3)) {
    echoSend();
  }
}

void setup() {
  Serial.begin(115200);
  
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);

  pinMode(4, INPUT);
  pinMode(5, INPUT_PULLUP);
  lastButState   = digitalRead(5); 
  stableButState = lastButState;
}

void loop() {
  // === CRCMASH queue processing ===
  for (uint8_t _i=0; _i<4; ++_i){ String _b; if (!qPop(_b)) break; handleBody(_b); }
  mesh.update();

  powerBatt();

  if (buttonklick == 1) {
    pinMode(4, OUTPUT);
  } else {
    pinMode(4, INPUT);
  }
}