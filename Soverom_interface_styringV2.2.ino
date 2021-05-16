
/*
  Prosjektgruppe 17, Datateknikk_vår21 : HYBELHUS
  Soverom_intercace_styringV2.0 for ESP32 Devkit Ver.4

  Styreprogram for interfacet til soverommene i hybelhuset. Settes til ønsket soverom med "beboerNum"
  variablen ved installering i huset. 

  Aktuatorer: steppermotor(vindu), DC motor(takvifte), lysstyring og varmestyring
  der motorer og lys/varme styres via 2 stk L293DH-Bridge kontrollere.
  TFT LCD display(80x160 RGB) med ST7735 Driver.

  Sensorer: Temp/Hum, CO2 og LUX, som tilkobles og leses via I2C kommunikkasjonsprotokollen.
  PIR(ir-movement) sensor, som gir HIGH output til pinne 35 når person er i rommet.

  Modulen Leser av sensorer og sender dette til CoT. Leser data fra CoT og styrer
  aktuatorer avhengig av om person er i rommet eller ikke. Viser temperaturdata etc på skjerm.
  Funksjoner kan også overstyres fra Controller på funksjoner som takvifte, lysstyrke, ønsket 
  temperatur og manuel åpning/lukking av vindu.
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

int beboerNum = 1; //Variabel som velger beboer/soverom for denne enheten, verdi: 1-6



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

//Pin defines H-Bridge1, Steppermotor (28BYJ-48 5VDC)
#define H_Bridge1_EN 19 // Enables all four outputs on H-Bridge1
#define H_Bridge1_A 32 //Steppermotor inngang A / Utgang A GUL wire
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
#define PIRpin 35 // Bevegelsessensor tilkoblet pin 35


//Enumerator for main loop statemaskina "SoveromMain"
enum mainstates {Mainloop, State2, State3, State4, State5, State6};
mainstates mainstate = Mainloop;

//setter opp kontakt mellom CoT og ESP32
char ssid[] = "Telenor7233lut"; // Navn på ruter
char password[] = "Tappingen2Sponsorkontrakt0"; //Passord på ruter
char server[] = "www.circusofthings.com"; //Her er serveren

//Tokens for soverommene. Rekkefølgen i arrays er soverom 1 til 6 (0-5 i array-posisjoner)
char *array_tokens[] = {"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDg5In0.BM73ysSfKKQDToNknonEI2KzO7oKwHQjLKUKIz9omyg",
"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDkwIn0.-KW0hqJRmbC1W61EJNkv1HjI47RGzg6G30KzhQlGYbw",
"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDkxIn0.aSusWa3j_LOPHIdmSh8vZ5GCzTTVaVOzoqqU_QfyCMw",
"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDkyIn0.Wv9KcGzlGCMTB0cXM9t0s0jX8g0dmyG1CCzmvYE5V4k",
"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDkzIn0.yQ3fNM61ecxH4U1f01kW9D8hUm5pbTlSNTPMMI9GKDc",
"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk0In0.yFXNG1sNpLgb30l7jGx7XFwLgxchWOMOQpHcwjcAZQE"};

//Keys for signalene til soverommene. Rekkefølgen i arrays er soverom 1 til 6 (0-5 i array-posisjoner)
char *array_key_LED[]= {"18663","19296","30697","9926","12023","26810"}; //nøkkel-informasjon til lysstyrke 0
char *array_key_LUX[] = {"13426","8977","2286","18680","17797","26308"}; //nøkkel-informasjon til lux-måler 1
char *array_key_temp[] = {"28604","26509","12095","19020","30379","32180"}; //nøkkel-informasjon til temperatur-måler 2
char *array_key_hum[] = {"31117","20272","30736","16555","18177","8618"}; //nøkkel.informasjon til humidity-måler 3
char *array_key_CO2[] = {"30290","9148","12201","25687","6944","15591"};  //nøkkel.informasjon til CO2 sensor 4
char *array_key_PIR[] = {"20999","18540","24524","481","5485","12822"}; //nøkkel.informasjon til PIR deteksjon 5
char *array_key_onsketTemp[] = {"25901","18399","19772","19704","10205","22658"}; //nøkkel.informasjon til ønsket temp 6
char *array_key_takvifte[] = {"17916","11119","16288","13744","2396","17719"};   //nøkkel.informasjon til takvifte 7
char *array_key_lysautomatisk[] = {"10503","17268","15673","17304","9231","14540"}; //nøkkel.informasjon til av/på for automatisk styring av lys 8
char *array_key_Vindu[] = {"15838","32767","30604","3094","1703","32123"};  //nøkkel.informasjon til Vindusmotor/Stepper Åpent/Lukket 9

/* Tabell for keys til alle signal for soverommene (hentet fra XL ark1)
    0     1     2     3     4     5     6     7     8     9 <-----Signal Nr
 1  18663	13426	28604	31117	30290	20999	25901	17916	10503	15838
 2  19296	8977	26509	20272	9148	18540	18399	11119	17268	32767
 3  30697	2286	12095	30736	12201	24524	19772	16288	15673	30604
 4  9926	18680	19020	16555	25687	481	  19704	13744	17304	3094
 5  12023	17797	30379	18177	6944	5485	10205	2396	9231	1703
 6  26810	26308	32180	8618	15591	12822	22658	17719	14540	32123
 ^
 |
 Soverom-Nr
*/


