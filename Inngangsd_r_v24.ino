#include <CircusESP32Lib.h>  // https://circusofthings.com/
#include <SPI.h>
#include <MFRC522.h>         // https://github.com/miguelbalboa/rfid

// COT:
char ssid[] = "sollia26";
char password[] = "Guttasollia6";
char server[] = "www.circusofthings.com";
CircusESP32Lib circusESP32(server, ssid, password);

// RFID - SPI communication:
// MISO   pin 19 used in SPI communication             (SPI)
// MISI   pin 23 used in SPI communication             (SPI)
// SCK    pin 18 used in SPi communication             (SPI)
#define SS_PIN 21 // Slave select pin, SDA             (SPI)
#define RST_PIN 22 // Reset pin, SCL                   (SPI)
byte nuidPICC[4];                   // Init array that will store new NUID
MFRC522 rfid(SS_PIN, RST_PIN);      // Instance of the class

// I/O -list:
#define button_opp 32 // Button for counting up         (input)
#define button_ned 33 // Button for counting down       (input)
#define button_enter 14 // Button for enter             (input)
#define led1 25 // Red led                              (Output) 
#define led2 26 // Yellow led                           (Output)
#define led3 27 // Green led                            (Output)
#define display_A  0 // Seven-segment display BCD       (Output)
#define display_D  4 // Seven-segment display BCD       (Output) 
#define display_C  5 // Seven-segment display BCD       (Output)
#define display_B  16 // Seven-segment display BCD      (Output)
#define ledRed_RFIO 2 // led Red                        (Output)                     
#define ledGreen_RFIO 15 // led Green                   (Output)    
#define ledBlue_RFIO 13 // led Yellow                   (Output) 
#define ledYellow_RFIO 12 // led Blue                   (Output) 

int counter_meny = 2;           // cCount up and down, from 1 to 6
unsigned long time_now = 0;     // Time from start
unsigned long time_button = 0;  // Time from last button pressed
int period_button = 700;        // How often the buttons registrer new input
unsigned long time_led = 0;     // Time from last led got activated
int period_led = 5000;          // When led turn off
int led_toggle = 0;             // If led is on, reset timer for led off

void setup() {
  Serial.begin(115200);
  circusESP32.begin();  // Init COT
  SPI.begin();          // Init SPI bus
  rfid.PCD_Init();      // Init MFRC522

  pinMode(button_opp, INPUT_PULLUP);
  pinMode(button_ned, INPUT_PULLUP);
  pinMode(button_enter, INPUT_PULLUP);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(display_A, OUTPUT);
  pinMode(display_D, OUTPUT);
  pinMode(display_C, OUTPUT);
  pinMode(display_B, OUTPUT);
  pinMode(ledRed_RFIO, OUTPUT);
  pinMode(ledGreen_RFIO, OUTPUT);
  pinMode(ledBlue_RFIO, OUTPUT);
  pinMode(ledYellow_RFIO, OUTPUT);
}

void loop() {
  time_now = millis();
  int state_button_opp = digitalRead(button_opp);
  int state_button_ned = digitalRead(button_ned);
  int state_button_enter = digitalRead(button_enter);

  // 1) ------ Meny -----
  if ((time_now >= (time_button + period_button)) and (state_button_opp == LOW or state_button_ned == LOW)) { // "Opp" or "ned" is pressded:
    time_button += period_button;
    counter_meny = Meny_uppdown(counter_meny, state_button_opp, state_button_ned);  // Uppdate counter_meny variabel:
    Print_nummber (counter_meny);  // Print number to seven segment display
  }

  // 2) ------ Gjest -----
  if ((time_now >= time_button + period_button) and (state_button_enter == LOW)) {    // "Enter" is pressded:
    time_button += period_button;
    WriteToCOT_antall_hybel ();      // Will update a global variabel on COT, corona safety
    Gjester(counter_meny);           // Will alert "Beboer", and give feedback to "Gjest".
  }

  // 3) ------ RFID -----
  RFIO();

  // 4) ------ LED -----
  Led();
}

// ---------------------------------------------- Meny_uppdown -------------------------------------------------------------------
// Denne funksjonen endrer en teller fra 1 til 6 og retunerer denne. 
int Meny_uppdown(int counter, int state_button_opp, int state_button_ned ) { // Input: beboer num, Button state upp, button state down. 
  if ((state_button_opp == LOW) and (counter < 6)) {
    counter += 1;
  } else if ((state_button_ned == LOW) and (counter > 1)) {
    counter -= 1;
  } else if ((state_button_opp == LOW) and ((counter == 6) or ((counter > 6)))) {
    counter = 1;
  } else if ((state_button_ned == LOW) and ((counter == 1) or ((counter < 1)))) {
    counter = 6;
  }
  return counter;
}

