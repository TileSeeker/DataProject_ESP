/*Prosjektgruppe 17, Datateknikk_vår21 : HYBELHUS
***Arthur, Emil, Johanne, Peter, Roger, Willhelm***

ControllerSoftwareV2.0

Styreprogram for individuell, bærbar Controller til IoT Hybelhus
Styreprogrammet settes til ønsket beboer/soverom med "beboerNum"
variablen ved installering i huset.

Implementert funksjonalitet:
  -Button-statemaskin med debounce og hurtigvalg ved å holde knappen inne.
  -Kommunikkasjon via WiFi til CircusOfThings(CoT) IoT-plattform.
  -Skjerm med menyvisning og pop-up menyer for ulike ulike funksjoner.
  -Alarm ved brann, med lyd, lys og melding på skjerm.
  -Ringeklokke og mottak av gjester på døra.
  -"Multitasking" av viktige funksjoner og sjekking av buttons.
  -Sleepmode for ESP32 når knapper og menyer ikke brukes.
  -Statussjekk mot CoT-registere hvert 30 sek, settes  med "lesCotIntervall" = 30000

  -Mulighet for manuell styring av lys, takvifte og lufting/vindu.
  -Instilling av ønsket temperatur mellom 12 og 25 Celsius grader

  Programmet har en egen statemaskin for registrering av knapper, som muliggjør samtidig kjøring av
  rutinene i default state i hovedstatemaskina mens knappetrykk sjekkes. Button og X, Y, Z variabler
  styrer både menyene og programflyt i mainloop'en. og i underfunksjoner. 


*/


//Libraries for TFT LCD display driver ST7789 over SPI
#include <CircusESP32Lib.h>   // Library for CircusOfThings API on ESP32
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>

//Vekker
#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  10       // hvor lenge ESP32 sover: "10" = 30 sek

//SPI pin configuration for "1,3 inch IPS TFT LCD display(240x240Color)" 
//used with ESP32 Devkit V4. SCL(Clock on pin 18) and SDA(MOSI on pin 23)

  #define TFT_CS        -1  // Not in USE/Not connected
  #define TFT_RST        4  // Display reset connected to IO04
  #define TFT_DC         2  // Display data/command select connected to IO02
  #define TFT_BACKLIGHT  5  // Display backlight pin. LOW = off. connected to IO05
  #define TFT_MOSI       23 // Data inn
  #define TFT_SCLK       18 // Klokkefrekvens

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); //Konstruerer displayobjekt "tft".

//Definerer tilkoblingspinner for trykknapper
#define pil_Opp 34
#define pil_Ned 33
#define pil_Ut 32
#define pil_Velg 35

//Definerer RGB LEDpins og Buzzerpin
#define redLED 25
#define greenLED 26
#define blueLED 27
#define buzzerAlarm 13

bool DEBUG = 0; // 0 her slår av Serial og debug_kommentarer til seriellmonitoren

//Setup for connecting with CoT
char ssid[] = "SPLITTERPINE";  //"CAT S60"; //"Atle Idar sin iPhone"; // //internet name
char password[] = "Proteus2X"; //"zug68guzX!"; // "txi896ehnfcxu"; //    //internet password

//Account token
char server[] = "www.circusofthings.com"; //the server

// beboer variabel (endrer denne for kvar kontroller)
const int beboerNum = 1;

CircusESP32Lib circusESP32(server, ssid, password); //Lager et objekt for CircusOfThings API'et


//Enumerator for main loop statemaskina "controllerMain"
enum mainstates{Mainloop, SjekkCoT, Booking, Hurtigbooking, Gjester, Info, Instillinger};
mainstates mainstate = Mainloop;

//Variabler for controllerMain statemaskina
unsigned long forrigeCotTimer = 0;
unsigned long CotTimer = 0;
const unsigned long lesCotIntervall = 20000;

//Enumerator for buttonTest statemaskina. 
enum buttonstates{Wait, Falling, Stable, Rising, Shortpress, Longpress};
buttonstates buttonstate = Wait; //Reset  //Initierer statemaskinvariabelen til buttonTest

// Variabler for Button-rutine og statemaskina som detekterer trykk på knappene

int Button = 0;                  //Holder styr på hvilken knapp som er trykket. resettes til 0 når menyvariabler er oppdatert.
int buttonStateValue = 0;        //Variabelen som leser og holder knappens status (HIGH or LOW)
int forrigebuttonStateValue = 0; //Holder knappens forrige status
int forrige_buttonstate = 0;     //Holder forrige state på buttonstate
unsigned long bounceDelay = 15;  //Setter tiden buttonTest venter på at buttonstateValue skal få en stabil LOW-verdi.
unsigned long holdDelay = 2000;  //Setter tiden for detektering av at knappen holdes inne (hurtigbooking)
unsigned long lowStarttid = 0;   //Teller for tidspunktet buttonStateValue går LOW 
unsigned long lowStopptid = 0;   //Teller for tidspunktet buttonStateValue går HIGH
unsigned long ventetid =0;       //Tiden som har gått mellom hvær CoT sjekk
int Teller = 1;                  //Styrer rekkefølgen på testingen av knapper (1-4)
int fastBooking = 0;             //Styrer hvilken hurtigbookingrutine som skal kjøres når en knapp holdes inne i mer enn 600ms 

//Variabler for programkontroll ellers i programmet
bool trigger = LOW; // Styrer while looper sammen med knapper på simpel knappesyring i pop-up's

//Variabler for Sleepmode timeren
unsigned long timeMillis = 0;//Timer som styrer sleepmodus
unsigned long sleepMillis = 60000; //sleepMillis=30000 sender ESP32 i sleepmodus etter et halvt minutt uten aktivitet på knappene

//Variabler for Menystyring og Display

int menypunkt_X = 0;     // Holder styr på hvilket nivå vi er på i menyen og når menypunkt_Y og Z skal låses
int menypunkt_Y = 0;     // Teller hvor vi er vertikalt i menynivå Y
int menypunkt_Z = 0;     // Teller hvor vi er vertikalt i menynivå Z

int menymax_X = 3;       //Antall undermenyenivåer i menyen
int menymax_Y = 3;       //Settes til antall menypunkter minus 1 i menyY
int menymax_Z = 4;       //Settes til antall menypunkter minus 1 i største menyarray på menylevel 2

//Menyinnstillinger
const int Fontsize = 2;     //Størrelsen på bokstavene i hovedmenyene
const int Fontheight = 25;  //Høyden i Pixler på bokstavene

//Menyarrays: Her legges menytekster inn for hver menynivå som har nye undermenyer.

const char *menyY00[4] = {"Booking","Gjester","Info/status","Instillinger"}; //Menylevel 1
const char *menyY0Z[5] = {"Bad/Toalett","Toalett","Kjoekken","Stue","Kanseler booking*"}; //Menylevel 2
const char *menyY1Z[5] = {"Send hjem 1","Send hjem alle","Not in use*","Not in use*","Not in use*"}; //Menylevel 2
const char *menyY2Z[5] = {"Oversikt","Vaerdata*","Stroemforbruk*","Inneklima*","Bookinger*"}; //Menylevel 2
const char *menyY3Z[5] = {"Lysdimming","Default lysstyrke","Temperatur","Lufting/Vindu","Takvifte Paa/Av"}; //Menylevel 2

