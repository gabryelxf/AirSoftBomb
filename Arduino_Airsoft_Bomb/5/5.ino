// Viral Science www.viralsciencecreativity.com www.youtube.com/c/viralscience
// Arduino Airsoft Time Bomb

#include <Keypad.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);  //Change the HEX address

int Hours = 00;
int Minutes = 00;
int Seconds = 00;
int trycount = 0;
int keycount = 0;
int i = 0;

int redled = A3;
int yellowled = A1;
int greenled = A2;

int hourstenscode;
int hoursonescode;
int mintenscode;
int minonescode;
int sectenscode;
int seconescode;

long secMillis = 0;
long interval = 1000;

char password[4];
char entered[4];

const byte ROWS = 3;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', '4'},
  {'5', '6', '7', '8'},
  {'9', '0', '*', '#'}
  };
  
byte rowPins[ROWS] = {10, 8, 7};  //7,2,3,5 for Black 4x3 keypad
byte colPins[COLS] = {6, 5, 2, A0};     //6,8,4 for Black 4x3 Keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

int readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0) ;
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}


#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices
#include <Servo.h>
/*
  Instead of a Relay you may want to use a servo. Servos can lock and unlock door locks too
  Relay will be used by default
*/

// #include <Servo.h>

/*
  For visualizing whats going on hardware we need some leds and to control door lock a relay and a wipe button
  (or some other hardware) Used common anode led,digitalWriting HIGH turns OFF led Mind that if you are going
  to use common cathode led or just seperate leds, simply comment out #define COMMON_ANODE,
*/



#define pinServo 4

Servo servo1;
int fecha = 0;
int abre = 90;
boolean estadoServoAberto = false;

//constexpr uint8_t relay = 4;     // Set Relay Pin
constexpr uint8_t wipeB = SCL;     // Button pin for WipeMode
void (*funcReset)() = 0;

boolean match = false;          // initialize card match to false
boolean programMode = false;  // initialize programming mode to false
boolean replaceMaster = false;

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