// ------------------------------------------ Print_nummber ---------------------------------------------------------------------
// Denne funksjonen printer ut en BCD kode fra 1 til 6. 
int Print_nummber (int x) { // input: num from 1 to 6 
  if (x == 1) {
    digitalWrite(display_A, HIGH);
    digitalWrite(display_B, LOW);
    digitalWrite(display_C, LOW);
    digitalWrite(display_D, LOW);
    Serial.println("1");
  } else if (x == 2) {
    digitalWrite(display_A, LOW);
    digitalWrite(display_B, HIGH);
    digitalWrite(display_C, LOW);
    digitalWrite(display_D, LOW);
    Serial.println("2");
  } else if (x == 3) {
    digitalWrite(display_A, HIGH);
    digitalWrite(display_B, HIGH);
    digitalWrite(display_C, LOW);
    digitalWrite(display_D, LOW);
    Serial.println("3");
  } else if (x == 4) {
    digitalWrite(display_A, LOW);
    digitalWrite(display_B, LOW);
    digitalWrite(display_C, HIGH);
    digitalWrite(display_D, LOW);
    Serial.println("4");
  } else if (x == 5) {
    digitalWrite(display_A, HIGH);
    digitalWrite(display_B, LOW);
    digitalWrite(display_C, HIGH);
    digitalWrite(display_D, LOW);
    Serial.println("5");
  } else if (x == 6) {
    digitalWrite(display_A, LOW);
    digitalWrite(display_B, HIGH);
    digitalWrite(display_C, HIGH);
    digitalWrite(display_D, LOW);
    Serial.println("6");
  } else {
    digitalWrite(display_A, HIGH);
    digitalWrite(display_B, HIGH);
    digitalWrite(display_C, HIGH);
    digitalWrite(display_D, HIGH);
    Serial.println("Out of range");
  }
}

// ---------------------------------------------- Gjester ------------------------------------------------------------------------
// Denne funksjonen har som oppgave å ta i mot en ny gjest. Når funksjonen vil hente verdier fra COT, disse blir sammenlikna med hver andre. 
// Den har 3 utfall: "Gjest ikke  tillat", "venter på svar fra beboer" og "Gjest godkjent" 
void Gjester( int counter) {

  // Retrive values from COT
  int A_beboer          = ReadFromCOT_gjester_inngangsdor(counter, 'A');
  int B_beboer          = ReadFromCOT_gjester_inngangsdor(counter, 'B');
  int C_max_gjester     = ReadFromCOT_gjester_inngangsdor(counter, 'C');
  int D_max_hybel       = ReadFromCOT_gjester_inngangsdor(counter, 'D');
  int E_antal_hybel     = ReadFromCOT_gjester_inngangsdor(counter, 'E');  
  int F_lokasjon_person = ReadFromCOT_gjester_inngangsdor(counter, 'F');

  if ( (A_beboer == C_max_gjester) or ( D_max_hybel < E_antal_hybel) or ( F_lokasjon_person == 0)) {   // No guests allowed
    digitalWrite(led1, HIGH);
    Serial.println("inside 1 ");
    return;
  } else if (B_beboer == 1) {                                                                           // Beboer have someone waiting for him/her already
    digitalWrite(led2, HIGH);
    Serial.println("inside 2 ");
    return;
  } else if ((A_beboer < C_max_gjester ) and (B_beboer == 0) and (F_lokasjon_person > 0)) {             // Beboer will get massage on the screen on his/her controller
    digitalWrite(led3, HIGH);
    WriteToCOT_gjester_inngangsdor(counter);
    Serial.println("inside 3 ");
    return;
  }
}

