#include <WiFi.h>
#include <PubSubClient.h>
#include "HX711.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

#define ON 1
#define OFF 0
#define motor 5
#define botao1 13
#define botao2 12
#define botao3 14
#define botao4 27
#define botao5 26
#define CALIBRATION_FACTOR 123.81
#define CALIBRATION_FACTOR2 -470.72
#define tareReservatorio -1896
#define tareTigela -250

const char* ssid = "Kakashi27";
const char* password = "ga87654321";
const char* mqtt_server = "io.adafruit.com";
const char* mqtt_device = "dispenserIPET"; //nome do dispositivo, pode ser qualquer um (único)
const char* mqtt_user = "guduh";
const char* mqtt_password = "aio_zsEd73MUdNZNQ91WCSubI5fA6QhO";
const char* topico_motor = "guduh/feeds/ipet.motor";
const char* topico_camera = "guduh/feeds/ipet.camera";
const char* topico_reservatorio = "guduh/feeds/ipet.reservatorio";
const char* topico_tigela = "guduh/feeds/ipet.tigela";
const char* topico_agendamento = "guduh/feeds/ipet.agendamento";

const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;
const int LOADCELL_DOUT_PIN2 = 16;
const int LOADCELL_SCK_PIN2 = 4;
static const int servoPin = 18;

Preferences preferences;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

WiFiClient espClient;
PubSubClient client(espClient);

LiquidCrystal_I2C lcd(0x27,16,2);

Servo servo1;

HX711 scale;
HX711 scale2;

double posicaoCamera = 0;
int direcaoCamera = 0;
const int velocidadeCamera = 5;
int 
estadoMotor = 0;

int ea1=0,eu1=0,ea2=0,eu2=0,ea3=0,eu3=0,ea4=0,eu4=0, eb1=OFF, eb2=OFF, eb3=OFF, eb4=OFF,jm1=0,jm2=0,jm3=0,jm4=0,ea5=0,eu5=0,eb5=OFF,jm5=0;
int h1=0,h2=0,m1=0,m2=0,modo=1, umodo=1, jaligou = 0;
double peso=0, peso2= 0;
double reading, reading2;
long lastTime = 0;

void setup_wifi();
void setup_mqtt();
void setup_ota();
void setup_RTC();
void setup_balancas();
void setup_servoMotor();
void callback(char* topic, byte* payload, unsigned int length);
String payload_to_string();
void reconnect();

void cameraRight();
void cameraLeft();
void handleMotor();
void ligarMotor();
void desligarMotor();
void sendDataBalanca();
void agendarHorario();
void setCurrentTimeDisplay();
void setPesoReseervatorio();
void setPesoTigela();


