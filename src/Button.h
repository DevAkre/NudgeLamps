#define LONG_PRESS_TIME 1000
#define LONG_PRESS_TIMEOUT 200

class Button {
  bool newState = false;
  bool oldState = false;
  unsigned long pressedTime  = 0;
  unsigned long longButtonTime = 0;
  uint8_t pin;
  public:
  
  bool isLongDetected = false;
  bool isShortDetected = false;
  bool isPressed = false;
  
  Button(uint8_t p): pin(p) {}

  void readState() {
    isShortDetected = false;
    newState = digitalRead(pin);
    
    if(newState == HIGH and oldState == LOW){
      delay(20);
      pressedTime = millis();
      isPressed = true;
    }else if(newState == LOW and oldState == HIGH){
      delay(20);
      isPressed = false;
      if( millis()- pressedTime < LONG_PRESS_TIME){
        Serial.println("- A short press is detected");
        isShortDetected = true;
      }
    }

    isLongDetected = false;
    if(isPressed){
      if( (millis()-(pressedTime+longButtonTime)) > LONG_PRESS_TIME ) {
        longButtonTime+=LONG_PRESS_TIMEOUT;
        Serial.println("- A long press is detected");
        isLongDetected = true;
      }
    }else{
      longButtonTime=0;
    }
  }

  void updateState() {
    oldState = newState;
  }
};