//Definerer variabler(array) for Keys og token
char *token = {"xxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}; //Skriv token her
char *order_key_LED = {"00000"}; //nøkkel-informasjon til lysstyring
char *order_key_LUX = {"00000"};  //nøkkel-informasjon til lux-måler
char *order_key_temp = {"00000"}; //nøkkel-informasjon til temperatur-måler
char *order_key_hum = {"00000"}; //nøkkel.informasjon til humidity-måler
char *order_key_CO2 = {"00000"};  //nøkkel-informasjon til CO2 sensor
char *order_key_PIR = {"00000"};  //nøkkel.informasjon til PIR deteksjon
char *order_key_onsketTemp = {"00000"}; //nøkkel.informasjon til ønsket temp
char *order_key_takvifte = {"00000"}; //nøkkel.informasjon til takvifte
char *order_key_lysautomatisk = {"00000"};  //nøkkel.informasjon til lysautomatisk
char *order_key_vindu = {"00000"};  //nøkkel.informasjon til Vindu status





//Configurerer CircusESP32 og oppretter objekter for sensorene
CircusESP32Lib circusESP32(server,ssid,password);
BH1750 lightMeter; //Objektet for LUX-måler
Adafruit_AHTX0 aht;  //Objektet for Temperatursensor

//DEBUG-bryter ( 0 av, 1 på)
bool DEBUG = 1;//Aktivere/deaktiverer seriellkommunikkasjonen og debug-meldinger til seriell-konsollen

//Variabler simulering av varmedimming
int varmeStyring; 
int onsketTemp = 22; //setter en startsverdi for ønsket temperatur til 22 grader


//Variabler for lux måling
float lux;//Variabel for lux-sensor verdi avlest
//variabler for lysstyring
int automatisk = 0;//Variabel for automatisk eller manuell styring av lys
float lux_avlest;//Variabel for luxverdi avlest fra CoT(ønsket lysnivå satt fra kontroller)
int LED_state = 0;//Variabel som brukes for å skrive lysstyrkeverdi(0-255) til H-bridge2_A inngangen som styrer lyset
int luxMax = 560; //Kalibrering av lyssensor i forhold til rommet. denne settes til målt verdi ved dagslys i rommet.
int luxMin = 50; //Kalibrering av lyssensor i forhold til rommet. Denne settes til målt luxverdi i "mørkt" rom.
  
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

//variabler for Steppermotor
const float antall_grader = 360/80;
const float STEPS_PER_REV = 32;
const float GEAR_RED  = 64;
const float STEPS_PER_OUT_REV = STEPS_PER_REV * GEAR_RED;
int StepsRequired;


Stepper steppermotor(STEPS_PER_OUT_REV, H_Bridge1_D, H_Bridge1_B, H_Bridge1_C, H_Bridge1_A); //Steppermotorobjekt.


void setup() {
  Wire.begin();
  if(DEBUG){Serial.begin(115200);} //Setter datahastigheten mellom ESP32 og PC'
  circusESP32.begin(); //initialiserer oppkoblign mot CoT
  lightMeter.begin(); //initialiserer BH1750 -> sjekker LUX
  
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10); //Stopper her om temperatursensor ikke initialiseres
  }
  Serial.println("AHT10 or AHT20 found");
  
  //SCREEN INIT
  pinMode(TFT_BACKLIGHT, OUTPUT);    // Output for controlling display ON/OFF
  digitalWrite(TFT_BACKLIGHT, HIGH); // HIGH=display ON, LOW=display OFF
  tft.initR(INITR_MINI160x80);       // Init ST7735 80x160 TFT LCD RGB screen
  tft.setRotation(1);                // Orientating screen for correct wieving   
  tft.invertDisplay(true);           // Used screen is initial inverted, so we must invert back to normal.
  tft.fillScreen(ST77XX_BLACK);      // Setting backgroundcolor at startup(startuplogo here instead??)


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

  steppermotor.setSpeed(10); // Steppermotorens rotasjonshastighet i runder pr min.