int horario1 = -1;
int minuto1 = -1;
int horarioAnt;
int minutoAnt;
void setup() {
  Serial.begin(115200);
  setup_wifi();
  setup_mqtt();
  setup_ota();

  lcd.init(); // Inicializa LCD
  lcd.clear();
  lcd.backlight();

  pinMode(botao1 , INPUT_PULLUP); // Inicializa botoes
  pinMode(botao2 , INPUT_PULLUP);
  pinMode(botao3 , INPUT_PULLUP);
  pinMode(botao4 , INPUT_PULLUP);
  pinMode(botao5 , INPUT_PULLUP);

  pinMode(motor, OUTPUT); // Inicializa motor
  digitalWrite(motor, HIGH);

  delay(2000);
  setup_balancas();

  setup_servoMotor();

  setup_RTC();
  
  preferences.begin("iPET", false);
  horario1 = preferences.getInt("horario1", -1);
  minuto1 = preferences.getInt("minuto1", -1);
  preferences.end();
  setCurrentTimeDisplay();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  
  handleMotor();
  servo1.write(posicaoCamera);
  delay(15);

  setPesoReservatorio(round(scale.get_units()));
  setPesoTigela(round(scale2.get_units()));

  ea1=!digitalRead(13);
  ea2=!digitalRead(12);
  ea3=!digitalRead(14);
  ea4=!digitalRead(27);
  ea5=!digitalRead(26);

  if(ea5==1 && eu5==0 && eb5==OFF) eb5=ON;

  if(ea5==0 && eu5==0) {eb5=OFF; jm5=0;}

  if(ea5==1 && eu5==0 && eb5==ON && jm5==0) {modo=!modo; jm5=1;}

  if(modo!=umodo){
    lcd.clear();
  }

  if(modo==0){
    
    if(ea1==1 && eu1==0 && eb1==OFF) eb1=ON;
    if(ea2==1 && eu2==0 && eb2==OFF) eb2=ON;
    if(ea3==1 && eu3==0 && eb3==OFF) eb3=ON;
    if(ea4==1 && eu4==0 && eb4==OFF) eb4=ON;
    if(ea5==1 && eu5==0 && eb5==OFF) eb5=ON;

    if(ea1==0 && eu1==0) {eb1=OFF; jm1=0;}
    if(ea2==0 && eu2==0) {eb2=OFF; jm2=0;}
    if(ea3==0 && eu3==0) {eb3=OFF; jm3=0;}
    if(ea4==0 && eu4==0) {eb4=OFF; jm4=0;}
    if(ea5==0 && eu5==0) {eb5=OFF; jm5=0;}
    
    if(ea1==1 && eu1==0 && eb1==ON && jm1==0) {h1++; jm1=1;}
    if(ea2==1 && eu2==0 && eb2==ON && jm2==0) {h2++; jm2=1;}
    if(ea3==1 && eu3==0 && eb3==ON && jm3==0) {m1++; jm3=1;}
    if(ea4==1 && eu4==0 && eb4==ON && jm4==0) {m2++; jm4=1;}
    if(ea5==1 && eu5==0 && eb5==ON && jm5==0) {modo=!modo; jm5=1;}
    
    if((10*h1+h2)>=24){
      h1=0;
      h2=0;
    }
    if((10*m1+m2)>=60){
      m1=0;
      m2=0;
    }
    if(h2>9){
      h2=0;
    }
    if(m2>9){
      m2=0;
    }
    lcd.setCursor(5,0);
    lcd.print(h1);
    lcd.print(h2);
    lcd.print(":");
    lcd.print(m1);
    lcd.print(m2);
    lcd.setCursor(0,1);
    lcd.print("Agendamento");
  }
  peso=reading;
  peso2 = reading2;
  if(modo==1){
    if(ea3 && !eu3){
      cameraLeft();
    }
    
    if (ea4 && !eu4){
      cameraRight();
    }
    if (ea1 && !eu1)
      ligarMotor();
    else if (!ea1 && eu1)
      desligarMotor();

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Reserva: ");
    if(peso<0) {
      lcd.print("0");
      lcd.print(" Kg");
    }
    else{
      lcd.print(peso);
      lcd.print(" Kg");
    }
    lcd.setCursor(0,1);
    lcd.print("Tigela: ");
    if(peso2<0) {
      lcd.print(0);
      lcd.print(" g");
    }
    else {
      lcd.print(peso2);
      lcd.print(" g");
    } 
  }

  sendDataBalanca(8000);

  if(modo==1 && modo!=umodo) {
    agendarHorario(10*h1+h2, 10*m1+m2);
  }

  if(modo==0 && modo!=umodo) {
    setCurrentTimeDisplay();
  }

  if (horario1 == timeClient.getHours() && minuto1 == timeClient.getMinutes() && jaligou == 0) {
    horarioAnt = horario1;
    minutoAnt = minuto1;
    jaligou = 1;
    if (reading2 <= 150) {
      ligarMotor();
    }
  }

  if (jaligou && (reading2 > 100)) {
    desligarMotor();
  }

  if (horarioAnt != timeClient.getHours() || minutoAnt != timeClient.getMinutes()) {
    jaligou = 0;
  }

  eu1=ea1;
  eu2=ea2;
  eu3=ea3;
  eu4=ea4;
  eu5=ea5;

  umodo=modo;
  
}

