
/*
  Prosjektgruppe 17, Datateknikk_vår21 : HYBELHUS
  Styreprogram for individuell, Soverom_styring, ESP32

  Aktuatorer: steppermotor(vindu), DC motor(takvifte), lysstyring og varmestyring
  der alle aktuatorer styres via 2 stk L293DH-Bridge kontrollere.
  TFT LCD display(80x160 RGB) med ST7735 Driver.

  Sensorer: Temp/Bar/Hum, CO2, LUX og PIR(ir-movement), som alle tilkobles
  og leses via I2C kommunikkasjonsprotokollen.

  Modulen Leser av sensorer og sender dette til CoT. Leser data fra CoT og styrer
  aktuatorer. Viser temperaturdata etc på skjerm. Funksjoner kan også overstyres fra
  Controller på funksjoner som takvifte, lysstyrke og varme.
*/
//Libraries
#include <CircusESP32Lib.h>
#include <BH1750.h>
#include <analogWrite.h>
#include <Wire.h> 
#include <Adafruit_AHTX0.h>
#include <Stepper.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>




//SPI pin configuration for "0,96 inch IPS TFT LCD display(80x160 Color)" 
//used with ESP32 Devkit V4. SCL(Clock on pin 18) and SDA(MOSI on pin 23)
  #define TFT_CS         5  // Chip select Connected to IO00 (ved flere display på samme SPI-Buss)
  #define TFT_RST        4  // Display reset connected to IO04
  #define TFT_DC         2  // Display data/command select connected to IO02
  #define TFT_BACKLIGHT  0  // Display backlight pin. LOW = off. connected to IO05
  #define TFT_MOSI       23 // Data inn
  #define TFT_SCLK       18 // Klokkefrekvens

  #define Fontsize       2  // 0-4 setter fontstørrelse for display ved oppstart(default str.)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); //Displayobjekt.
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

//Pin defines H-Bridge1, Steppermotor
#define H_Bridge1_EN 19 // Enables all four outputs on H-Bridge1
#define H_Bridge1_A 32 //Steppermotor inngang A / Utgang A RØD wire
#define H_Bridge1_B 33 //Steppermotor inngang B / Utgang B ORANGE wire
#define H_Bridge1_C 12 //Steppermotor inngang C / Utgang C ROSA wire
#define H_Bridge1_D 13 //Steppermotor inngang D / Utgang D BLÅ wire

//Pin defines H-Bridge2 Lightdimming, Heatercontroll and DC-motor
#define H_Bridge2_EN 15 // Enables all four outputs om H-Bridge2
#define H_Bridge2_A 27 // Lysdimmer(White LED)
#define H_Bridge2_B 14 // Varmestyring(Red LED)
#define H_Bridge2_C 25 // DC motor FVD
#define H_Bridge2_D 26 // DC motor BVD

//Pin defines other
#define PIRpin 35 // Bevegelsessensor tilkoblet

//Enumerator for main loop statemaskina "SoveromMain"
enum mainstates {Mainloop, State2, State3, State4, State5, State6};
mainstates mainstate = Mainloop;

//setter opp kontakt mellom CoT og ESP32
char ssid[] = "Telenor7233lut"; // Navn på ruter
char password[] = "Tappingen2Sponsorkontrakt0"; //Passord på ruter
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDg5In0.BM73ysSfKKQDToNknonEI2KzO7oKwHQjLKUKIz9omyg"; //Skriv token her
char server[] = "www.circusofthings.com"; //Her er serveren
char order_key_LED[]= "18663"; //nøkkel-informasjon til lysstyring
char order_key_LUX[] = "13426"; //nøkkel-informasjon til lux-måler
char order_key_temp[] = "28604";//nøkkel-informasjon til temperatur-måler
char order_key_hum[] = "31117";//nøkkel.inforamasjon til humidity-måler
char order_key_PIR[] = "20999";//nøkkel.inforamasjon til PIR deteksjon
char order_key_onsketTemp[] = "25901";//nøkkel.inforamasjon til ønsket temp
char order_key_takvifte[] = "17916";//nøkkel.inforamasjon til takvifte
char order_key_lysautomatisk[] = "10503";//nøkkel.inforamasjon til lysautomatisk