//Tokens for soverommene Rekkefølgen i arrays er soverom 1 til 6 (0-5 i array-posisjoner)
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

/* Tabell for keys til alle signal for soverommene
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


void setup() {

  //Sleep wake up setup
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_32,0); //Den gule pinnen 
  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  

  pinMode(TFT_BACKLIGHT, OUTPUT);    // Output for controlling display ON/OFF via IO pin 5
  digitalWrite(TFT_BACKLIGHT, HIGH); // HIGH=display ON, LOW=display OFF
  tft.init(240, 240, SPI_MODE2);     // Init ST7789 240x240, SPI_MODE2 for Display's without CS-pin(Chip slect pin. Otherwise SPI_MODE0 (Default)
  tft.setRotation(2);                // Orientating screen for correct wieving    
  tft.fillScreen(ST77XX_BLUE);      // Setter Bakgrunnsfarge til blå for oppstartsskjerm
  

  //Initiering av IO-pinner for knappene:
  pinMode(pil_Opp, INPUT);
  pinMode(pil_Ned, INPUT_PULLUP);
  pinMode(pil_Ut, INPUT_PULLUP);
  pinMode(pil_Velg, INPUT);

  //Configure RGB LED og BuzzerAlarm output pins for ESP32:
  ledcSetup(0, 5000, 8); //Setter PVM-pinne 0
  ledcSetup(1, 5000, 8); //Setter PVM-pinne 1
  ledcSetup(2, 5000, 8); //Setter PVM-pinne 2 
  ledcSetup(3, 1500, 12); //Setter PVM-pinne 3 

  // Attach RGB pins.
  ledcAttachPin(redLED, 0); //Linker PWM 0 til pin 25
  ledcAttachPin(greenLED, 1); //Linker PWM 1 til pin 26
  ledcAttachPin(blueLED, 2); //Linker PWM 2 til pin 27
  ledcAttachPin(buzzerAlarm, 3); //Linker PWM 3 til pin 13

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


  //******Selvtest av RGB-LED og Piezohøyttaler********

    //Tester BuzzerAlarm
    for(int m=0;m<1;m++){
      ledcWrite(3, 1024); // 50% dutycycle på buzzerAlarm pin (på)
      delay(70);
      ledcWrite(3, 0);     // 0% dutycycle på buzzerAlarm pin (av)
      delay(70);
    }

  //Tester RGB LED
    for(int n=0;n<3;n++){
      for(int i=0;i<1;i++){ 
        ledcWrite(n, 255); //LED på full styrke
        delay(100);
        ledcWrite(n, 0); //LED av
        delay(100);
      }
    }
  //*****************Selvtest ferdig*******************

   //Velkomsttekst på display
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLUE);
  tft.setCursor(35, 50);
  tft.println("Velkommen til");
  tft.setCursor(15, 180);
  tft.println("Kobler til WiFi...");
  tft.setTextSize(3);
  tft.setCursor(12, 100);
  tft.println("IoT HYBELHUS");
 
   
  if(DEBUG) {Serial.begin(115200);}
  circusESP32.begin();// Initierer CoT-kommunikkasjonen
  delay(2000);
  tft.fillScreen(ST77XX_BLACK); //Setter bakgrunn på skjermen til svart
  oppdaterMeny(menypunkt_X, menypunkt_Y, menypunkt_Z, menymax_Y, menymax_Z); //Viser Hovedmeny på skjermen
} //END SETUP

void loop() {  //Det er HER det skjer.... :-o 

   switch(mainstate){ //Statemaskina som kontrollerer programflyten i hovedprogrammet

    case Mainloop: //Det som skal gjøres hele tiden 

     //Dette er sleepmode:
      //Rutinen som sender controlleren i DeepSleep mode etter 3 minutter når sleepMillis = 180000
      if(digitalRead(pil_Opp) == HIGH && digitalRead(pil_Ned) == HIGH && digitalRead(pil_Velg) == HIGH && digitalRead(pil_Ut) == HIGH){
        if(millis()-timeMillis>= sleepMillis){ //Venter 60 sek
          timeMillis = millis();
          Serial.println("Going to sleep");
          esp_deep_sleep_start();
        }
      }
  

      if((Button > 0)&&(Button < 5)){ // Resetter Button til 0 etter at trykk er registrert og variabler oppdatert.
        if(DEBUG){Serial.print("Knapp trykket: "); Serial.println(Button);} //DEBUG
        oppdaterMenyTellere(Button); //Kaller rutine for å oppdattere X, Y og Z 
        Button = 0;
      }
      else if(Button == 5){
        Button=0; //Hurtigbooking er brukt. Bypasser oppdatering av menytellere og resetter Button til 0 
        mainstate = Hurtigbooking;
      }

      if(Teller == 1){checkbuttonstate(pil_Opp);}    //Leser Grønn knapp
      if(Teller == 2){checkbuttonstate(pil_Ned);}    //Leser Rød knapp
      if(Teller == 3){checkbuttonstate(pil_Ut);}     //Leser Gul knapp
      if(Teller == 4){checkbuttonstate(pil_Velg);}   //Leser Blå knapp
      
      CotTimer=millis(); //Timer for intervallet mellom sjekking av registerverdier i CoT

      if(CotTimer - forrigeCotTimer > lesCotIntervall){mainstate = SjekkCoT;}
      if(DEBUG){if(forrige_buttonstate != buttonstate){Serial.print("Switchcasestate nr: "); Serial.println(buttonstate);}} //DEBUG


      if(menypunkt_X == 2 && menypunkt_Y == 0){mainstate = Booking;} //Går til case for valg av bookingfunksjoner
      if(menypunkt_X == 2 && menypunkt_Y == 1){mainstate = Gjester;} //Går til case for mottak/retur av gjester
      if(menypunkt_X == 2 && menypunkt_Y == 2){mainstate = Info;} //Går til case for informasjonsrutiner
      if(menypunkt_X == 2 && menypunkt_Y == 3){mainstate = Instillinger;} //Går til case for systeminnstillinger
    break;

    case SjekkCoT:  
      ventetid = CotTimer - forrigeCotTimer; //Finner tidsintervallet siden siste avlesning
      
      sjekker(beboerNum); //Funksjonen som sjekker alle varselkoder i CoT
      if(DEBUG){Serial.println("Verdier fra Gjest og Alarm er hentet og sjekket");}
      if(DEBUG){Serial.println(ventetid);}
      forrigeCotTimer = millis();
      delay(500); //Sørger for at det ikke hoppes i menyen etter retur fra sjekker()
      mainstate = Mainloop;
      break;

    case Booking:
    if(menypunkt_Z < 4){bookingRutiner();if(DEBUG){Serial.println("Booking er utført");}} //funksjonen inn her???
    if(menypunkt_Z == 4){menydummy();if(DEBUG){Serial.println("KanselerBooking rutine er kjørt");}}
    
    mainstate = Mainloop;
    break;

    case Hurtigbooking:
    mainstate = Mainloop;
      switch(fastBooking){
        case 1: fastbookingToalett(); break;
        case 2: fastbooking2(); break;
        case 3: fastbooking3(); break;
        case 4: fastbooking4(); break;
        default: break;
      }
    if(DEBUG){Serial.println("Hurtigbooking er utført");}
    fastBooking = 0; 
    
    break;

    case Gjester:
      if(menypunkt_Z == 0){send_hjem(beboerNum);}
      if(menypunkt_Z == 1){send_hjem_alle(beboerNum);}
      if(menypunkt_Z == 2){menydummy();}
      if(menypunkt_Z == 3){menydummy();}
      if(menypunkt_Z == 4){menydummy();}
      if(DEBUG){Serial.println("Gjestebehandling er utført");}
      mainstate = Mainloop;
    break;

    case Info:
        if(menypunkt_Z == 0){info();}
        if(menypunkt_Z == 1){menydummy();}
        if(menypunkt_Z == 2){menydummy();}
        if(menypunkt_Z == 3){menydummy();}
        if(menypunkt_Z == 4){menydummy();}
        if(DEBUG){Serial.println("Inforutine er utført");}
        mainstate = Mainloop;
    break;

     case Instillinger:
        if(menypunkt_Z == 0){lysDimming();}
        if(menypunkt_Z == 1){lysStyrke();}
        if(menypunkt_Z == 2){temperatur();}
        if(menypunkt_Z == 3){vindu();}
        if(menypunkt_Z == 4){vifte();}
        if(DEBUG){Serial.println("Instillingsrutine er utført");}
        mainstate = Mainloop;
    break;

  } //END SWITCH mainstate
} //END MAIN LOOP

void updateButtonVariabel(int buttonpin){ // Oppdaterer Button-variabelen 
 switch(buttonpin){ //Finner hvilken trykknapp som er trykt ned og Opptaderer Button-variabelen
    case 34: Button = 1; break;  //pil_Opp GRØNN knapp  
    case 33: Button = 2; break;   //pil_Ned RØD knapp   
    case 32: Button = 3; break;   //pil_Ut GUL knapp   
    case 35: Button = 4; break;   //pil_Velg BLÅ knapp
  } //END SWITCH buttonpin
} //END FUNCTION updateButtonVariabel

void oppdaterMenyTellere(int button){ //Oppdaterer menytellerne X, Y og Z
  //Variabler for sammenligning/detektering av endring i menyvariablene
  int oldX = menypunkt_X; int oldY = menypunkt_Y; int oldZ = menypunkt_Z; 

  switch (Button) {

    case 1:  // Oppknapp Y og Z (teller  nedover)
      if ((menypunkt_Y > 0 ) and (menypunkt_X == 0)) {
        menypunkt_Y -=  1;
      } 
      else if ((menypunkt_Z > 0 ) and (menypunkt_X == 1)) {
        menypunkt_Z -= 1;
      }
    break;

    case 2:  // Nedknapp Y og Z (teller oppover)
      if ((menypunkt_Y < menymax_Y ) and (menypunkt_X == 0)) {
        menypunkt_Y +=  1;
      } 
      else if  ((menypunkt_Z < menymax_Z ) and (menypunkt_X == 1)) {
        menypunkt_Z += 1;
      }
    break;

    case 3: // Negativ x / bakover knapp  
      if (menypunkt_X > 0) {
        if(menypunkt_X == 1){menypunkt_Y = 0; menypunkt_Z = 0;}
        menypunkt_X -= 1;
      }
    break;

    case 4: // Positiv x / enter knapp
      if ((menypunkt_X < menymax_X)) {
        menypunkt_X += 1;
      }
    break;
  }
  //Oppdater hovedmeny og cursor dersom det er en endring i Y eller Z telleren når X telleren er mindre enn 2
  if((menypunkt_X < 2) && ((oldX != menypunkt_X) || (oldY != menypunkt_Y) || (oldZ != menypunkt_Z))){
    oppdaterMeny(menypunkt_X, menypunkt_Y, menypunkt_Z, menymax_Y, menymax_Z);
  }

  if(DEBUG){Serial.print("X-verdi: "); Serial.println(menypunkt_X);}
  if(DEBUG){Serial.print("Y-verdi: "); Serial.println(menypunkt_Y);}
  if(DEBUG){Serial.print("Z-verdi: "); Serial.println(menypunkt_Z);}
  if(DEBUG){Serial.println("Menytellere oppdatert");}
} //END FUNCTION oppdaterMenyTellere



void oppdaterMeny(int x, int y, int z, int maxY, int maxZ){ //Skriver hovedmeny og undermenyer

  // Setter standard fontstørrelse, tekstfarge og blanker skjermen
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(Fontsize);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  if(x==0){ // Skriver hovedmenyen på skjermen
    for(int n=0;n <= maxY;n++){
      tft.setCursor(15, 30 + Fontheight*n);
      tft.println(menyY00[n]);  
    }
    //Skriver cursor med invers farge
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setCursor(15, 30 + Fontheight*y);
    tft.println(menyY00[y]); 
  }

 if((x==1) && (y==0)){ //Skriver undermeny 1 på skjermen  
    for(int n=0;n <= maxZ;n++){
      //tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(15, 30 + Fontheight*n);
      tft.println(menyY0Z[n]);  
    }
    //Skriver cursor med invers farge
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setCursor(15, 30 + Fontheight*z);
    tft.println(menyY0Z[z]); 
  }

  if((x==1) && (y==1)){ //Skriver undermeny 2 på skjermen
    for(int n=0;n <= maxZ;n++){ //Undermeny 2
      tft.setCursor(15, 30 + Fontheight*n);
      tft.println(menyY1Z[n]);  
    }
    //Skriver cursor med invers farge
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setCursor(15, 30 + Fontheight*z);
    tft.println(menyY1Z[z]); 
  }

  if((x==1) && (y==2)){ //Skriver undermeny 3 på skjermen
    for(int n=0;n <= maxZ;n++){ // Undermeny 3
      tft.setCursor(15, 30 + Fontheight*n);
      tft.println(menyY2Z[n]);  
    }
    //Skriver cursor med invers farge
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setCursor(15, 30 + Fontheight*z);
    tft.println(menyY2Z[z]); 
  }

  if((x==1) && (y==3)){ //Skriver undermeny 4 på skjermen
    for(int n=0;n <= maxZ;n++){ // Undermeny 4
      tft.setCursor(15, 30 + Fontheight*n);
      tft.println(menyY3Z[n]);  
    }
    //Skriver cursor med invers farge
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setCursor(15, 30 + Fontheight*z);
    tft.println(menyY3Z[z]); 
  }
} //END FUNKTION oppdaterMeny

void menydummy(){//Beskjed når ikke-implementerte menypunkter velges...

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(30, 60);
  tft.print("**FUTURE**");
  tft.setCursor(10, 100);
  tft.print("New Function");
  tft.setCursor(10, 150);
  tft.print("TO COME HERE");
  if(DEBUG){Serial.print("Menydummy Kjørt");}
  delay(3000);

  //Går tilbke til hovedmeny
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
}//END FUNCTION menydummy



void fastbookingToalett(){ //Funksjonen for hurtigbooking av toalett på GRØNN knapp
  //Her legges koden inn for direktebooking av første ledige tid for dobesøk på bad 1 eller 2
  //Svar gis i popup meny på controller,  med tidspunkt og rom.
  menydummy();
  if(DEBUG){Serial.println("Toalett hurtigbooking på GRØNN knapp OK :-)");}
}

void fastbooking2(){ 
  // Legg inn kode/Funksjonskall for hurtigbooking på RØD knapp her
  menydummy();
  if(DEBUG){Serial.println("Fastbooking rutine 2 på RØD knapp OK :-)");}
}

void fastbooking3(){ 
  // Legg inn kode/Funksjonskall for hurtigbooking på GUL knapp her
  menydummy();
  if(DEBUG){Serial.println("Fastbooking rutine 3 på GUL knapp OK :-)");}
}

void fastbooking4(){ 
  // Legg inn kode/Funksjonskall for hurtigbooking på BLÅ knapp her
  menydummy();
  if(DEBUG){Serial.println("Fastbooking rutine 4 på BLÅ knapp OK :-)");}
}

void nesteTeller(int teller, int buttonpin){   // Styrer hvilken knapp som testes for at den trykkes ned.
  if((teller < 4) && (buttonpin==pil_Opp)||(buttonpin==pil_Ned)||(buttonpin==pil_Ut)){Teller = Teller + 1; }
  else if((teller == 4)&&(buttonpin==pil_Velg)){Teller = 1;}
}

//****Buttontester rutine med debouncing og detektering for kort/langt(1 sek) trykk****
void checkbuttonstate(int buttonpin) {

  buttonStateValue = digitalRead(buttonpin);    //leser av button før hver kjøring av statemaskina
  forrige_buttonstate = buttonstate;       // Setter forrigebuttonstate til nåværende buttonstate

  //Statemaskin for registrering av tastetrykk, tast holdt inne og debouncing av tastetrykk.
  switch (buttonstate) {

    case Wait: //Venter på at button skal bli trykket ned.
      if(buttonStateValue == LOW) {buttonstate = Falling;}
      else if(buttonstate == Wait){nesteTeller(Teller, buttonpin);}
    break;

    case Falling: //Logger tidspunktet buttonStateValue går LOW
      lowStarttid = millis();
       buttonstate = Stable;      
    break;

    case Stable: //Sjekker at signalet har vært stabilt lenge nok(> bouncdDelay), og går til Wait stat'en om det fortsatt bouncer/er utstabilt
      lowStopptid = millis();
      if (lowStopptid - lowStarttid > bounceDelay) {buttonstate = Rising;}          
    break;

    case Rising:  //Godkjenner ev. kort knappetrykk når buttonPin går stabil HIGH, og sjekker om det også er et langt knappetrykk
      lowStopptid = millis();
      if (buttonStateValue == HIGH) {buttonstate = Shortpress;} //Registrerer korte trykk
      if (lowStopptid - lowStarttid > holdDelay){buttonstate = Longpress;} //Registrerer at knappen er holdt inne i 1 sek      
    break;

    case Shortpress: // Setter Button-variabelen til verdien for trykket knapp, og buttonstate til Wait    
      updateButtonVariabel(buttonpin);
      nesteTeller(Teller, buttonpin);
      buttonstate = Wait; 
      while (buttonStateValue == LOW) {}
    break;
    
    case Longpress:  //Venter på at knappen skal slippes (HIGH) når den holdes inne, kjører hurtigbooking og setter buttonstate til reset
      if ((buttonStateValue == HIGH) && (buttonpin == pil_Opp)){  //Booker når GRØNN knapp holdes inne i 1 sek
        fastBooking =1; nesteTeller(Teller, buttonpin); buttonstate = Wait;
      } 
      if ((buttonStateValue == HIGH) && (buttonpin == pil_Ned)){  //Booker når RØD knapp holdes inne i 1 sek
        fastBooking = 2; nesteTeller(Teller, buttonpin); buttonstate = Wait;
      } 
      if ((buttonStateValue == HIGH) && (buttonpin == pil_Ut)){//Booker når GUL knapp holdes inne i 1 sek 
        fastBooking = 3; nesteTeller(Teller, buttonpin); buttonstate = Wait;
      }
      if ((buttonStateValue == HIGH) && (buttonpin == pil_Velg)){//Booker når BLÅ knapp holdes inne i 1 sek
        fastBooking = 4; nesteTeller(Teller, buttonpin); buttonstate = Wait;
      }           
      Button = 5;
    break;
  } 
}


void lysStyrke() {
 
  // Setter ønsket lysnivå i rommet Signal 1 i soverom statusregisteret tilsier lys nivå (0-255)
  int Lys = circusESP32.read(order_key_LED, token);
  char L_[16];
  
  //Displayet viser verdiene
      sprintf(L_, "Lys lvl %d", Lys);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 40);
      tft.print(L_);
      if(DEBUG){Serial.print("Lys nivå");Serial.print(Lys);Serial.println(" ");}

  //Det skal vises lys nivået
  while ( digitalRead(pil_Velg) == HIGH) {
    if ((digitalRead(pil_Ned) == LOW) && (Lys >= 0)) {
      Lys = Lys - 5;
      delay(100);
      //Displayet viser verdiene
      sprintf(L_, "Lys lvl %d", Lys);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 40);
      tft.print(L_);
      if(DEBUG){Serial.print("Lys nivå");Serial.print(Lys);Serial.println(" ");}
    }
    else if ((digitalRead(pil_Opp) == LOW) && (Lys <= 255)) {
      Lys = Lys + 5;
      delay(100);
      //Displayet viser verdiene
      sprintf(L_, "Lys lvl %d", Lys);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 40);
      tft.print(L_);
      if(DEBUG){Serial.print("Lys nivå");Serial.print(Lys);Serial.println(" ");}  
    }
  }
  circusESP32.write(order_key_LED, Lys, token);
  tft.fillScreen(ST77XX_BLACK);
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
}//END FUNCTION lys


//Gjør at lyset styres automatisk eller manuelt
void lysDimming(){ //Styrer lyset "direkte" med kontrolleren i manuelt modus
  
  //Setter førat auto eller manuelt modus for lyset. Hvis manuelt modus velges, kan lyset styres
  //med grønn(opp) og rød(ned) knapp. Settes modus til auto, går rutinen tilbake til hovedmeny.
        
        int Lys = circusESP32.read(order_key_LED, token);
        int automatisk = circusESP32.read(order_key_lysautomatisk, token);
        char L_[16];

        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(3);
        tft.setCursor(15, 30);
        tft.print("Lysmodus:");
         tft.setTextSize(2);
        tft.setCursor(20, 80);
        tft.print("Auto-> pil opp");
        tft.setCursor(20, 120);
        tft.print("Manuell -> pil ned");
  
  
  while((digitalRead(pil_Velg)==HIGH) && (digitalRead(pil_Ut)==HIGH)){
    //automatisk 1 så vil lyset styre seg mot LUX sensoren
    //0 så vil lyset styres av Lys funksjonen på kontrolleren
    if (digitalRead(pil_Ned)==LOW){
      automatisk = 0;
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(15, 80);
      tft.print("Lys:");
      tft.setCursor(15, 120);
      tft.print("Manuelt   ");
      delay(100);
    }
    else if(digitalRead(pil_Opp)==LOW){
      automatisk = 1;
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(15, 80);
      tft.print("Lys:");
      tft.setCursor(15, 120);
      tft.print("Automatisk");
      delay(100);
    }
  }
  while((digitalRead(pil_Velg)==LOW) || (digitalRead(pil_Ut)==LOW)){delay(100);}//venter på at knapp slippes
  circusESP32.write(order_key_lysautomatisk, automatisk, token); //Oppdaterer CoT-verdien
  
  if(automatisk == 0){// Her justeres lyset nå manuelt dersom manuell styring ble valgt eller var satt fra før
    while ((digitalRead(pil_Velg) == HIGH) && (digitalRead(pil_Velg) == HIGH)) {
      if ((digitalRead(pil_Ned) == LOW) && (Lys >= 0)) {
        Lys = Lys - 5;
        delay(100);
        //Displayet viser verdiene
        sprintf(L_, "Lys lvl %d", Lys);
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(3);
        tft.setCursor(10, 40);
        tft.print(L_);
        if(DEBUG){Serial.print("Lys nivå");Serial.print(Lys);Serial.println(" ");}
        circusESP32.write(order_key_LED, Lys, token);
      }
      else if ((digitalRead(pil_Opp) == LOW) && (Lys <= 255)) {
        Lys = Lys + 5;
        delay(100);
        //Displayet viser verdiene
        sprintf(L_, "Lys lvl %d", Lys);
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(3);
        tft.setCursor(10, 40);
        tft.print(L_);
        if(DEBUG){Serial.print("Lys nivå");Serial.print(Lys);Serial.println(" ");}
         circusESP32.write(order_key_LED, Lys, token); 
      }      
    }
  }
  tft.fillScreen(ST77XX_BLACK);
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
} //END FUNCTION lysDimming


void temperatur() {
 
  // Setter ønsket temperatur i rommet Signal 6 i soverom statusregisteret,
  // tilsier temperaturer mellom 12-25 grader.
  int temp = circusESP32.read(order_key_onsketTemp, token);
  char L_[16];
  
  //Displayet viser verdiene
      sprintf(L_, "Temp lvl %d", temp);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 40);
      tft.print(L_);
      if(DEBUG){Serial.print("Temperatur");Serial.print(temp);Serial.println(" ");}

  //Viser valgt temperatur
  while ( digitalRead(pil_Velg) == HIGH) {
    if ((digitalRead(pil_Ned) == LOW) && (temp > 12)) {
      temp = temp - 1;
      delay(100);
      //Displayet viser verdiene
      sprintf(L_, "Temp lvl %d", temp);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 40);
      tft.print(L_);
      if(DEBUG){Serial.print("Temperatur");Serial.print(temp);Serial.println(" ");}
    }
    else if ((digitalRead(pil_Opp) == LOW) && (temp < 25)) {
      temp = temp + 1;
      delay(100);
      //Displayet viser verdiene
      sprintf(L_, "Temp lvl %d", temp);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(10, 40);
      tft.print(L_);
      if(DEBUG){Serial.print("Temperatur");Serial.print(temp);Serial.println(" ");}  
    }
  }
  circusESP32.write(order_key_onsketTemp, temp, token);
  tft.fillScreen(ST77XX_BLACK);
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
}//END FUNCTION temperatur


void vifte(){
 
  // Setter ønsket viftehastighet i rommet Signal 7 i soverom statusregisteret. 
  //Vifte hastighet er (0(av) og 90-200)
  int viftespeed = circusESP32.read(order_key_takvifte, token);
  char L_[16];
  if(viftespeed < 90){viftespeed = 90;} //Setter vifta til minste fart hvis den er slått av fra før
  
  //Displayet viser verdien
      sprintf(L_, "Speed: %d ", viftespeed);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(20, 60);
      tft.print("Viftens");
      tft.setCursor(20, 90);
      tft.print("hastighet:");
      tft.setCursor(20, 150);
      tft.print(L_);
      if(DEBUG){Serial.print("Viftespeed");Serial.print(viftespeed);Serial.println(" ");}

  //Det skal vises viftehastighet på skjermen
  while ((digitalRead(pil_Velg) == HIGH) && (digitalRead(pil_Ut) == HIGH)) {
    if ((digitalRead(pil_Ned) == LOW) && (viftespeed >= 90)) {
      viftespeed = viftespeed - 5;
      delay(100);
      if(viftespeed < 90){viftespeed = 0;}
      //Displayet viser verdiene
      sprintf(L_, "Value: %d ", viftespeed);
      //tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(20, 150);
      tft.print(L_);
      if(DEBUG){Serial.print("Viftespeed");Serial.print(viftespeed);Serial.println(" ");}
      circusESP32.write(order_key_takvifte, viftespeed, token); 
    }
    else if ((digitalRead(pil_Opp) == LOW) && (viftespeed < 205)) {
      viftespeed = viftespeed + 5;
      delay(100);
      if(viftespeed > 0 && viftespeed < 90){viftespeed = 90;}
      //Displayet viser verdiene
      sprintf(L_, "Value: %d ", viftespeed);
      //tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(20, 150);
      tft.print(L_);
      if(DEBUG){Serial.print("Viftespeed");Serial.print(viftespeed);Serial.println(" ");}
      circusESP32.write(order_key_takvifte, viftespeed, token);  
    }
  }
  trigger = LOW;
  tft.fillScreen(ST77XX_BLACK);
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
}//END FUNCTION vifte


void vindu(){ //Styrer åpning og lukking av vinduet i soverommet manuelt fra kontrolleren
 
  // Åpner eller lukker vinduet via Kontroller
  int vindustatus = circusESP32.read(order_key_vindu, token);
  char L_[16];
  
  //Displayet viser Status for vinduet 1=LUKKET, 0=ÅPENT
      sprintf(L_, "Status: %d ", vindustatus);
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(15, 60);
      tft.print("Velg status");
      tft.setTextSize(2);
      tft.setCursor(5, 120);
      tft.print("1 = Close  0 = Open");
      tft.setTextSize(3);
      tft.setCursor(25, 160);
      tft.print(L_);
      if(DEBUG){Serial.print("Vindu status: ");Serial.print(vindustatus);Serial.println(" ");}

  //Viser valgt status for vinduet. Verdi velges med GRØNN(pil_Opp) og RØD(pil_Ned) knapp.
  while ( digitalRead((pil_Velg) == HIGH) && (trigger == LOW)) {
    if ((digitalRead(pil_Ned) == LOW) && (vindustatus == 1)) {
      vindustatus = 0;
      delay(100);
      //Displayet viser verdiene
      sprintf(L_, "Status: %d ", vindustatus);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(25, 160);
      tft.print(L_);
      if(DEBUG){Serial.print("Vindu status:");Serial.print(vindustatus);Serial.println(" ");}
      circusESP32.write(order_key_vindu, vindustatus, token);
    }
    else if ((digitalRead(pil_Opp) == LOW) && (vindustatus == 0)) {
      vindustatus = 1;
      delay(100);
      //Displayet viser verdiene
      sprintf(L_, "Status: %d ", vindustatus);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.setCursor(25, 160);
      tft.print(L_);
      if(DEBUG){Serial.print("Vindu status: ");Serial.print(vindustatus);Serial.println(" ");} 
      circusESP32.write(order_key_vindu, vindustatus, token);
    }
    if( digitalRead(pil_Velg) == LOW){trigger = HIGH;}
    
  }
  trigger = LOW;
  tft.fillScreen(ST77XX_BLACK);
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
}//END FUNCTION Vindu


void info() { //Tekst framstilling i displayet av  vær, temperatur og strømforbruk
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.print("Henter data");
 
    char V_[18];  // char V_[16]; Opprinnelig 16. Dette ble for lite allokert plass i minne, og krasjet programmet når info() ble kjørt.
    char T_[18];  // NB: Det må også settes av plass til /0 karakter som alltid legges til (end of array marker - /0)
    char W_[18];
    //Vær
    int Vear = circusESP32.read("39", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
    //temperatur
    int Temp = circusESP32.read("20183", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
    //Strømforruk personlig
    int Watt = circusESP32.read("1069", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk1In0.9bD-g6Gi40yjEiEYGOY1eoWl0wEuAZZN67yzS5gYOQs");

    //Omgøring til char for displayet
    sprintf(V_, "Weather %d", Vear); //Kanskje endre dette til en rekke char meldinger ut fra signalet
    sprintf(T_, "Temp: %d C", Temp); 
    sprintf(W_, "%d   Kr", Watt);

    if (Vear == 0){ sprintf(V_, "Weather: sol", Vear);}
    else if(Vear == 1){ printf(V_, "Weather: Skyet", Vear);}
    else if(Vear == 2){sprintf(V_, "Weather: Regn", Vear);}
    else if(Vear == 3){sprintf(V_, "Weather: Snø", Vear);}

    //Skriv til displayet
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setCursor(15, 50);
    tft.print(T_);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);   
    tft.setCursor(15, 100);
    tft.print(V_);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(15, 150);   
    tft.print("Stromforbruk");
    tft.setCursor(30, 180);
    tft.print(W_);
    
   while (digitalRead(pil_Ut) == HIGH) {
     delay(bounceDelay - 5);
    }
  
  tft.fillScreen(ST77XX_BLACK);
  
  //Meny det skal returneres til:
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X, menypunkt_Z, menypunkt_Y, menymax_Y, menymax_Z);
  
} //END FUNCTION info


void send_hjem(int beboer) { // beboer: er nummeret til beboer ( 1 til 6)

  char *myArray[] = {"24425", "10799", "9663", "2277", "31631", "27545"}; // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[beboer - 1];

  //Lokale variablen til personlige gjester i inngangsdør status registeret
  int gjest = circusESP32.read(key_beboer, "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U");
  gjest -= 10;
  circusESP32.write(key_beboer, gjest, "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U");

  menypunkt_X = 1;
  //menypunkt_Y = 0;
  //menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);

  delay(2000);
} //END FUNCTION send_hjem


void send_hjem_alle(int beboer) { // beboer: er nummeret til beboer ( 1 til 6)

  char *myArray[] = {"24425", "10799", "9663", "2277", "31631", "27545"}; // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[beboer - 1];

  double AB_beboer = circusESP32.read(key_beboer, "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"); // Get the old value From COT
  int AB = AB_beboer;
  int B_beboer = AB % 10;
  AB_beboer = ( 00 + B_beboer ); // add new A value
  circusESP32.write(key_beboer, AB_beboer , "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"); // update value to COT

  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
} //END FUNCTION send_hjem_alle


void bookingRutiner(){ //Når rommet er valgt
  char start_tid_Array[3]= {};
  char stop_tid_Array[3] = {};
  //enable om det er mulig å booke viss andre booker må alle andre vente
  //Bruker signal 5 i booking statusregisteret
  if (circusESP32.read("5141","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8")==0){
    //oppdaterer signalet slik at andre ikke booker samtidig
    circusESP32.write("5141",1,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    int lokal_enable = 1;
    int room = menypunkt_Z; //for koordinatene til rommet valgt
    //Alt dette bør være

    //Leser av nåtiden
    int DATO = circusESP32.read("6660","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
    int tid = circusESP32.read("20723","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
    //Gjør om tiden til en char og putter dem i arrayet slik at de kan spaltes opp i mindre biter
    sprintf(start_tid_Array,"%ld", tid);
    sprintf(stop_tid_Array,"%ld", tid);
    
    //Tar hver posisjon i arrayet og splitter det i timer og minutter, og så legger de sammen
    //tar -'0' for hvert element for å konvertere de til int
    int start_min = (start_tid_Array[2]-'0')*10 + (start_tid_Array[3]-'0');
    int start_time = (start_tid_Array[0]-'0')*10 + (start_tid_Array[1]-'0');
    int slutt_min = (stop_tid_Array[2]-'0')*10 + (stop_tid_Array[3]-'0');
    int slutt_time = (stop_tid_Array[0]-'0')*10 + (stop_tid_Array[1]-'0');

    tft.fillScreen(ST77XX_BLACK);

    char DATO_[16];
    char start_[16];
    char stop_[16];
    
    //KODE SOM GJØR AT KNAPPENE VELGER START OG SLUTT TID
    //Kjører i en uendilg while løkker slik at verdiene man kan endre verdiene til man velger å gå ut
    while (digitalRead(pil_Velg) == HIGH){
           
      if(digitalRead(pil_Opp)==LOW){
        DATO += 1; //Skal gå framover en dag for hvert trykk
        delay(250);// Delays utover i funksjonen slik at verdiene ikke øker veldig mye for vert knappetrykk
      }
      else if(digitalRead(pil_Ned)==LOW){
        DATO -= 1;
        delay(250);
      }
      else{
        //char DATO_[16];
        sprintf(DATO_, "Velg dato:%d", DATO);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 40);
        tft.print(DATO_);
        
      }
    }
    delay(500);
    tft.fillScreen(ST77XX_BLACK);
    while (digitalRead(pil_Velg) == HIGH){
      if(digitalRead(pil_Opp)==LOW){
        start_min += 10; //Skal gå framover 10 min for hvert trykk
        delay(250);
      }
      else if(digitalRead(pil_Ned)==LOW){
        start_min -= 10;
        delay(250);
      }
      //Gjør det slik at int for minuttene ikke vil overskride 60
      //Viss de evt. gjør det vil det plusse på int for timen og sette min 60 tilbake
      else if(start_min >=60){
        start_time += 1;
        start_min -= 60;
      }
      else if(start_min <0){
        start_time -= 1;
        start_min += 60;
      }
      else{
        //char start_[16];
        sprintf(start_ , "Velg start tid:%d", (start_time*100 + start_min));
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 40);
        tft.print(start_);
      }
    }
    delay(500);
    tft.fillScreen(ST77XX_BLACK);
    while (digitalRead(pil_Velg) == HIGH){
      if(digitalRead(pil_Opp)==LOW){
        slutt_min += 10; //Skal gå framover 10 min for hvert trykk
        delay(250);
      }
      else if(digitalRead(pil_Ned)==LOW){
        slutt_min -= 10;
        delay(250);
      }
      else if(slutt_min >=60){
        slutt_time += 1;
        slutt_min -= 60;
      }
      else if(slutt_min <=0){
        slutt_time -= 1;
        slutt_min += 60;
      }
      else{
        //char stop_[16];
        sprintf(stop_, "Velg stop tid:%d", (slutt_time*100 + slutt_min));
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 40);
        tft.print(stop_);
      }
    }
    delay(250);
    int start_tid = start_time*100 + start_min;
    int stop_tid = slutt_time*100 + slutt_min;
    //Når alt dette er valgt og velger send så skal alle disse signalene sendes
    //signalet for vise vilket rom man valgte (signal 9 i bookingregisteret)
    circusESP32.write("9027",room,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    //Personlig verdien for brukeren 1-6 (signal 4 i Bookingregisteret)
    circusESP32.write("11448",1,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    //signalet for start tid (sginal 0 i Bookingregestiret)
    circusESP32.write("12596",DATO,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    circusESP32.write("28768",start_tid,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    //signalet for stopp tid 
    circusESP32.write("26982",stop_tid,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    tft.fillScreen(ST77XX_BLACK);
  }
  else{
    
      //Skriv i displayet
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.setCursor(10, 20);
      //tft.setTextSize(3);
      tft.print("ERROR!:");
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      //tft.setTextSize(3);
      tft.setCursor(10, 45);
      tft.print("andre booker");
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      //tft.setTextSize(3);
      tft.setCursor(10, 65);
      tft.print("vent et par sekekunder");
    while(digitalRead(pil_Opp) == HIGH && digitalRead(pil_Ned) == HIGH && digitalRead(pil_Velg) == HIGH && digitalRead(pil_Ut) == HIGH){
      delay(15);
    }
    tft.fillScreen(ST77XX_BLACK);
  }
  menypunkt_X = 0;
  menypunkt_Y = 0;
  menypunkt_Z = 0;
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
} //END FUNCTION bookingRutiner


void sjekker(int beboer){ //Sjekker de forskjellige varslingene
  // oppdatere ulike variabler: 
  int B = ReadFromCOT_B_gjester_kontroller(beboerNum); //Henter registerstatus på ringeklokka og antall på besøk
  int bookingMelding = circusESP32.read("29239","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
  int brannAlarmStatus = circusESP32.read("31931", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
  
  if(brannAlarmStatus > 0){ //Varsler BRANN i display, med lyd og blinkende led
     if(DEBUG){Serial.print("BRANNALARMEN HAR GÅTT !!!");}
    //pop up on screen -Skriv i displayet
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setCursor(40, 90);
    tft.print("ADVARSEL!");
    tft.setTextSize(4);
    tft.setCursor(50, 140);
    tft.print("BRANN!");
      while (digitalRead(pil_Opp) == HIGH && digitalRead(pil_Ned) == HIGH && digitalRead(pil_Velg) == HIGH && digitalRead(pil_Ut) == HIGH) {
        //Buzzer&Blink
        ledcWrite(3, 1024); // 50% dutycycle på buzzerAlarm pin (på)
        ledcWrite(2, 255); //LED på full styrke
        tft.setTextColor(ST77XX_BLACK, ST77XX_RED);
        tft.setCursor(50, 140);
        tft.print("BRANN!");
        delay(300);
        ledcWrite(3, 0); // 0% dutycycle på buzzerAlarm pin (av)
        ledcWrite(2, 0); //LED av
        tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
        tft.setCursor(50, 140);
        tft.print("BRANN!");
        delay(300);
      }
    tft.fillScreen(ST77XX_BLACK); //Clear screen
    ledcWrite(3, 0); // 0% dutycycle på buzzerAlarm pin (av)
    ledcWrite(2, 0); //LED av
    oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
  } 
  else if ( B  > 0) { //Sjekker om noen er på døra 
    if(DEBUG){Serial.print("Gjest har trykket på ringeklokka :-)");}
    tft.fillScreen(ST77XX_BLACK); //Clear Screen
    
    //pop up on screen
    //Gjest på døra annonseres i displayet
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setCursor(30, 40);
    tft.print("ta imot en");
    tft.setCursor(80, 80);
    tft.print("gjest ?");
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(10, 150);
    tft.print("JA -VELG");
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setCursor(70, 200);
    tft.print("NEI -UT");

    for(int n=0;n<4;n++){ //Ringeklokka ringer og blinker 4 ganger
      ledcWrite(3, 1024); // 50% dutycycle på buzzerAlarm pin (på)
      ledcWrite(0, 255); //LED på full styrke
      delay(300);
      ledcWrite(3, 512); // 0% dutycycle på buzzerAlarm pin (av)
      ledcWrite(0, 0); //LED av
      delay(300);
      ledcWrite(3, 0); // 0% dutycycle på buzzerAlarm pin (av)
      ledcWrite(0, 255); //LED på full styrke
    }

  while(trigger == LOW){
    
      if (digitalRead(pil_Velg) == LOW){
        WriteToCOT_ja_gjester_kontroller(beboerNum);
        tft.fillScreen(ST77XX_BLACK);
        menypunkt_X = 0;
        menypunkt_Y = 0;
        menypunkt_Z = 0;
        oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
        trigger = HIGH;
      }
      else if (digitalRead(pil_Ut) == LOW){
        WriteToCOT_nei_gjester_kontroller(beboerNum);
        tft.fillScreen(ST77XX_BLACK);
        menypunkt_X = 0;
        menypunkt_Y = 0;
        menypunkt_Z = 0;
        oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
        trigger = HIGH;
      }     
    }
  ledcWrite(0, 0); //LED av
  delay(bounceDelay);
  trigger = LOW;
  }   
    
  
    //Booking melding
  else if(bookingMelding > 0){
    tft.fillScreen(ST77XX_CYAN);
      //pop up on screen
    if(bookingMelding == 1){//Skriv i displayet at booking er godkjent        
      tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);
      tft.setTextSize(2);
      tft.setCursor(10, 80);
      tft.print("Bookingen godkjent");
      tft.setCursor(10, 120);
      tft.print("Trykk UT-knappen");
    }
    else if(bookingMelding == 2){ //Skriv i displayet at bookingne er godkjent, men overlapper        
      tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);
      tft.setTextSize(2);
      tft.setCursor(10, 80);
      tft.print("Bookingen godkjent,");
      tft.setCursor(10, 120);
      tft.print("men overlapper");
      tft.setCursor(10, 160);
      tft.print("Trykk UT-knappen");
    }
    else if(bookingMelding == 3){ //Skriv i displayet feil      
      tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);
      tft.setTextSize(2);
      tft.setCursor(10, 80);
      tft.print("Booking AVVIST !"); //Kanskje legge på mulige grunner
      tft.setCursor(10, 120);
      tft.print("Book ny tid :-)");
      tft.setCursor(10, 160);
      tft.print("Trykk UT-knappen");
    }

    while(digitalRead(pil_Ut) == HIGH && trigger == LOW){ //Venter til UT-knappen er trykt
      if(digitalRead(pil_Ut) == LOW){
        delay(bounceDelay-5);
        trigger = HIGH;
      }
    } 

    tft.fillScreen(ST77XX_BLACK);
    oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
    //Setter bookingMelding tilbake til "0"
    circusESP32.write("29239",0,"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAxIn0.K5jdezdIHMtXu-oXr5cJYWosPWWQdtSH9W9O-D6jkb8");
    trigger = LOW;
    
    
  }
} //END FUNCTION Sjekker

//------------------------------ Sjekke variabel B, "Gjest på døra" --------------------------------------- (Universal)
int ReadFromCOT_B_gjester_kontroller (int counter)  {// counter: er nummeret til beboer ( 1 til 6)

  char *myArray[] = {"24425", "10799", "9663", "2277", "31631", "27545"}; // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[counter -1];
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"; // CoT - Inngangsdor_statusregister - token:

  double AB_beboer = circusESP32.read(key_beboer, token_Inngangsdor_statusregister); // Get the old value From COT
  int AB = AB_beboer;
  int B_beboer = AB % 10;
  return B_beboer;  // Return B value
} //END FUNCTION ReadFromCOT_B_gjester_kontroller

//------------------------------ReadFromCOT_B_gjester_kontroller (slave til "sjekker()")   --------------------------------------- 
int WriteToCOT_ja_gjester_kontroller(int counter){   // counter: er nummeret til beboer ( 1 til 6)
  
  char *myArray[] = {"24425","10799","9663","2277","31631","27545"}; // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[counter - 1];
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"; // CoT - Inngangsdor_statusregister - token:

  double AB_beboer = circusESP32.read(key_beboer, token_Inngangsdor_statusregister); // Get the old value From COT
  int A_beboer = AB_beboer / 10;
  AB_beboer = ((A_beboer + 1) * 10 + 0 ); // add new B value
  circusESP32.write(key_beboer, AB_beboer , token_Inngangsdor_statusregister);  // update value to COT
} //END FUNCTION WriteToCOT_ja_gjester_kontroller

//------------------------------Vist "Nei" i Meny-en --------------------------------------- (Universal)
int WriteToCOT_nei_gjester_kontroller(int counter){  // counter: er nummeret til beboer ( 1 til 6)

  char *myArray[] = {"24425","10799","9663","2277","31631","27545"}; // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[counter - 1];
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"; // CoT - Inngangsdor_statusregister - token:

  double AB_beboer = circusESP32.read(key_beboer, token_Inngangsdor_statusregister); // Get the old value From COT
  int A_beboer = AB_beboer / 10;
  AB_beboer = ((A_beboer) * 10 + 0 ); // add new B value
  circusESP32.write(key_beboer, AB_beboer , token_Inngangsdor_statusregister); // update value to COT
} //END FUNKTION WriteToCOT_nei_gjester_kontroller

//------------------------------ WriteToCOT__parameter_update_kontroller --------------------------------------- (Universal)
// dette er signalet RPI en trenger for at den skal sjekke "gjester" funsjonen
// Global_variabler2 -> parameter_update

void WriteToCOT__parameter_update_kontroller(char x) { // ask for number pos. A , B , C ( look at "Global_variabler2" );
  double num = circusESP32.read("4600", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");  // CoT - Global_variabler2 - parameter_update:

  int A = splitt_num(num, 3);
  int B = splitt_num(num, 2);
  int C = splitt_num(num, 1);

  if (x == 'A') {
    int new_num = (200 + (B * 10) + C);
    circusESP32.write("4600" , new_num , "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
  } else if ( x == 'B') {
    int new_num = ((A * 100) + 20 + C);
    circusESP32.write("4600" , new_num , "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
  } else if (x == 'C') {
    int new_num = ((A * 100) + (B * 10) + 2);
    circusESP32.write("4600" , new_num , "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8");
  }
} //END FUNCTION WriteToCOT__parameter_update_kontroller

//------------------------------ ekstra --------------------------------------- (Universal)
int splitt_num (int x, int place ){  // ansk for number "x", ask for num placement "place", return a singel siffer
  switch (place) {
    case 1: return x % 10; break;
    case 2: return (x / 10) % 10; break;
    case 3: return (x / 100) % 10; break;
    case 4: return (x / 1000) % 10; break;
    case 5: return (x / 10000) % 10; break;
    case 6: return (x / 100000) % 10; break;
    default: break;
  }
} //END FUNCTION splitt_num


//**************************Kode som ennå ikke er ferdig/implementert nedenfor her:****************************

//Kanseller booking funskjon

//Booking oversikt funksjon

//Værdata funksjon (mer spesifikk enn "info()" funksjonen)

//inneklima funksjon (må da få CO2 sensor på rom-modul til å sende data til CoT)

//Hurtigbooking av toalett/bad etc. En automatisering av koden i booking funksjonen,
// slik at første ledige tidspunkt for et rom blir booket.

/*
Legge inn styring via menypunktvariablene, slik at men returnerer til der en sto i menyen
når en funksjon valgt i menyen -og ikke til hovedmeny hver gang. Dette kan gjøres ved å sette riktig
X, Y og Z verdi i enden av kalte funksjoner og oppdatere menyen FØR funksjonen returnerer:

  menypunkt_X = 0; setter menynivå
  menypunkt_Y = 0; setter hovedmeny punkt
  menypunkt_Z = 0; setter undermeny1 punkt
  oppdaterMeny(menypunkt_X,menypunkt_Z,menypunkt_Y,menymax_Y,menymax_Z);
*/


//**************** Videre forbedringer, utvidelser...NEVER ENDING STORY!************