//Velger Key og token ut ifra valgte beboer(beboerNum verdi) for denne enheten
token = array_tokens[beboerNum - 1];//Skriv token her
order_key_LED = array_key_LED[beboerNum - 1];//nøkkel-informasjon til lysstyring
order_key_LUX = array_key_LUX[beboerNum - 1];//nøkkel-informasjon til lux-måler
order_key_temp = array_key_temp[beboerNum - 1];//nøkkel-informasjon til temperatur-måler
order_key_hum = array_key_hum[beboerNum - 1];//nøkkel.informasjon til humidity-måler
order_key_CO2 = array_key_CO2[beboerNum - 1];//nøkkel-informasjon til CO2 sensor
order_key_PIR = array_key_PIR[beboerNum - 1];//nøkkel.informasjon til PIR deteksjon
order_key_onsketTemp = array_key_onsketTemp[beboerNum - 1];//nøkkel.informasjon til ønsket temp
order_key_takvifte = array_key_takvifte[beboerNum - 1];//nøkkel.informasjon til takvifte
order_key_lysautomatisk = array_key_lysautomatisk[beboerNum - 1];//nøkkel.informasjon til lysautomatisk
order_key_vindu = array_key_Vindu[beboerNum - 1];//nøkkel.informasjon til Vindu status

if(DEBUG){Serial.println(token);Serial.println(order_key_LED);}
} //END SETUP


void loop() {
  /*
    Mainstate er Statemaskina som styrer programflyten i programmet til soveromsmodulen.
    All kode som skal kjøre hele tiden når ingenting annet skjer, skal inn i case: Mainloop.
    også timere som styrer kaller på andre states/case eller enkeltfunksjoner legges inn her
    Ubrukte case kan godt få stå, og legg gjerne inn en forklarende tekst om hva de er tenkt brukt til/satt av til
    i videre utvikling av funksjonalitet for soveromsmodulen (det vi ikke rekker å implementere, men har planlagt)
  */
  switch (mainstate) { //Statemaskina som kontrollerer programflyten i styringsenheten
    
    case Mainloop: //Det som skal gjøres hele tiden skal inn her

      PIRsensor(); //sjekker tilstedeværelse i soverommet
      LCDskjerm(); //Oppdaterer LCD-skjerm med miljødata
      mainstate = State2;//Gå til State2
          
    break;

    case State2:
      motordc(); //skrus på etter noen kommer i rommet
      temp(); //tar temperatur og sender det til COT
      sjekkLux(); //sjekker luxverdi fra omgivelsen
      if (DEBUG){Serial.println("State2 er kjørt");}
      mainstate = State3; //Sender programmet til State3
      
    break;

    case State3:
      tempregulering(); //regulerer temperaturen. Bruker Cot ønsket verdi og faktisk målt verdi
      motorstep(); // åpner/lukker vindu hvis temp. er for høy eller lav
      automatisk = circusESP32.read(order_key_lysautomatisk,token); //leser av fra cot om vi skal styre lyset automtisk eller manuelt,velger hvilken state man skal til
      if(automatisk == 1){
          mainstate = State4;//Går til State4
      
      }else if(automatisk == 0){
          mainstate = State5;//Går til State5
      }else{
        mainstate = Mainloop;//Sender programmet tilbake til Mainloop
      }        
      if (DEBUG){Serial.println("State3 er kjørt");}
      
    break;

    case State4:
      lyspaa_automatisk();
      if (DEBUG){Serial.println("State4 er kjørt");}
      mainstate = Mainloop; //Sender programmet tilbake til Mainloop
    
    break;

    case State5:
     lyspaa_manuelt();
      if (DEBUG){Serial.println("State5 er kjørt");}
      mainstate = Mainloop; //Sender programmet tilbake til Mainstate
      
    break;

    case State6:
      //For videre utvidelse av statemskina
      if (DEBUG){Serial.println("State6 er kjørt");}
      mainstate = Mainloop; //Sender programmet tilbake til Mainstate
    break;
  } //END mainstate
} //END LOOP HOVEDPROGRAM