//Configurerer CircusESP32 og oppretter objekter for sensorene
CircusESP32Lib circusESP32(server,ssid,password);
BH1750 lightMeter; //Objektet for LUX-måler
Adafruit_AHTX0 aht;  //Objektet for Temperatursensor

//Variabler og konstanter spesifikk for dette soverommet
int soveromID = 1;  //Fra 1 til 6 -samme som controlleren/personen for dette soverommet


//DEBUG-bryter ( 0 av, 1 på)
int debug = 0;

//Variabler simulering av varmedimming
int varmeStyring;
int onsketTemp = 22; //setter en startsverdi til 22 grader
int bright = 0;
int step = 1;

//Variabler for lux måling
float lux;
//variabler for lysstyring
int automatisk = 0;
float lux_avlest;
int LED_state = 0;

  
//variabler for vindu
//starter med vinduet lukket igjen
bool lukket = true;


//variabler for temperatur og humidity
float temperaturmaaling;
float humiditymaaling;

//variabler for PIR
bool PersonInRoom = LOW;
bool isDetected = LOW;

//variabler for tid
long previousMillis = 0; 
long interval = 60000;  //intervall på 1 min

//variabler for takvifte
int takvifte = 0;

//variabler for motor
const float antall_grader = 360/80;
const float STEPS_PER_REV = 32;
const float GEAR_RED  =64;
const float STEPS_PER_OUT_REV = STEPS_PER_REV * GEAR_RED;
int StepsRequired;

//Stepper steppermotor(STEPS_PER_REV,32,12,33,13); //Objektet for Steppermotor
Stepper steppermotor(STEPS_PER_REV, H_Bridge1_A, H_Bridge1_B, H_Bridge1_C, H_Bridge1_D); //Steppermotorobjekt.



void sjekkLux(){
 float lux = lightMeter.readLightLevel(); 
 Serial.print("Light: "); 
 Serial.print(lux); 
 Serial.println(" lx"); 
 delay(1000); 
 //skriver lux verdien over til COT
 circusESP32.write(order_key_LUX,lux,token);

}

void lyspaa_manuelt(){
  int LED_state = circusESP32.read(order_key_LED,token); //går fra 0-255 
  analogWrite(H_Bridge2_A, LED_state);
  delay(500);
  Serial.println(LED_state);
}

void lyspaa_automatisk(){
  float lux_avlest = circusESP32.read(order_key_LUX,token);
  if(lux_avlest > 1000){//hvis vi har mer enn 1000 lux skrur vi av lyset
    int LED_state = 0;
    analogWrite(H_Bridge2_A,LED_state);
  }
  else if (lux_avlest > 500 && lux < 1000){
    int LED_state = 100;
    analogWrite(H_Bridge2_A,LED_state);
  }
  else if (lux_avlest > 0 && lux < 500){
    int LED_state = 200;
    analogWrite(H_Bridge2_A,LED_state);
 }
}


void temp(){
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  temperaturmaaling = temp.temperature;
  humiditymaaling = humidity.relative_humidity;
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
  //sender verdier 
  circusESP32.write(order_key_temp,temp.temperature,token);
  circusESP32.write(order_key_hum,humidity.relative_humidity,token);
  delay(100);
}

void motorstep(){
  //hvis temperaturen er for høy og vinduet er lukket igjen, åpner vi det 80 grader
  if(temperaturmaaling > onsketTemp && lukket == true){
    //roterer motor 80grader
    StepsRequired = STEPS_PER_OUT_REV / antall_grader;
    steppermotor.setSpeed(100);
    steppermotor.step(StepsRequired);
    delay(100); 
    lukket = false;
  }
    //Kanskje jeg må stoppe motor også
    // steppermotor.setSpeed(0);
  // hvis vinduet er åpent og det blir for kaldt lukker vi igjen vinduet.  
  else if(temperaturmaaling < onsketTemp && lukket == false){
  //roter motor tilbake 80grader
    StepsRequired = - STEPS_PER_OUT_REV / antall_grader;
    steppermotor.setSpeed(100);
    steppermotor.step(StepsRequired);
    delay(100);
    lukket = true;
  //Kanskje jeg må stoppe motor også
  // steppermotor.setSpeed(0);
  }
}


