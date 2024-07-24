// Node ID: 434115122
//
//

#include "painlessMesh.h"

#define   MESH_PREFIX     "kennet"
#define   MESH_PASSWORD   "kennet123"
#define   MESH_PORT       5555

painlessMesh  mesh;

bool buttonklick = 0;

unsigned long pMillis = 0;
const long interval = 100;
int lButState = LOW; // Зберігаємо останній стан кнопки

class Button {
  public:
    Button(int pin, unsigned long debounceDelay, void (*callback)()) {
      this->pin = pin;
      this->debounceDelay = debounceDelay;
      this->callback = callback;
      pinMode(pin, INPUT_PULLUP);
      buttonState = HIGH;
      lastButtonState = HIGH;
      lastDebounceTime = 0;
    }

    void update() {
      int reading = digitalRead(pin);

      // Якщо стан кнопки змінився (через дребезг або натискання)
      if (reading != lastButtonState) {
        lastDebounceTime = millis();  // Запам'ятовуємо час останньої зміни стану
      }

      // Якщо зміна стану стабільна протягом debounceDelay
      if ((millis() - lastDebounceTime) > debounceDelay) {
        // Якщо стан кнопки змінився
        if (reading != buttonState) {
          buttonState = reading;

          // Якщо кнопка натиснута
          if (buttonState == LOW) {
            callback();
          }
        }
      }

      // Запам'ятовуємо попередній стан кнопки
      lastButtonState = reading;
    }

  private:
    int pin;
    int buttonState;
    int lastButtonState;
    unsigned long lastDebounceTime;
    unsigned long debounceDelay;
    void (*callback)();
};

void buttonPressed() {
  buttonklick = !buttonklick;
}

const int buttonPin = 5;
unsigned long debounceDelay = 50;
Button button(buttonPin, debounceDelay, buttonPressed);

void receivedCallback( uint32_t from, String &msg ) {

  String str1 = msg.c_str();
  String str2 = "lam";
  String str3 = "lamech";
  Serial.print(str1);


  if (str1.equals(str2)) {

    if (buttonklick == 1) {
      buttonklick = 0;
      mesh.sendSingle(624409705,"La0");
    } else {
      buttonklick = !buttonklick;
      mesh.sendSingle(624409705,"La1");
      //Serial.print("La1");
    } 
  }

  if (str1.equals(str3)) {
    if (buttonklick == 0) {
      mesh.sendSingle(624409705,"La0");
    } else {
      mesh.sendSingle(624409705,"La1");
    }
  }
}

void setup() {
  Serial.begin(9600);
  
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  mesh.onReceive(&receivedCallback);

  pinMode(4, INPUT);
  pinMode(5, INPUT_PULLUP);
}

void loop() {
  mesh.update();

  button.update();

  if (buttonklick == 1) {
    pinMode(4, OUTPUT);
  } else {
    pinMode(4, INPUT);
  }
}
