#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3c

#define BUZZER 5
#define LED_1  15   // alarm 1
#define LED_2  2    // alarm 2
#define LED_3  0    // alarm 3
#define LED_4  25   // temperature
#define LED_5  26   // humidity

#define PB_CANCEL 34
#define PB_DOWN 14
#define PB_OK 27
#define PB_UP 35

#define DHTPIN 12

#define NTP_SERVER     "pool.ntp.org"
// if time zone = +5:30, then UTC_SIGN = '+', UTC_HOUR = 5, UTC_MINUTE = 30
char UTC_SIGN = '+';
int UTC_MULTIPLIER = +1; //To multiply UTC_OFFSET by -1, if UTC_SIGN is equal to '-'  
// Initially the time zone is set to Sri Lankan time zone
int UTC_HOUR = 5;  
int UTC_MINUTE = 30;
int UTC_OFFSET = UTC_HOUR*3600 + UTC_MINUTE*60; // UTC_OFFSET = UTC_HOUR*3600 + UTC_MINUTE*60
#define UTC_OFFSET_DST 0

//-----------------------------------------------------------------------------------
//*******************************CODE FOR THE DASHBOARD******************************
#define LDR_LEFT 32
#define LDR_RIGHT 33

// for LDRs
int ldr_left_val;
int ldr_right_val;
float ldr_left_intensity;
float ldr_right_intensity;

char left_ldr_AR[6];
char right_ldr_AR[6];

// wifi
WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// for servo motor
Servo myservo; 

float theta = 0;
float theta_offset = 30;
float I_value;
float gammaValue  = 0.75;
float D_value;

// for temeperature sensor
char tempAR[6];
char humiAR[6];

bool isScheduledON = false;
unsigned long scheduledOnTime;
//-----------------------------------------------------------------------------------
//***********************************************************************************



// Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor; // temperature and humidity sensor

// Gloabal Variables
int LEDs[] = {LED_1, LED_2, LED_3};

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timenow = 0;
unsigned long timelast = 0;

bool alarm_enabled[] = {true,true,true};
int n_alarms = 3;
int alarm_hours[] = {0,1,0};
int alarm_minutes[] = {1,1,1};
bool alarm_triggered[] = {false,false,false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 7;
String modes[] = {"1- Set Time Zone", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3", "5 - Disable Alarm 1", "6 - Disable Alarm 2", "7 - Disable Alarm 3"};

void setup() {

  // For Buzzer
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(LED_5, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  
  // For DHT Sensor
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  
  Serial.begin(9600);
  
  // For Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.display();
  delay(2000); // Pause for 2 seconds

  display.clearDisplay();
  print_line("Welcome to medibox!", 2, 0, 0);
  delay(3000);
  
  // initializing the time using WiFi 
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 2, 0, 0);
  }
  display.clearDisplay();
  print_line("Connected to WIFI", 2, 0, 0);

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  // For Dashboard
  setupMqtt();

  timeClient.begin();
  timeClient.setTimeOffset(5.5 * 3600);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  pinMode(LDR_LEFT, INPUT);
  pinMode(LDR_RIGHT, INPUT);

  myservo.attach(4);   
}

void loop() {
  update_time_with_check_alarm();
  //-------------------------------------------------------------------------------
  if (!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();

  read_ldr_left();
  read_ldr_right();
  calculate_I_and_D(ldr_left_intensity,ldr_right_intensity);
  calculate_theta();
  check_temp();
  checkSchedule();

  
  Serial.print("Left LDR: ");
  Serial.println(left_ldr_AR);
  Serial.print("Right LDR: ");
  Serial.println(right_ldr_AR);
  Serial.println(I_value);
  Serial.println(D_value);
  Serial.print("theta :");
  Serial.println(theta);
  Serial.print("theta_offset :");
  Serial.println(theta_offset);
  Serial.print("gammaValue :");
  Serial.println(gammaValue);
  Serial.print("temp :");
  Serial.println(tempAR);
  Serial.println("humi :");
  Serial.println(humiAR);

  mqttClient.publish("Medibox-LDR-Left", left_ldr_AR);
  mqttClient.publish("Medibox-LDR-Right", right_ldr_AR);
  mqttClient.publish("Medibox-Temp", tempAR);
  mqttClient.publish("Medibox-Humidity", humiAR);

  myservo.write(theta);
  //-------------------------------------------------------------------------------
  
  if (digitalRead(PB_OK) == LOW){
    delay(200);
    go_to_menu();
  }
}

void print_line(String text, int text_size, int column, int row) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);

  display.display();
}

