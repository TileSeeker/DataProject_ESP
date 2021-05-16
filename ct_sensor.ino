#include <CircusESP32Lib.h>
#include <Arduino.h>
#include "EmonLib.h"
#include "WiFi.h"
#include <ESP32AnalogRead.h>

// GPIO pinnene (ADC input) for CT sensor
#define ADC_PIN1 34
#define ADC_PIN2 35
#define ADC_PIN3 32
#define ADC_PIN4 33
#define ADC_PIN5 36
#define ADC_PIN6 39

// spenning
#define HOME_VOLTAGE 230.0

// EmonLib varibler for å bruke 10bit ADC oppløsning
#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)

// lage emon instanser
EnergyMonitor emon1;
EnergyMonitor emon2;
EnergyMonitor emon3;
EnergyMonitor emon4;
EnergyMonitor emon5;
EnergyMonitor emon6;

//CoT
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1ODMyIn0.1sZoVBXJzmhJ71UiSxFxPcZ-3FufHWOO1UokVe0IBwA";
char server [] = "www.circusofthings.com";
char key_1 [] = "24060";
char key_2 [] = "9511";
char key_3 [] = "12024";
char key_4 [] = "2953";
char key_5 [] = "17975";
char key_6 [] = "4649";

//wifi information
const char ssid[] = "wifi";
const char psk[] = "password";

// Arrays for å lagere 120 målinger
short watt_data1[120];
short watt_data2[120];
short watt_data3[120];
short watt_data4[120];
short watt_data5[120];
short watt_data6[120];

short Index_data = 0;
unsigned long last_reading = 0;
unsigned long Setuptime_done = 0;

//wattHours variables
short watt_hours1;
short watt_hours2;
short watt_hours3;
short watt_hours4;
short watt_hours5;
short watt_hours6;

CircusESP32Lib circusESP32(server, ssid, psk);

//sjekk om wifi tilgjenglig funksjon
void connectWiFi() {
  Serial.println("WiFi...      ");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("Energi-monitor-sikringsskap");
  WiFi.begin(ssid, psk);

  // prøver å koble til netverk 10 ganger
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10){
    delay(500);
    Serial.print(".");
    retries++;
  }

	// Hvis ingen wifi, gå i sleepmode i et minutt, deretter prøv igjen.
	if(WiFi.status() != WL_CONNECTED){
  		esp_sleep_enable_timer_wakeup(1 * 60L * 1000000L);
		  esp_deep_sleep_start();
	}

  //print IP addressen til serielmonitor
  Serial.println(WiFi.localIP());
}

//funksjon for å hente sensordata
void get_sensordata() {
  double amps1 = emon1.calcIrms(1480);
  double watt1 = amps1 * HOME_VOLTAGE;
  watt_data1[Index_data] = watt1;

  double amps2 = emon2.calcIrms(1480);
  double watt2 = amps2 * HOME_VOLTAGE;
  watt_data2[Index_data] = watt2;

  double amps3 = emon3.calcIrms(1480);
  double watt3 = amps3 * HOME_VOLTAGE;
  watt_data3[Index_data] = watt3;

  double amps4 = emon4.calcIrms(1480);
  double watt4 = amps4 * HOME_VOLTAGE;
  watt_data4[Index_data] = watt4;

  double amps5 = emon5.calcIrms(1480);
  double watt5 = amps5 * HOME_VOLTAGE;
  watt_data5[Index_data] = watt5;

  double amps6 = emon6.calcIrms(1480);
  double watt6 = amps6 * HOME_VOLTAGE;
  watt_data6[Index_data] = watt6;
}

//funksjon som gjør om wattdata til wattimer
void wattdata_to_watthours() {
  watt_hours1 = 0;
  watt_hours2 = 0;
  watt_hours3 = 0;
  watt_hours4 = 0;
  watt_hours5 = 0;
  watt_hours6 = 0;
  for (short i = 0; i <=119; i++) {
    watt_hours1 += watt_data1[i];
    watt_hours2 += watt_data2[i];
    watt_hours3 += watt_data3[i];
    watt_hours4 += watt_data4[i];
    watt_hours5 += watt_data5[i];
    watt_hours6 += watt_data6[i];

  }
  watt_hours1 = watt_hours1 / 120;
  watt_hours2 = watt_hours2 / 120;
  watt_hours3 = watt_hours3 / 120;
  watt_hours4 = watt_hours4 / 120;
  watt_hours5 = watt_hours5 / 120;
  watt_hours6 = watt_hours6 / 120;
}

//funksjon laster opp COT
void upload_CoT() {
  circusESP32.write(key_1, watt_hours1, token);
  circusESP32.write(key_2, watt_hours2, token);
  circusESP32.write(key_3, watt_hours3, token);
  circusESP32.write(key_4, watt_hours4, token);
  circusESP32.write(key_5, watt_hours5, token);
  circusESP32.write(key_6, watt_hours6, token);
}

//setup funksjon: init pinnene, sjekker wifi connection
void setup() {
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
  analogReadResolution(10);
  Serial.begin(115200);

  connectWiFi();
  emon1.current(ADC_PIN1, 30);
  emon2.current(ADC_PIN2, 30);
  emon3.current(ADC_PIN3, 30);
  emon4.current(ADC_PIN4, 30);
  emon5.current(ADC_PIN5, 30);
  emon6.current(ADC_PIN6, 30);

  Setuptime_done = millis();
}

//lagerer data hvert 30 sekund i en time å laster opp til CoT
void loop() {
  unsigned long time_now = millis();

  if (time_now - last_reading > 30) {
    last_reading = millis();
    get_sensordata();
    Index_data++;
  }

  if (Index_data == 120) {
    wattdata_to_watthours();
    upload_CoT();
    Index_data = 0;
  }

}