//**** Alle funksjoner brukt i programmet kommer under her****

void PIRsensor() {
  //Sjekker først om det er noen i rommet.
  bool isDetected = digitalRead(PIRpin);
  Serial.println(isDetected);
  if(isDetected){
  PersonInRoom = true; //mulig person i rommet detektert
  //analogWrite(H_Bridge2_A,250); //skrur på lyset
  }
  unsigned long currentMillis = millis(); //starter tiden

  //Venter 1 min før den sjekker om den er av.
  if(currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
         
    if(isDetected){ //PersonInRoom = true; //Bekrefter at person faktisk er i rommet, og oppdaterer CoT på dette
      
      if(DEBUG){Serial.println("Det er noen i rommet");}
      circusESP32.write(order_key_PIR,PersonInRoom,token);
      //analogWrite(H_Bridge2_A,250); //skrur på lyset
    } 
    else if(!isDetected){//Ikke lengernoen person registrert i rommet, oppdaterer CoT på dette
      PersonInRoom = false;
      if(DEBUG){Serial.println("ingen i rommet");}
      circusESP32.write(order_key_PIR,PersonInRoom,token);
      //analogWrite(H_Bridge2_A,0); //skrur av lyset       
    } 
   }
   delay(100);
  }//END FUNCTION PIRsensor


 
void sjekkLux(){

 float lux = lightMeter.readLightLevel();//Leser Lux verdi fra sensor 
 if(DEBUG){Serial.print("Light: ");Serial.print(lux);Serial.println(" lx");}
 //delay(1000); 
 //skriver lux verdien over til COT
 circusESP32.write(order_key_LUX,lux,token);
}//END FUNCTION sjekkLux

void lyspaa_manuelt(){

  int LED_state = circusESP32.read(order_key_LED,token); //går fra 0-255 
  analogWrite(H_Bridge2_A, LED_state);
  if(DEBUG){Serial.println(LED_state);}
}//END FUNCTION lyspaa_manuelt