void print_time_now(void){
  display.clearDisplay();
  print_line(String(days), 2, 0, 0);
  print_line(":", 2, 20, 0);  
  print_line(String(hours), 2, 30, 0);
  print_line(":", 2, 50, 0);
  print_line(String(minutes), 2, 60, 0);
  print_line(":", 2, 80, 0);
  print_line(String(seconds), 2, 90, 0);

}

void update_time(void){
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);
}

void ring_alarm(void){
  display.clearDisplay();
  print_line("MEDICINE TIME!", 2, 0, 0);

  bool break_happen = false;

  // Ring the buzzer
  while (break_happen == false && digitalRead(PB_CANCEL) == HIGH){
    for (int i = 0; i<n_notes; i++){
      if (digitalRead(PB_CANCEL) == LOW){
        break_happen = true;
        delay(200);
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }
  
  display.clearDisplay();
}

void update_time_with_check_alarm(void){
  update_time();
  print_time_now();

  for(int i = 0; i<n_alarms; i++){//check each alarms one by one 
    if (alarm_enabled[i] && alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes){
      digitalWrite(LEDs[i], HIGH);// LED is on while ringing the alarm
      ring_alarm();
      digitalWrite(LEDs[i], LOW); // Turn off the LED
      alarm_triggered[i] = true;
    }
  }
}

int wait_for_button_press(){
  while (true){
    if (digitalRead(PB_UP) == LOW){
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW){
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW){
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW){
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void go_to_menu(void){
  while (digitalRead(PB_CANCEL) == HIGH){
    display.clearDisplay();
    print_line(modes[current_mode], 2, 0, 0);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      current_mode += 1;
      current_mode %= max_modes;
    }

    else if (pressed == PB_DOWN){
      delay(200);
      current_mode -= 1;
      if (current_mode < 0){
        current_mode = max_modes-1;
      } 
    }

    else if (pressed == PB_OK){
      delay(200);
      Serial.println(current_mode);
      run_mode(current_mode);
    }

    if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }
}

void set_alarm(int alarm){
  int temp_hour = alarm_hours[alarm];

  while (true){
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 2, 0, 0);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_hour += 1;
      temp_hour %= 24;
    }

    else if (pressed == PB_DOWN){
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0){
        temp_hour = 23;
      } 
    }

    else if (pressed == PB_OK){
      delay(200);
      alarm_hours[alarm] = temp_hour;
      alarm_enabled[alarm] = true; // If a alrm is set, Enable it again
      alarm_triggered[alarm] = false;
      break;
    }

    if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  } 

  int temp_minute = alarm_minutes[alarm];

  while (true){
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 2, 0, 0);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_minute += 1;
      temp_minute %= 60;
    }

    else if (pressed == PB_DOWN){
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0){
        temp_minute = 59;
      } 
    }

    else if (pressed == PB_OK){
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      alarm_enabled[alarm] = true; // If a alrm is set, Enable it again
      alarm_triggered[alarm] = false;
      break;
    }

    if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }  

  display.clearDisplay();
  print_line("Alarm is set", 2, 0, 0); 
  delay(1000); 
}