// Create MFRC522 instance.
constexpr uint8_t RST_PIN = 9;     // Configurable, see typical pin layout above
constexpr uint8_t SS_PIN = SDA;     // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {



   servo1.attach(pinServo);
  //Arduino Pin Configuration
  pinMode(wipeB, INPUT_PULLUP);   // Enable pin's pull up resistor
 // pinMode(relay, OUTPUT);
 servo1.attach(pinServo);
  //Be careful how relay circuit behave on while resetting or power-cycling your Arduino
  //digitalWrite(relay, HIGH);    // Make sure door is locked
  servo1.write(fecha);


  //Protocol Configuration
  Serial.begin(9600);  // Initialize serial communications with PC
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware

  //If you set Antenna Gain to Max it will increase reading distance
  //mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println(F("Access Control Example v0.1"));   // For debugging purposes
  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details

  //Wipe Code - If the Button (wipeB) Pressed while setup run (powered on) it wipes EEPROM
  if (digitalRead(wipeB) == LOW) {  // when button pressed pin should get low, button connected to ground
    // Red Led stays on to inform user we are going to wipe
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 10 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
    if (buttonState == true && digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
      Serial.println(F("Starting Wiping EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
     
    }
    else {
      Serial.println(F("Wiping Cancelled")); // Show some feedback that the wipe button did not pressed for 15 seconds
      
    }
  }
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
      
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    Serial.println(F("Master Card Defined"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds





  
  pinMode(redled, OUTPUT);
  pinMode(yellowled, OUTPUT);
  pinMode(greenled, OUTPUT);
  digitalWrite(redled,HIGH);
  digitalWrite(yellowled,HIGH);
  digitalWrite(greenled,HIGH);



   lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  
readVcc();
  if (readVcc() >= 3800)
  {
    lcd.setCursor(0, 0);
    lcd.print("voltagem:");
    lcd.print(readVcc() / 1000.00);
    lcd.print("v");
    lcd.setCursor(0, 1);
    lcd.print("voltagem ok!");
    delay(3000);
  }

  if (readVcc() <= 3800)
  {
    lcd.setCursor(0, 0);
    lcd.print("voltagem:");
    lcd.print(readVcc() / 1000.00);
    lcd.print("v");
    lcd.setCursor(0, 1);
    lcd.print("trocar bateria!");
    delay(1000000);
  }

  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bomba ativa!");
  lcd.setCursor(0, 1);
  lcd.print("Enter Code:");
  while (keycount < 4)
  {
    lcd.setCursor(keycount + 12, 1);
    lcd.blink();
    char armcode = keypad.getKey();
    armcode == NO_KEY;
    if (armcode != NO_KEY)
    {
      if ((armcode != '*') && (armcode != '#'))
      {
        lcd.print(armcode);
        tone(3, 5000, 100);
        password[keycount] = armcode;
        keycount++;
      }
    }
  }

  if (keycount == 4)
  {
    delay(500);
    lcd.noBlink();
    lcd.clear();
    lcd.home();
    lcd.print("Senha de desarme");
    lcd.setCursor(6, 1);
    lcd.print(password[0]);
    lcd.print(password[1]);
    lcd.print(password[2]);
    lcd.print(password[3]);
    delay(3000);
    lcd.clear();
  }
  lcd.setCursor(0, 0);
  lcd.print("Timer: HH:MM:SS");
  lcd.setCursor(0, 1);
  lcd.print("SET:   :  :");
  keycount = 5;

  while (keycount == 5)
  {
    char hourstens = keypad.getKey();
    lcd.setCursor(5, 1);
    lcd.blink();
    if (hourstens >= '0' && hourstens <= '9')
    {
      hourstenscode = hourstens - '0';
      tone(3, 5000, 100);
      lcd.print(hourstens);
      keycount++;
    }
  }

  while (keycount == 6)
  {
    char hoursones = keypad.getKey();
    lcd.setCursor(6, 1);
    lcd.blink();
    if (hoursones >= '0' && hoursones <= '9')
    {
      hoursonescode = hoursones - '0';
      tone(3, 5000, 100);
      lcd.print(hoursones);
      keycount++;
    }
  }

  while (keycount == 7)
  {
    char mintens = keypad.getKey();
    lcd.setCursor(8, 1);
    lcd.blink();
    if (mintens >= '0' && mintens <= '9')
    {
      mintenscode = mintens - '0';
      tone(3, 5000, 100);
      lcd.print(mintens);
      keycount++;
    }
  }

  while (keycount == 8)
  {
    char minones = keypad.getKey();
    lcd.setCursor(9, 1);
    lcd.blink();
    if (minones >= '0' && minones <= '9')
    {
      minonescode = minones - '0';
      tone(3, 5000, 100);
      lcd.print(minones);
      keycount++;
    }
  }

  while (keycount == 9)
  {
    char sectens = keypad.getKey();
    lcd.setCursor(11, 1);
    lcd.blink();
    if (sectens >= '0' && sectens <= '9')
    {
      sectenscode = sectens - '0';
      tone(3, 5000, 100);
      lcd.print(sectens);
      keycount = 10;
    }
  }

  while (keycount == 10)
  {
    char secones = keypad.getKey();
    lcd.setCursor(12, 1);
    lcd.blink();
    if (secones >= '0' && secones <= '9')
    {
      seconescode = secones - '0';
      tone(3, 5000, 100);
      lcd.print(secones);
      keycount = 11;
    }
  }

  if (keycount == 11);
  {
    Hours = (hourstenscode * 10) + hoursonescode;
    Minutes = (mintenscode * 10) + minonescode;
    Seconds = (sectenscode * 10) + seconescode;
    delay(100);
    lcd.noBlink();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Timer em:");
    if (Hours >= 10)
    {
      lcd.setCursor (7, 1);
      lcd.print (Hours);
    }
    if (Hours < 10)
    {
      lcd.setCursor (7, 1);
      lcd.write ("0");
      lcd.setCursor (8, 1);
      lcd.print (Hours);
    }
    lcd.print (":");

    if (Minutes >= 10)
    {
      lcd.setCursor (10, 1);
      lcd.print (Minutes);
    }
    if (Minutes < 10)
    {
      lcd.setCursor (10, 1);
      lcd.write ("0");
      lcd.setCursor (11, 1);
      lcd.print (Minutes);
    }
    lcd.print (":");

    if (Seconds >= 10)
    {
      lcd.setCursor (13, 1);
      lcd.print (Seconds);
    }

    if (Seconds < 10)
    {
      lcd.setCursor (13, 1);
      lcd.write ("0");
      lcd.setCursor (14, 1);
      lcd.print (Seconds);
    }
    delay(3000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Aperte S12 para");
    lcd.setCursor (0, 1);
    lcd.print(" armar");
    delay (50);
    keycount = 12;
  }

  while (keycount == 12)
  {
    char armkey = keypad.getKey();

    if (armkey == '#')
    {
      tone(3, 5000, 100);
      delay(50);
      tone(3, 0, 100);
      delay(50);
      tone(3, 5000, 100);
      delay(50);
      tone(3, 0, 100);
      delay(50);
      tone(3, 5000, 100);
      delay(50);
      tone(3, 0, 100);
      lcd.clear();
      lcd.print ("Bomba Armada!");
      lcd.setCursor(0, 1);
      lcd.print("Timer Iniciado");
      delay(3000);
      lcd.clear();
      keycount = 0;
    }
  }


  
 
}
void loop()
{


  
  do {
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    // When device is in use if wipe button pressed for 10 seconds initialize Master Card wiping
    if (digitalRead(wipeB) == LOW) { // Check if button is pressed
      // Visualize normal operation is iterrupted by pressing wipe button Red is like more Warning to user
      
      // Give some feedback
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
      if (buttonState == true && digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
        EEPROM.write(1, 0);                  // Reset Magic Number.
        Serial.println(F("Master Card Erased from device"));
        Serial.println(F("Please reset to re-program Master Card"));
        while (1);
      }
      Serial.println(F("Master Card Erase Cancelled"));
    }
    if (programMode) {
      cycleLeds();              // Program Mode cycles through Red Green Blue waiting to read a new card
    }
    else {
      normalModeOn();     // Normal mode, blue Power LED is on, all others are off
    }
  }
  while (!successRead);   //the program will not go further while you are not getting a successful read
  if (programMode) {
    if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I do not know this PICC, adding..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  }
  else {
    if ( isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      Serial.println(F("Hello Master - Entered Program Mode"));
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan Master Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, You shall pass"));

        String idCard = "";
        for ( uint8_t i = 0; i < 4; i++) {  //
          readCard[i] = mfrc522.uid.uidByte[i];
           idCard = (readCard[i]);

           
        }

        Serial.print(idCard);
        if(idCard.equals("16")){
          //Serial.println("Entrou!!!!!!!!!!!");
delay(1000);          
         funcReset();
          
        }else{
        granted(300);         // Open the door lock for 300 ms           
        }

        
                  
        
        
      }
      else {      // If not, show that the ID was not valid
        Serial.println(F("You shall not pass"));
        denied();
      }
    }
  }


  
  timer();
  char disarmcode = keypad.getKey();

  if (disarmcode == '*')
  {
    tone(3, 5000, 100);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Senha: ");

    while (keycount < 4)
    {
      timer();

      char disarmcode = keypad.getKey();
      if (disarmcode == '#')
      {
        tone(3, 5000, 100);
        keycount = 0;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Senha: ");
      }
      else if (disarmcode != NO_KEY)
      {
        lcd.setCursor(keycount + 7, 0);
        lcd.blink();
        lcd.print(disarmcode);
        entered[keycount] = disarmcode;
        keycount++;
        tone(3, 5000, 100);
        delay(100);
        lcd.noBlink();
        lcd.setCursor(keycount + 6, 0);
        lcd.print("*");
        lcd.setCursor(keycount + 7, 0);
        lcd.blink();
      }
    }

    if (keycount == 4)
    {
      if (entered[0] == password[0] && entered[1] == password[1] && entered[2] == password[2] && entered[3] == password[3])
      {
        lcd.noBlink();
        lcd.clear();
        lcd.home();
        lcd.print("Bomba Desarmada!");
        lcd.setCursor(0, 1);
        lcd.print("Muito bem! :D");
        keycount = 0;
        digitalWrite(greenled, HIGH);
        delay(15000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Bomba Desarmada!");
        lcd.setCursor(0, 1);
        lcd.print("Reset a Bomba");
        delay(1000000);
      }
      else
      {
        lcd.noBlink();
        lcd.clear();
        lcd.home();
        lcd.print("Senha Errada! :(");
        trycount++;

        if (Hours > 0)
        {
          Hours = Hours / 2;
        }

        if (Minutes > 0)
        {
          Minutes = Minutes / 2;
        }
        if (Seconds > 0)
        {
          Seconds = Seconds / 2;
        }
        if (trycount == 2)
        {
          interval = interval / 10;
        }
        if (trycount == 3)
        {
          Minutes = Minutes - 59;
          Hours = Hours - 59;
          Seconds = Seconds - 59;
        }
        delay(1000);
        keycount = 0;
      }
    }
  }



  
}

void timer()
{
  Serial.print(Seconds);
  Serial.println();

  if (Hours <= 0)
  {
    if ( Minutes < 0 )
    {
      lcd.noBlink();
      lcd.clear();
      lcd.home();
      lcd.print("A bomba...");
      lcd.setCursor (0, 1);
      lcd.print("Explodiu!!");
 int contador = 0;
      while (Minutes < 0)
      {
        if(contador <= 20){
        digitalWrite(redled, LOW);
        tone(3, 7000, 100);
        delay(100);
        digitalWrite(redled, HIGH);
        tone(3, 7000, 100);
        delay(100);
        digitalWrite(yellowled, LOW);
        tone(3, 7000, 100);
        delay(100);
        digitalWrite(yellowled, HIGH);
        tone(3, 7000, 100);
        delay(100);
        digitalWrite(greenled, LOW);
        tone(3, 7000, 100);
        delay(100);
        digitalWrite(greenled, HIGH);
        tone(3, 7000, 100);
        delay(100);
        contador++;
        }else{

          digitalWrite(redled, LOW);
        
        delay(100);
        digitalWrite(redled, HIGH);
        
        delay(100);
        digitalWrite(yellowled, LOW);
        
        delay(100);
        digitalWrite(yellowled, HIGH);
        
        delay(100);
        digitalWrite(greenled, LOW);
        
        delay(100);
        digitalWrite(greenled, HIGH);
        
        delay(100);
        }


        
      }
    }
  }

  lcd.setCursor (0, 1);
  lcd.print ("Timer:");

  if (Hours >= 10)
  {
    lcd.setCursor (7, 1);
    lcd.print (Hours);
  }
  if (Hours < 10)
  {
    lcd.setCursor (7, 1);
    lcd.write ("0");
    lcd.setCursor (8, 1);
    lcd.print (Hours);
  }
  lcd.print (":");

  if (Minutes >= 10)
  {
    lcd.setCursor (10, 1);
    lcd.print (Minutes);
  }
  if (Minutes < 10)
  {
    lcd.setCursor (10, 1);
    lcd.write ("0");
    lcd.setCursor (11, 1);
    lcd.print (Minutes);
  }
  lcd.print (":");

  if (Seconds >= 10)
  {
    lcd.setCursor (13, 1);
    lcd.print (Seconds);
  }

  if (Seconds < 10)
  {
    lcd.setCursor (13, 1);
    lcd.write ("0");
    lcd.setCursor (14, 1);
    lcd.print (Seconds);
  }

  if (Hours < 0)
  {
    Hours = 0;
  }

  if (Minutes < 0)
  {
    Hours --;
    Minutes = 59;
  }

  if (Seconds < 1)
  {
    Minutes --;
    Seconds = 59;
  }

  if (Seconds > 0)
  {
    unsigned long currentMillis = millis();

    if (currentMillis - secMillis > interval)
    {
      tone(3, 7000, 50);
      secMillis = currentMillis;
      Seconds --;
      digitalWrite(yellowled, LOW);
      digitalWrite(redled, LOW);
      digitalWrite(greenled, LOW);
      delay(25);
      digitalWrite(yellowled, HIGH);
      delay(25);
    }
  }
}


/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted ( uint16_t setDelay) {
  
  //digitalWrite(relay, LOW);     // Unlock door!
  
  delay(setDelay);          // Hold door lock open for given seconds
  //digitalWrite(relay, HIGH);     // Relock door

  if(estadoServoAberto){
  servo1.write(fecha);
  estadoServoAberto = false;
  }else{
  servo1.write(abre);
  estadoServoAberto = true;
  }
  delay(1000);            // Hold green LED on for a second
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  
  delay(1000);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted

    while (true); // do not go further
  }
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {

}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
 
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

bool monitorWipeButton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(wipeB) != LOW)
        return false;
    }
  }
  return true;
}