void setup_wifi() {
  Serial.print("connect ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP addredd: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  bool horaLida = false;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message = payload_to_string(payload, length);
  Serial.print(message);
  Serial.println("");
  String topico = topic;
  if (topico.equals(topico_motor)) {
    estadoMotor = message.toInt();
  } else if (topico.equals(topico_camera)) {
    if (message.toInt()) {
      cameraRight();
    } else {
      cameraLeft();
    }
  } else if (topico.equals(topico_agendamento)) {
    String hour = "";
    String minute = "";
    for (int i = 0; i < length; i++) {
      if (message[i] == ':') {
        horaLida = true;
        continue;
      }
      if (!horaLida)
        hour += message[i];
      else
        minute += message[i];
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Agendamento APP");
    lcd.setCursor(5, 1);
    lcd.print(hour);
    lcd.print(":");
    lcd.print(minute);
    delay(5000);
    agendarHorario(hour.toInt(), minute.toInt());
  }
}

  String payload_to_string(byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += ((char)payload[i]);
  }
  return message;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_device, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(topico_motor);
      client.subscribe(topico_camera);
      client.subscribe(topico_agendamento);
    } else {
      Serial.print("failed, state: ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);

    }
  }
}

void sendDataBalanca(int timeDelay) {
  if (millis() - lastTime >= timeDelay) {
    String reservatorio = String(reading, 1);
    String tigela = String(reading2, 1);
    client.publish(topico_reservatorio, reservatorio.c_str());
    client.publish(topico_tigela, tigela.c_str());
    Serial.println("Sending data");
    lastTime = millis(); 
  }
}

void cameraRight() {
  if (posicaoCamera < 180) {
    for (int i = posicaoCamera; i <= posicaoCamera+60; i++){
      servo1.write(i);
      delay(15);
    }
    posicaoCamera = posicaoCamera+60;
  }
}

void cameraLeft() {
  if (posicaoCamera > 0) {
    for(int i=posicaoCamera; i >= posicaoCamera-60; i--){
      servo1.write(i);
      delay(15);
    }
    posicaoCamera = posicaoCamera-60;
  }
}

void handleMotor(){
  digitalWrite(motor, !estadoMotor);
}

void ligarMotor() {
  estadoMotor = 1;
}

void desligarMotor() {
  estadoMotor = 0;
}

void setup_ota() {

  ArduinoOTA.setHostname("iPET");

  ArduinoOTA.setPassword("12345678");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  
}

void setup_RTC() {
  timeClient.setTimeOffset(-10800); //GMT -3
  timeClient.begin();
  timeClient.update();
}

void setup_balancas() {
  Serial.println("Initializing the scale");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);   // Inicializa balanca 1
  //scale.tare();     

  scale2.begin(LOADCELL_DOUT_PIN2, LOADCELL_SCK_PIN2);
  scale2.set_scale(CALIBRATION_FACTOR2);   // Inicializa balanca 2
  //scale2.tare();    
}

void setup_servoMotor() {
  servo1.attach(servoPin);
  for(int i = 180; i>=0; i--){
    servo1.write(i);
    delay(15);
  }
}


void agendarHorario(int hora, int minuto) {
  horario1 = hora;
  minuto1 = minuto;
  preferences.begin("iPET", false);
  preferences.putInt("horario1", horario1);
  preferences.putInt("minuto1", minuto1);
  preferences.end();
}

void setCurrentTimeDisplay() {
  h1 = timeClient.getHours() / 10;
  h2 = timeClient.getHours() % 10;
  m1 = timeClient.getMinutes() / 10;
  m2 = timeClient.getMinutes() % 10;
}

void setPesoReservatorio(double peso) {
  reading = (double) (peso + tareReservatorio) / 1000;
}

void setPesoTigela(double peso) {
  reading2 = peso + tareTigela;
}