void set_time_zone(void){
  int temp_hour = UTC_HOUR;
  int temp_minute = UTC_MINUTE;
  String signs[] = {"-", "+"};
  int temp_multiplier = UTC_MULTIPLIER;

  // To update the sign of the UTC
  while(true){
    display.clearDisplay();
    print_line("Enter Time Zone's Sign: " + signs[(temp_multiplier+1)/2], 2, 0, 0);
    int pressed = wait_for_button_press();

    if (pressed == PB_UP){
      delay(200);
      temp_multiplier *= -1;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_multiplier *= -1;
    }
    else if (pressed == PB_OK){
      delay(200);
      UTC_MULTIPLIER = temp_multiplier;
      break;
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  // To update the hour of the UTC
  while(true){
    display.clearDisplay();
    int hour_range;
    if (UTC_MULTIPLIER == -1){
      hour_range = 12;
      if (UTC_HOUR == 13 || UTC_HOUR == 14){
        UTC_HOUR = 0;
        temp_hour = UTC_HOUR;
      }
    }
    else{
      hour_range = 14;
    }

    print_line("Enter Time Zone's Hour: " + String(temp_hour), 2, 0, 0);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP){
      delay(200);
      temp_hour = ((temp_hour + 1) % (hour_range + 1));
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_hour = temp_hour - 1;
      if (temp_hour<0){
        temp_hour = hour_range;
      }
    }
    else if (pressed == PB_OK){
      delay(200);
      UTC_HOUR = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  // To update the minute of the UTC
  while(true){
    // To keep the time zone -12:00 to +14:00
    if ((UTC_MULTIPLIER == -1 && UTC_HOUR == 12 ) || UTC_HOUR == 14){
      UTC_MINUTE = 0;
      break;
    }
    display.clearDisplay();
    print_line("Enter Time Zone's Minute: " + String(temp_minute), 2, 0, 0);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP){
      delay(200);
      temp_minute +=5; // increse minitues of the time zone 5 by 5
      temp_minute %= 60;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_minute = temp_minute - 5;
      if (temp_minute<0){
        temp_minute = 55;
      }
    }
    else if (pressed == PB_OK){
      delay(200);
      UTC_MINUTE = temp_minute;
      break;
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }
  
  UTC_OFFSET = UTC_MULTIPLIER*(UTC_HOUR*3600 + UTC_MINUTE*60);

  display.clearDisplay();
  print_line("Time Zone is set", 2, 0, 0);
  delay(1000);
}

void run_mode(int mode){
  if (mode == 0){
    set_time_zone();
  }
  else if (mode == 1 || mode == 2 || mode ==3){
    set_alarm(mode - 1);
  }

  else if (mode == 4){
    alarm_enabled[mode-4] = false;
    display.clearDisplay();
    print_line("Alarm 1 is disabled", 2, 0, 0);
    delay(1000);
  }
  else if (mode == 5){
    alarm_enabled[mode-4] = false;
    display.clearDisplay();
    print_line("Alarm 2 is disabled", 2, 0, 0);
    delay(1000);
  }
  else if (mode == 6){
    alarm_enabled[mode-4] = false;
    display.clearDisplay();
    print_line("Alarm 3 is disabled", 2, 0, 0);
    delay(1000);
  }
}

void check_temp(void){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAR, 6);
  String(data.humidity, 2).toCharArray(humiAR, 6);
  if (data.temperature > 32){
    display.clearDisplay();
    print_line("TEMP HIGH", 1, 0, 40);
    digitalWrite(LED_4,HIGH);
  }
  else if (data.temperature < 26){
    display.clearDisplay();
    print_line("TEMP LOW", 1, 0, 40);
    digitalWrite(LED_4,HIGH);
  }
  else{
    digitalWrite(LED_4,LOW);
  }

  if (data.humidity > 80){
    display.clearDisplay();
    print_line("HUMIDITY HIGH", 1, 0, 50);
    digitalWrite(LED_5,HIGH);
  }
  else if (data.humidity < 60){
    display.clearDisplay();
    print_line("HUMIDITY LOW", 1, 0, 50);
    digitalWrite(LED_5,HIGH);
  }
  else{
    digitalWrite(LED_5,LOW);
  }
}


//-----------------------------------------------------------------------------------
//*******************************CODE FOR THE DASHBOARD******************************

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

// To Subscribe the MQTT topics
void connectToBroker(){
  while (!mqttClient.connected()){
    Serial.println("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-12345678")){
      Serial.println("connected");
      mqttClient.subscribe("Medibox-Minimum-Angle");
      mqttClient.subscribe("Medibox-Controlling-Factor");
      mqttClient.subscribe("Medibox-Select-Medicine");
      mqttClient.subscribe("Medibox-SCH-ON");
    }
    else{
      Serial.println("failed");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }  
}

// To receive data from the MQTT
void receiveCallback(char* topic, byte* payload, unsigned int  length){
  Serial.print("Messege arrived [");
  Serial.print(topic);
  Serial.println("] ");

  char payloadCharAr[length];
  for (int i = 0; i<length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.println();

  // To get the theta_offset
  if (strcmp(topic, "Medibox-Minimum-Angle") == 0){
    theta_offset = atol(payloadCharAr);
  }

  // To  get the controling factor
  if (strcmp(topic, "Medibox-Controlling-Factor") == 0){
    gammaValue = atof(payloadCharAr);
    Serial.println(gammaValue);
    Serial.println("------------------------------------------");

  }

  // Drop down menu for selecting medicine
  if (strcmp(topic, "Medibox-Select-Medicine") == 0){
    // Medicine A
    if (payloadCharAr[0] == 'A'){
      char payloadStr[6];
      dtostrf(0.1, 4, 2, payloadStr); 
      mqttClient.publish("Medibox-ConFactor", payloadStr);

      dtostrf(50, 2, 0, payloadStr); 
      mqttClient.publish("Medibox-MinAngle", payloadStr);
    }
    // Medicine B
    else if (payloadCharAr[0] == 'B'){
      char payloadStr[6];
      dtostrf(0.2, 4, 2, payloadStr); 
      mqttClient.publish("Medibox-ConFactor", payloadStr);

      dtostrf(60, 2, 0, payloadStr); 
      mqttClient.publish("Medibox-MinAngle", payloadStr);
    } 
    // Medicine C
    else if (payloadCharAr[0] == 'C'){
      char payloadStr[6];
      dtostrf(0.3, 4, 2, payloadStr); 
      mqttClient.publish("Medibox-ConFactor", payloadStr);

      dtostrf(90, 2, 0, payloadStr); 
      mqttClient.publish("Medibox-MinAngle", payloadStr);
    } 

    // Default
    else if (payloadCharAr[0] == 'D'){
      char payloadStr[6];
      dtostrf(0.75, 4, 2, payloadStr); 
      mqttClient.publish("Medibox-ConFactor", payloadStr);

      dtostrf(30, 2, 0, payloadStr); 
      mqttClient.publish("Medibox-MinAngle", payloadStr);
    } 
  } 

  // To set the Alarm
  if (strcmp(topic, "Medibox-SCH-ON") == 0){
    if (payloadCharAr[0] == 'N'){
      isScheduledON = false;
    }
    else{
      scheduledOnTime = atol(payloadCharAr);
      unsigned long currentTime = getTime();
      if(currentTime>scheduledOnTime){
        isScheduledON = false;
        mqttClient.publish("Medibox-SWITCH-ON", "0");
        mqttClient.publish("Medibox-Notify", "0");
      }
      else{
        isScheduledON = true;
        mqttClient.publish("Medibox-Notify", "1");
      }
    }
  } 
}

// alarm buzzer for the dashboard alarm
void buzzerOn(bool on){
  if(on){
    tone(BUZZER,256);
  }
  else{
    noTone(BUZZER);
  }
}

// to get the real time
unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime();
}

// to check the time for ringing the alarm 
void checkSchedule(){
  if(isScheduledON){
    unsigned long currentTime = getTime();
    if(currentTime>scheduledOnTime){
      buzzerOn(true);
      Serial.println("Alarm is ON");
    }
  }
  else{
    buzzerOn(false);
  }  
}


// left ldr
void read_ldr_left(){
  ldr_left_val = analogRead(LDR_LEFT);
  ldr_left_intensity = mapFloat(ldr_left_val, 32, 4064, 0.0, 1.0); // Map analog reading to range 1 to 0
  String(ldr_left_intensity, 2).toCharArray(left_ldr_AR, 6);
}

// right ldr
void read_ldr_right(){
  ldr_right_val = analogRead(LDR_RIGHT);
  ldr_right_intensity = mapFloat(ldr_right_val,32,4064,0.0,1.0);
  String(ldr_right_intensity, 2).toCharArray(right_ldr_AR, 6);
}

// for mapping the float values
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_max) * (out_max - out_min) / (in_min - in_max); 
}

// For calculating the I and D values
void calculate_I_and_D(float intensity_of_left_ldr,float intensity_of_right_ldr){
  if (intensity_of_left_ldr>intensity_of_right_ldr){
    D_value = 1.5;
  }
  else{
    D_value = 0.5;
  }
  I_value = max(intensity_of_left_ldr,intensity_of_right_ldr);
}

// For calculating the angle
void calculate_theta(){
  theta = min(theta_offset*D_value + (180 - theta_offset)*I_value*gammaValue ,float(180)); 
}