// -------------------------------------------COT_read_gjester (Underlagt: Gjester) -----------------------------------
// Denne funksjonen er underlagt "Gjester". Den har som oppgave å hente ønska verdi fra COT. 
// Funsjonen spør om beboer number og en boksatv fra A til F. Denne bokstaven vil hente en bestemt verdi fra COT. 
// (Tips: Sjå Flytkjema for "gjester" i vedlegg 1. Bokstavene er relatert til dette)
int ReadFromCOT_gjester_inngangsdor(int counter, char X) { //input: Beboer number, Letter from A to F

  // 1) ------ Retrive Keys -----
  char *myArray[] = {"27545", "31631", "2277", "9663", "10799", "24425"};   // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[counter - 1];
  char *myArray2[] = {"19808", "24769", "20536", "3430", "2105", "10015"};  // CoT - PersonX_statusregister - key
  char *key_lokasjon_person = myArray2[counter - 1] ;
  char key_max_gjester  [] = "27067";   // CoT - Global_konstanter1 - key
  char key_max_hybel    [] = "23508";   // CoT - Global_konstanter1 - key
  char key_antal_hybel  [] = "3624";    // CoT - Global_variabler2 - key

  // 2) ------ Retrive Tokens -----
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"; // CoT - Inngangsdor_statusregister - token:
  char token_Global_konstanter1[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTcxIn0.0PyyZPjdG8-Hr5LxA2pNGWNq2oJAMRxdKAtFy_n-7MY";         // CoT - Global_konstanter1 - token:
  char token_Global_variabler2[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8";          // CoT - Global_variabler2 - token:
  char *myArray3[] = {"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk1In0.9bD-g6Gi40yjEiEYGOY1eoWl0wEuAZZN67yzS5gYOQs", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk2In0.zADZlTeWjpJpbEmG_d1mcw07mDtT9ZJ30sMDtU1ex80", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk3In0.es9iHyTEfrYM3ksN0QWtiULhRlQEcwatXWHFc5_fscc", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk4In0.WP5pJqMaPL8AwEEii2TMFys9kQUabpl2iztyxBRdLuc", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk5In0.fbZfuVuDshnMdUMW6EXrB6fSYhtdq0l2-j92h6AtlbM", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAwIn0.nGZXNRU34wVFtzex9tS-0gVky_ppn3Gjmj_riA4oLZY"};
  char *token_Person_statusregister = myArray3[counter - 1];                                                                        // CoT - PersonX_statusregister - token:

  // 3) ------ Retrive valus from COT -----
  if (X ==  'A') {
    double AB_beboer = circusESP32.read(key_beboer, token_Inngangsdor_statusregister);              // From Inngangsdor_statusregister
    int A_beboer = get_tens (AB_beboer);
    return A_beboer; // Return number of Guests

  } else if (X  ==  'B') {
    double AB_beboer = circusESP32.read(key_beboer, token_Inngangsdor_statusregister);              // From Inngangsdor_statusregister
    int B_beboer = get_ones (AB_beboer);
    return B_beboer; // Return status og beboer 

  } else if (X  == 'C') {
    double C_max_gjester = circusESP32.read(key_max_gjester, token_Global_konstanter1);             // From Global_konstanter1
    return C_max_gjester; // Return max limit guests 

  } else if (X ==  'D') {
    double D_max_hybel = circusESP32.read(key_max_hybel, token_Global_konstanter1);                 // From Global_konstanter1
    return D_max_hybel; // Return max limit in dorm 

  } else if (X  ==  'E') {
    double E_antal_hybel = circusESP32.read(key_antal_hybel, token_Global_variabler2);              // From Global_variabler2 
    return E_antal_hybel; // Return number of peole in dorm  

  } else if (X == 'F') {
    double F_lokasjon_person = circusESP32.read(key_lokasjon_person, token_Person_statusregister);  // From PersonX_statusregister

    if (F_lokasjon_person > 0 ) { // Return where the "beboer" is  
      return 1;
    } else {
      return 0;
    }
  }
}
// ------------------------------------------- COT_write_gjester (Underlagt: Gjester) ---------------------------------------------------------------
// Denne funsjonen har som oppgave å sende varsel til "kontrolleren" om at det er en gjest som venter ved døren. 
// Den henter et 2 siffret signal fra "Inngangsdor_statusregister" i COT. Den endrer sifferet på enerplassen til "1".

void WriteToCOT_gjester_inngangsdor(int counter)  { // Beboer number 
  Serial.println("WriteToCOT__gjester_inngangsdor");

  // CoT - Inngangsdor_statusregister - keylist:
  char *myArray[] = {"27545", "31631", "2277", "9663", "10799", "24425"};
  char *key_beboer = myArray[counter - 1];

  // CoT - Inngangsdor_statusregister - token:
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U";

  double AB_beboer = circusESP32.read(key_beboer , token_Inngangsdor_statusregister); // Get the old value From COT
  int A_beboer = get_tens (AB_beboer);
  int AB = ( A_beboer * 10 + 1 );                                                     // Add new B value

  circusESP32.write(key_beboer, AB , token_Inngangsdor_statusregister);               // Update value to COT
}
// ------------------------------------------ Uppdate_antall_hybel ----------------------------------------------------------------------------
// Denne funksjonen har som oppgave å endre "antall_hybel" variabelen i statusregisteret "globale_variabler_2".
// funksjonen lese alle relevante verdier fra COT, legger disse sammen og oppdaterer "antal_hybel"
// Dette er en kritisk funksjonen. Den er laget for å hindre at flere en max antall personer kommer inn i hybelen!!!
// (Denne funksjonen er en stor funksjon som teker tid, den skal bare kjøres når en ny person skal inn i hybelen) 

void WriteToCOT_antall_hybel () {
  Serial.println("Uppdate_antall_hybel: ");
  // 1) ------ Keys -----
  char key_beboer1  [] = "27545";   // CoT - Inngangsdor_statusregister - key for "beboer1"
  char key_beboer2  [] = "31631";   // CoT - Inngangsdor_statusregister - key for "beboer2"
  char key_beboer3  [] = "2277";    // CoT - Inngangsdor_statusregister - key for "beboer3"
  char key_beboer4  [] = "9663";    // CoT - Inngangsdor_statusregister - key for "beboer4"
  char key_beboer5  [] = "10799";   // CoT - Inngangsdor_statusregister - key for "beboer5"
  char key_beboer6  [] = "24425";   // CoT - Inngangsdor_statusregister - key for "beboer6"

  char key_antal_hybel  [] = "3624";        // CoT - Global_variabler2 - key for "Antal_hybel"
  char key_lokasjon_person1  [] = "19808";  // CoT - Person1_statusregister - key for "lokasjon_person1"
  char key_lokasjon_person2  [] = "24769";  // CoT - Person2_statusregister - key for "lokasjon_person2"
  char key_lokasjon_person3  [] = "20536";  // CoT - Person3_statusregister - key for "lokasjon_person3"
  char key_lokasjon_person4  [] = "3430";   // CoT - Person4_statusregister - key for "lokasjon_person4"
  char key_lokasjon_person5  [] = "2105";   // CoT - Person5_statusregister - key for "lokasjon_person5"
  char key_lokasjon_person6  [] = "10015";  // CoT - Person6_statusregister - key for "lokasjon_person6"

  // 2) ------ Tokens -----
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"; // CoT - Inngangsdor_statusregister - token:
  char token_Global_variabler2[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjcwIn0.4GshD9I6ZBE0roZzIsjHpIBLasIbH0JLc3TRhJwxJg8";          // CoT - Global_variabler2 - token:
  char token_Person1_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk1In0.9bD-g6Gi40yjEiEYGOY1eoWl0wEuAZZN67yzS5gYOQs";     // CoT - Person1_statusregister - token:
  char token_Person2_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk2In0.zADZlTeWjpJpbEmG_d1mcw07mDtT9ZJ30sMDtU1ex80";     // CoT - Person2_statusregister - token:
  char token_Person3_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk3In0.es9iHyTEfrYM3ksN0QWtiULhRlQEcwatXWHFc5_fscc";     // CoT - Person3_statusregister - token:
  char token_Person4_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk4In0.WP5pJqMaPL8AwEEii2TMFys9kQUabpl2iztyxBRdLuc";     // CoT - Person4_statusregister - token:
  char token_Person5_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk5In0.fbZfuVuDshnMdUMW6EXrB6fSYhtdq0l2-j92h6AtlbM";     // CoT - Person5_statusregister - token:
  char token_Person6_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAwIn0.nGZXNRU34wVFtzex9tS-0gVky_ppn3Gjmj_riA4oLZY";     // CoT - Person6_statusregister - token:

  // 3) ------ Retrive valus from COT -----
  double beboer1 = circusESP32.read(key_beboer1, token_Inngangsdor_statusregister); // CoT - Inngangsdor_statusregister
  double beboer2 = circusESP32.read(key_beboer2, token_Inngangsdor_statusregister); // CoT - Inngangsdor_statusregister
  double beboer3 = circusESP32.read(key_beboer3, token_Inngangsdor_statusregister); // CoT - Inngangsdor_statusregister
  double beboer4 = circusESP32.read(key_beboer4, token_Inngangsdor_statusregister); // CoT - Inngangsdor_statusregister
  double beboer5 = circusESP32.read(key_beboer5, token_Inngangsdor_statusregister); // CoT - Inngangsdor_statusregister
  double beboer6 = circusESP32.read(key_beboer6, token_Inngangsdor_statusregister); // CoT - Inngangsdor_statusregister

  int G_beboer1 = get_tens(beboer1);
  int H_beboer2 = get_tens(beboer2);
  int I_beboer3 = get_tens(beboer3);
  int J_beboer4 = get_tens(beboer4);
  int K_beboer5 = get_tens(beboer5);
  int L_beboer6 = get_tens(beboer6);

  double A_lokasjon_person1 = circusESP32.read(key_lokasjon_person1, token_Person1_statusregister); // CoT - Person1_statusregister
  double B_lokasjon_person2 = circusESP32.read(key_lokasjon_person2, token_Person2_statusregister); // CoT - Person2_statusregister
  double C_lokasjon_person3 = circusESP32.read(key_lokasjon_person3, token_Person3_statusregister); // CoT - Person3_statusregister
  double D_lokasjon_person4 = circusESP32.read(key_lokasjon_person4, token_Person4_statusregister); // CoT - Person4_statusregister
  double E_lokasjon_person5 = circusESP32.read(key_lokasjon_person5, token_Person5_statusregister); // CoT - Person5_statusregister
  double F_lokasjon_person6 = circusESP32.read(key_lokasjon_person6, token_Person6_statusregister); // CoT - Person6_statusregister

  if (A_lokasjon_person1 != 0) {
    A_lokasjon_person1 = 1;
  }
  if (B_lokasjon_person2 != 0) {
    A_lokasjon_person1 = 1;
  }
  if (C_lokasjon_person3 != 0) {
    A_lokasjon_person1 = 1;
  }
  if (D_lokasjon_person4 != 0) {
    A_lokasjon_person1 = 1;
  }
  if (E_lokasjon_person5 != 0) {
    A_lokasjon_person1 = 1;
  }
  if (F_lokasjon_person6 != 0) {
    A_lokasjon_person1 = 1;
  }

  // 4) ------ Add Values -----
  int M_globale_variabler = ( G_beboer1 + H_beboer2 + I_beboer3 + J_beboer4 + K_beboer5 + L_beboer6 + A_lokasjon_person1 + B_lokasjon_person2 + C_lokasjon_person3 + D_lokasjon_person4 + E_lokasjon_person5 + F_lokasjon_person6);

  // 5) ------ Write to COT -----
  circusESP32.write(key_antal_hybel , M_globale_variabler , token_Global_variabler2);
}

// ------------------------------------------ RFID_function ----------------------------------------------------------------------------
// Dette er en funksjon som sjekker kontinuerlig om det er en brikke i nærheten. Vist en brikke er detektert vil funksjonen sjekke den unike UDI-en til brikka...
// opp imot hver Beboer sin UDI. Vist det ikke stemmer vil rød led lyse. Vist det stemmer vil grøn led lyse og "PersonX_statusregister" endres til omvende av hva den var. (borte / hjemme) 
// samstundes som "Inngangsdor_statusregister" vil endre beboeren sine gjester til 0.

void RFID() {
  //------ Unique UDI beboer -----
  byte Bebeor1_card[4] = {215, 0, 169, 6};    // RFID brikke beboer 1
  byte Bebeor2_card[4] = {83, 172, 140, 185}; // RFID brikke beboer 2
  // add more unique UDI here...

  // 1) ------ Feel for RFID-tag ----- (If no Tag near exit the void)
  if ( ! rfid.PICC_IsNewCardPresent())  // No RFID-tag on the antenna. Jump out of "RFID"
    return;
  if ( ! rfid.PICC_ReadCardSerial())   // RFID-tag detected
    return;

  // 2) ------ compare the RFID-UDI to database -----
  if (Bebeor1_card[0] == rfid.uid.uidByte[0] ||   // Beboer 1
      Bebeor1_card[1] == rfid.uid.uidByte[1] ||
      Bebeor1_card[2] == rfid.uid.uidByte[2] ||
      Bebeor1_card[3] == rfid.uid.uidByte[3] ) {
    digitalWrite(ledGreen_RFIO, HIGH);
    counter = 1;

  } else if (Bebeor2_card[0] == rfid.uid.uidByte[0] || // Beboer 2 
             Bebeor2_card[1] == rfid.uid.uidByte[1] ||
             Bebeor2_card[2] == rfid.uid.uidByte[2] ||
             Bebeor2_card[3] == rfid.uid.uidByte[3] ) {
    digitalWrite(ledGreen_RFIO, HIGH);
    counter = 2;

    // compare more UDI here...

  } else {
    digitalWrite(ledRed_RFIO, HIGH); // User not found 
    return;
  }

  // 3) ------ Retrive Keys -----
  char *myArray[] = {"27545", "31631", "2277", "9663", "10799", "24425"};   // CoT - Inngangsdor_statusregister - keylist:
  char *key_beboer = myArray[counter - 1];
  char *myArray2[] = {"19808", "24769", "20536", "3430", "2105", "10015"};  // CoT - PersonX_statusregister - key
  char *key_lokasjon_person = myArray2[counter - 1] ;

  // 4) ------ Retrive Tokens -----
  char *myArray3[] = {"eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk1In0.9bD-g6Gi40yjEiEYGOY1eoWl0wEuAZZN67yzS5gYOQs", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk2In0.zADZlTeWjpJpbEmG_d1mcw07mDtT9ZJ30sMDtU1ex80", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk3In0.es9iHyTEfrYM3ksN0QWtiULhRlQEcwatXWHFc5_fscc", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk4In0.WP5pJqMaPL8AwEEii2TMFys9kQUabpl2iztyxBRdLuc", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NTk5In0.fbZfuVuDshnMdUMW6EXrB6fSYhtdq0l2-j92h6AtlbM", "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NjAwIn0.nGZXNRU34wVFtzex9tS-0gVky_ppn3Gjmj_riA4oLZY"};
  char *token_Person_statusregister = myArray3[counter - 1];                                                                        // CoT - PersonX_statusregister - token:
  char token_Inngangsdor_statusregister[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1NDk1In0.EiWIxyQlEo84QAN1FwQe-q810LxQ1u1UOTjRGEwNW5U"; // CoT - Inngangsdor_statusregister - token:

  // 5) ------ Read valus from COT -----
  double A_lokasjon_person = circusESP32.read(key_lokasjon_person, token_Person_statusregister); // CoT - Inngangsdor_statusregister

  // 5) ------ Write valus to COT -----
  if (A_lokasjon_person != 0) {
    circusESP32.write(key_beboer ,  0 , token_Inngangsdor_statusregister);
    circusESP32.write(key_lokasjon_person , 0 , token_Person_statusregister);
    digitalWrite(ledBlue_RFIO, HIGH);
  } else {
    circusESP32.write(key_lokasjon_person , 1 , token_Person_statusregister);
    digitalWrite(ledYellow_RFIO, HIGH);
  }
}

// ------------------------------------------ LED ----------------------------------------------------------------------------
// Denne funsjonen skrur av ledene etter en vis tid 
void Led() {
  if ((led_toggle == 0) and ((digitalRead(led1) == HIGH) or (digitalRead(led2) == HIGH) or (digitalRead(led3) == HIGH) or (digitalRead(ledRed_RFIO) == HIGH) or (digitalRead(ledGreen_RFIO) == HIGH) or (digitalRead(ledBlue_RFIO) == HIGH) or (digitalRead(ledYellow_RFIO) == HIGH))) {
    time_led = millis();
    led_toggle = 1;
    Serial.println("Led is on");
  }
  if ((time_now > (time_led + period_led)) and (led_toggle == 1)) {   // "Led" is  activated
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
    digitalWrite(led3, LOW);
    digitalWrite(ledRed_RFIO, LOW);
    digitalWrite(ledGreen_RFIO, LOW);
    digitalWrite(ledBlue_RFIO, LOW);
    digitalWrite(ledYellow_RFIO, LOW);
    led_toggle = 0;
  }
}

// ------------------------------------------ Splitt number from int -------------------------------------------------------------------------
// Disse funksjonene har som oppgave å splitte et nummer og retunere verdien til et siffer: 
int get_ones (int x) {    // only 2 siffer num
  int ones = x % 10;  //ones  B
  return ones;
}
int get_tens (int x) {    // only 2 siffer num
  int tens = x / 10;  //ones  A
  return tens;
}
int get_num (int x, int place ) { // Get a number "x" and the desired digit placement "place". Return a singel digit.
  switch (place) {
    case 1:
      return x % 10;
      break;
    case 2:
      return (x / 10) % 10;
      break;
    case 3:
      return (x / 100) % 10;
      break;
    case 4:
      return (x / 1000) % 10;
      break;
    case 5:
      return (x / 10000) % 10;
      break;
    case 6:
      return (x / 100000) % 10;
      break;
    default:
      break;
  }
}