void motordc(){
  //#define H_Bridge2_C 25 // DC motor FVD
  //#define H_Bridge2_D 26 // DC motor BVD
  //Setter på vifte når person er i rommet eller via COT
  digitalWrite(H_Bridge2_D, LOW);
  takvifte = circusESP32.read(order_key_takvifte,token);
  if(PersonInRoom || takvifte == 1){
    //setter motor verdi til 100, slik at motor går, men ikke for fort
    analogWrite(H_Bridge2_C,100);
    if(debug){
      Serial.println("Motor skal starte nå");
    }
  }else if(!PersonInRoom || takvifte == 0){
    //skrur av motor igjen
    analogWrite(H_Bridge2_C,0);
    if(debug){
      Serial.println("nå skal motor stoppe");
    }
  }
  

  
  
    
}


void LCDskjerm(){
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10,10);
  tft.print("Temp:");
  tft.print(temperaturmaaling);
  tft.println("C");
  tft.print("hum: ");
  tft.print(humiditymaaling);
  tft.println("%");
  delay(100);
  
}

void tempregulering(){
  //onsketTemp = 22 //bare setter en fin temperatur hvis man ikke ønsker COT
  onsketTemp = circusESP32.read(order_key_onsketTemp,token); //Setter ønsket temperatur for rommet fra COT
  if(debug){
    Serial.println(onsketTemp);   
  }
  if(PersonInRoom){ // regulerer bare når det er folk i rommet
    if(temperaturmaaling < onsketTemp) { //hvis temperatur er mindre enn 22
      analogWrite(H_Bridge2_B, 255); //Setter lyset("varmeovnen") på maks
    }else if (temperaturmaaling >= onsketTemp){
      analogWrite(H_Bridge2_B,0); //nådd ønsket temperatur, skrur av lyset("varmeovnen")
    }
  }else if(!PersonInRoom){
     analogWrite(H_Bridge2_B,0);//Hvis det ikke er folk i rommmet skal varmen slåes av...  
  }
}
void PIRsensor() {
  //Sjekker først om det er noen i rommet.
  bool isDetected = digitalRead(PIRpin);
  if(isDetected){
    PersonInRoom = true;
    analogWrite(H_Bridge2_A,250); //skrur på lyset
  }
  unsigned long currentMillis = millis(); //starter tiden
  //Venter 1 min før den sjekker om den er av.
  if(currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
    if(isDetected){ 
      PersonInRoom = true; //viser at det er noen i rommet, kan bruke denne igjen i andre funksjoner
      Serial.println("Det er noen i rommet");
      analogWrite(H_Bridge2_A,250); //skrur på lyset
      
    }else if(!isDetected){
        PersonInRoom = false;
        Serial.println("ingen i rommet");
        analogWrite(H_Bridge2_A,0); //skrur av lyset       
     } 
   }
   delay(100);
  }