void lyspaa_automatisk2(){

  if(PersonInRoom){

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
  else if(!PersonInRoom){
    analogWrite(H_Bridge2_A, 0);
  }
}//END FUNCTION lyspaa_automatisk

//Denne gjør det samme som funksjonen lyspaa_automatisk, bare trinnløst med map-funksjonen
// samme kode Kan også tilpasses for trinnløs styring av motor/vifte og varme
void lyspaa_automatisk(){

  if(PersonInRoom){

    float lux_avlest = circusESP32.read(order_key_LUX,token);//Leser LUX verdi fra CoT
    int LED_state = map(lux_avlest, luxMax, luxMin, 0, 255);//Mapper fluxområdet 1000-200 til 0-255
    analogWrite(H_Bridge2_A,LED_state);// setter lysnivå i rommet
  }
  else if(!PersonInRoom){
    analogWrite(H_Bridge2_A, 0);
  }

}//END FUNCTION lyspaa_automatisk2



void temp(){//Leser av temperatursensor og oppdaterer signaler i CoT
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  temperaturmaaling = temp.temperature;
  humiditymaaling = humidity.relative_humidity;

  if(DEBUG){Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");}
  if(DEBUG){Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");}

  //sender verdier til CoT
  circusESP32.write(order_key_temp,temp.temperature,token);
  circusESP32.write(order_key_hum,humidity.relative_humidity,token);
}//END FUNCTION temp

void motorstep(){//Styrer åpning/lukking av vindu på soverom
 
 int vindusstyring = circusESP32.read(order_key_vindu,token);//Leser status for manuel vindusstyring på CoT
  
  //hvis temperaturen er for høy og vinduet er lukket igjen, åpner vi det for lufting
  if((temperaturmaaling > onsketTemp && lukket == true) || (vindusstyring == 1 && lukket == true)){
    //roterer motor 80grader
    StepsRequired = STEPS_PER_OUT_REV / antall_grader;
    steppermotor.step(StepsRequired);
    circusESP32.write(order_key_vindu,1,token);
    lukket = false;
  }
    // hvis vinduet er åpent og det blir for kaldt lukker vi igjen vinduet.  
  else if((temperaturmaaling < onsketTemp && lukket == false) || (vindusstyring == 0 && lukket == false)){
    //roter motor tilbake 80grader
    StepsRequired = - STEPS_PER_OUT_REV / antall_grader;
    steppermotor.step(StepsRequired);
    circusESP32.write(order_key_vindu,0,token);

    lukket = true;
  }
}//END FUNCTION motorstep


void motordc(){

 if(PersonInRoom){
    //Setter på vifte når person er i rommet eller via CoT
    digitalWrite(H_Bridge2_D, LOW);
    takvifte = circusESP32.read(order_key_takvifte,token);

    if(PersonInRoom && takvifte == 1){//setter motor på 25%   
      analogWrite(H_Bridge2_C, 90);
      if(DEBUG){Serial.println("Motor skal starte nå 25%");}
    }
    else if(PersonInRoom && takvifte == 2){ //setter motor på 50%
      analogWrite(H_Bridge2_C, 120);
      if(DEBUG){Serial.println("Motor skal starte nå 50%");}
    }
    else if(PersonInRoom && takvifte == 3){ //setter motor på 75%
      analogWrite(H_Bridge2_C,150);
      if(DEBUG){Serial.println("Motor skal starte nå 75%");}
    }
    else if(PersonInRoom && takvifte == 4){ //setter motor på 100%
      analogWrite(H_Bridge2_C,180);
      if(DEBUG){Serial.println("Motor skal starte nå 100%");}
    }
    else if(takvifte == 0){ //Motor slås av
      analogWrite(H_Bridge2_C, 0);
      if(DEBUG){Serial.println("Nå skal motor stoppe");}
    }
 }
  else if(!PersonInRoom){ //Vifte stopper når ingen er i rommet.
    analogWrite(H_Bridge2_C, 0);
      if(DEBUG){Serial.println("Nå skal motor stoppe");}
  }
}//END FUNCTION motordc


void LCDskjerm(){
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10,10);
  tft.print("Temp:");
  tft.print(temperaturmaaling);
  tft.println("C");
  tft.setCursor(10,50);
  tft.print("hum: ");
  tft.print(humiditymaaling);
  tft.println("%");
}//END FUNCTION LCDskjerm

void tempregulering(){
  //onsketTemp = 22 //bare setter en fin temperatur hvis man ikke ønsker COT
  onsketTemp = circusESP32.read(order_key_onsketTemp,token); //Setter ønsket temperatur for rommet fra COT
  if(DEBUG){Serial.println(onsketTemp);}

  if(PersonInRoom){ // regulerer bare når det er folk i rommet
    if(temperaturmaaling < onsketTemp) { //hvis temperatur er mindre enn 22
      analogWrite(H_Bridge2_B, 255); //Setter lyset("varmeovnen") på maks
    }
    else if (temperaturmaaling >= onsketTemp){
      analogWrite(H_Bridge2_B,0); //nådd ønsket temperatur, skrur av lyset("varmeovnen")
    }
  }
  else if(!PersonInRoom){
     analogWrite(H_Bridge2_B,0);//Hvis det ikke er folk i rommmet skal varmen slåes av...  
  }
}//END FUNCTION tempregulering