void setup() {
  Wire.begin();
  circusESP32.begin(); //initialiserer oppkoblign mot CoT
  lightMeter.begin(); //initialiserer BH1750 -> sjekker LUX
  Serial.begin(115200); //Setter datahastigheten mellom ESP32 og PC'
  
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
  
  //SCREEN INIT
  pinMode(TFT_BACKLIGHT, OUTPUT);    // Output for controlling display ON/OFF
  digitalWrite(TFT_BACKLIGHT, HIGH); // HIGH=display ON, LOW=display OFF
  tft.initR(INITR_MINI160x80);       // Init ST7735 80x160 TFT LCD RGB screen
  tft.setRotation(1);                // Orientating screen for correct wieving   
  tft.invertDisplay(true);           // Used screen is initial inverted, so we must invert back to normal.
  tft.fillScreen(ST77XX_BLACK);      // Setting backgroundcolor at startup(startuplogo here instead??)
  
  //


  //H-Bridges INIT Setting all ESP32 Pins connected to H-Bridges to OUTPUT. 
  //Use analogWrite(pin_name, val) to set motorlevel(0-255), or digitalwrite(pin_name, HIGH)
  //to enable all outputs  on a H-Bridge.
  pinMode(H_Bridge1_EN, OUTPUT);
  pinMode(H_Bridge2_EN, OUTPUT);
  pinMode(H_Bridge1_A, OUTPUT);
  pinMode(H_Bridge1_B, OUTPUT);
  pinMode(H_Bridge1_C, OUTPUT);
  pinMode(H_Bridge1_D, OUTPUT);
  pinMode(H_Bridge2_A, OUTPUT);
  pinMode(H_Bridge2_B, OUTPUT);
  pinMode(H_Bridge2_C, OUTPUT);
  pinMode(H_Bridge2_D, OUTPUT);
  digitalWrite(H_Bridge1_EN, HIGH);
  digitalWrite(H_Bridge2_EN, HIGH);
  pinMode(PIRpin,INPUT); 


} 

void loop() {
  /*
    Mainstate er Statemaskina som styrer programflyten i programmet til soveromsmodulen.
    All kode som skal kjøre hele tiden når ingenting annet skjer, skal inn her.
    også timere som styrer kaller på andre states/case eller enkeltfunksjoner legges inn her
    Ubrukte case kan godt få stå, og legg gjerne inn en forklarende tekst om hva de er tenkt brukt til/satt av til
    i videre utvikling av funksjonalitet for soveromsmodulen (det vi ikke rekker å implementere, men har planlagt)
  */
  switch (mainstate) { //Statemaskina som kontrollerer programflyten i styringsenheten
    case Mainloop: //Det som skal gjøres hele tiden skal inn her
    
    //PIRsensor(); sjekkes heletiden, må være inni main
    
    //LCDskjerm(); kan også være i main

    
    //lyspaa();
    //endrer lysstyrke enten fra verdien til cot eller fra målt lysstyrke fra omgvielsene
    //må også sette på om det skal gjøres automatisk eller manuelt fra COT


    
    
      break;

    case State2:
      motordc(); //skrus på etter noen kommer i rommet
      temp(); //tar temperatur og sender det til COT
      sjekkLux(); //sjekker luxverdi fra omgivelsen
      if (debug) {
        Serial.println("State2 er kjørt");
      }
      mainstate = State3; //Sender programmet til neste State
      break;

    case State3:
      tempregulering(); //regulerer temperaturen. Bruker Cot ønsket verdi og faktisk målt verdi
      motorstep(); // åpner/lukker vindu hvis temp. er for høy eller lav
      automatisk = circusESP32.read(order_key_lysautomatisk,token); //leser av fra cot om vi skal styre lyset automtisk eller manuelt,velger hvilken state man skal til
      if(automatisk == 1){
          mainstate = State4;
      
      }else if(automatisk == 0){
          mainstate = State5;
      }else{
        mainstate = Mainloop;
      }        
      if (debug) {
        Serial.println("State3 er kjørt");
      }
      break;

    case State4:
      lyspaa_automatisk();
      if (debug) {
        Serial.println("State4 er kjørt");
      }
      mainstate = Mainloop; //Sender programmet tilbake til Mainstate
      break;

    case State5:
     lyspaa_manuelt();
      if (debug) {
        Serial.println("State5 er kjørt");
      }
      mainstate = Mainloop; //Sender programmet tilbake til Mainstate
      break;

    case State6:
      //Last change to do it...
      if (debug) {
        Serial.println("State6 er kjørt");
      }
      mainstate = Mainloop; //Sender programmet tilbake til Mainstate
      break;
  } //END mainstate
} //END LOOP HOVEDPROGRAM
//**** Alle funksjoner brukt i programmet kommer under her****

  
