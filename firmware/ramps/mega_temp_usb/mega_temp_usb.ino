/* FilamentRecycler V0.4 - Arduino Mega 2560 / RAMPS 1.6
 * Three NTC thermistors + volatile configuration received from Raspberry Pi.
 * SAFETY: Heating outputs remain OFF. E0 motor output ONLY for timed, confirmed manual test.
 * No extrusion sequence is connected to motor tests.
 */
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

const uint8_t TEMP_PIN[3] = {A13, A15, A14};
const float R_NOMINAL = 100000.0f, T_NOMINAL = 25.0f, BETA = 3950.0f, R_PULLUP = 4700.0f;
float temperatures[3] = {NAN, NAN, NAN};
float targets[3] = {175,190,205};
float pid[3][3] = {{5,.1,15},{5,.1,15},{5,.1,15}};
float motorRPM = 12.5f, filamentTarget = 1.75f;
char cmd[96]; uint8_t cmdLen = 0;
unsigned long lastSample = 0;
// RAMPS 1.6 E0 pin map (Mega2560), TMC2208 STEP/DIR standalone
const uint8_t E0_STEP=26, E0_DIR=28, E0_ENABLE=24;
const uint16_t TEST_MIN_HZ=50, TEST_MAX_HZ=800;
const unsigned long TEST_MAX_MS=15000UL;
const unsigned long TEST_WATCHDOG_MS=1300UL;
bool testRunning=false, testForward=true;
uint16_t testHz=200;
unsigned long testStart=0, testKeep=0, lastStepUs=0, lastMotorReport=0;
void stopTest() {
  testRunning=false;
  digitalWrite(E0_ENABLE,HIGH);
  digitalWrite(E0_STEP,LOW);
}
void reportMotor() {
  Serial.print(F("MOTOR,TEST,")); Serial.print(testRunning?1:0);
  Serial.print(','); Serial.print(testHz);
  Serial.print(','); Serial.println(testForward?1:0);
}
void motorTick() {
  if(!testRunning) return;
  unsigned long now=millis();
  if(now-testStart >= TEST_MAX_MS || now-testKeep >= TEST_WATCHDOG_MS){
    stopTest(); Serial.println(F("ACK,MTEST,TIMEOUT")); return;
  }
  unsigned long us=micros();
  if((uint32_t)(us-lastStepUs)>=1000000UL/testHz){
    lastStepUs=us;
    digitalWrite(E0_STEP,HIGH); delayMicroseconds(4); digitalWrite(E0_STEP,LOW);
  }
}


float readTemperature(uint8_t pin) {
  analogRead(pin); // discard first conversion after channel change
  unsigned long sum = 0;
  for (uint8_t i=0;i<10;i++) sum += analogRead(pin);
  float adc = sum / 10.0f;
  if (adc<=2 || adc>=1021) return NAN;
  float r = R_PULLUP*adc/(1023.0f-adc);
  float invT = 1.0f/(T_NOMINAL+273.15f)+log(r/R_NOMINAL)/BETA;
  float t = 1.0f/invT-273.15f;
  return (isfinite(t) && t>=-20 && t<=300) ? t : NAN;
}
void sample() { for(uint8_t i=0;i<3;i++) temperatures[i]=readTemperature(TEMP_PIN[i]); }
void sendTemps() {
  Serial.print(F("TEMP"));
  for(uint8_t i=0;i<3;i++) {
    Serial.print(',');
    if(isfinite(temperatures[i])) Serial.print(temperatures[i],1);
    else Serial.print(F("ERR"));
  }
  Serial.println();
}
bool parseFloat(const char *s, float low, float high, float &out) {
  if(!s || !*s) return false;
  char *end;
  double value = strtod(s,&end);
  if(end==s || *end || !isfinite(value) || value<low || value>high) return false;
  out=(float)value; return true;
}
void handle(char *line) {
  if(!strcmp(line,"HELLO")) { Serial.println(F("ID,RAMPS_CONFIG,0.4")); return; }
  if(!strcmp(line,"PING")) { Serial.println(F("PONG")); return; }
  if(!strcmp(line,"GET_TEMP")) { sendTemps(); return; }
  if(!strcmp(line,"MTEST,STOP")) { stopTest(); Serial.println(F("ACK,MTEST,STOP")); reportMotor(); return; }
  if(!strcmp(line,"MTEST,KEEP")) {
    if(testRunning) testKeep=millis();
    return;
  }
  if(!strcmp(line,"MTEST,STATUS")) { reportMotor(); return; }
  if(!strncmp(line,"MTEST,START,",12)) {
    // START,stepHz,direction (direction 0 or 1); reject malformed commands.
    // Never allow motor to start on a stale or incomplete request.
    unsigned int rate=0; int direction=-1; char tail='x';
    if(sscanf(line,"MTEST,START,%u,%d%c",&rate,&direction,&tail)==2 &&
       rate>=TEST_MIN_HZ && rate<=TEST_MAX_HZ &&
       (direction==0 || direction==1) && !testRunning) {
      testHz=(uint16_t)rate; testForward=(direction==1);
      digitalWrite(E0_DIR,testForward?HIGH:LOW);
      delayMicroseconds(20);
      testStart=testKeep=millis(); lastStepUs=micros();
      digitalWrite(E0_ENABLE,LOW); testRunning=true;
      Serial.println(F("ACK,MTEST,START")); reportMotor();
    } else Serial.println(F("ERR,MTEST,START"));
    return;
  }
  if(!strncmp(line,"CFG,",4)) {
    char *parts[6]={}; uint8_t n=0;
    char *ctx=nullptr;
    for(char *p=strtok_r(line,",",&ctx); p && n<6; p=strtok_r(nullptr,",",&ctx)) parts[n++]=p;
    bool valid=false; float value;
    if(n==4 && !strcmp(parts[1],"TEMP")) {
      int idx=atoi(parts[2]);
      if(strlen(parts[2])==1 && parts[2][0]>='0' && parts[2][0]<='2' && parseFloat(parts[3],0,250,value)) {
        targets[idx]=value; valid=true;
      }
    } else if(n==5 && !strcmp(parts[1],"PID")) {
      int idx=atoi(parts[2]), term=-1;
      if(!strcmp(parts[3],"KP")) term=0;
      if(!strcmp(parts[3],"KI")) term=1;
      if(!strcmp(parts[3],"KD")) term=2;
      if(strlen(parts[2])==1 && parts[2][0]>='0' && parts[2][0]<='2' && term>=0 &&
         parseFloat(parts[4],0,term==0?100:(term==1?10:200),value)) {
        pid[idx][term]=value; valid=true;
      }
    } else if(n==4 && !strcmp(parts[1],"MOTOR") && !strcmp(parts[2],"RPM") && parseFloat(parts[3],0,150,value)) {
      motorRPM=value; valid=true;
    } else if(n==4 && !strcmp(parts[1],"FILAMENT") && !strcmp(parts[2],"TARGET") && parseFloat(parts[3],1,3,value)) {
      filamentTarget=value; valid=true;
    }
    Serial.println(valid?F("ACK,CFG"):F("ERR,CFG"));
    return;
  }
  Serial.println(F("ERR,UNKNOWN"));
}
void setup(){
  digitalWrite(E0_ENABLE,HIGH);
  digitalWrite(E0_STEP,LOW);
  digitalWrite(E0_DIR,LOW);
  pinMode(E0_ENABLE,OUTPUT);
  pinMode(E0_STEP,OUTPUT);
  pinMode(E0_DIR,OUTPUT);
  stopTest();
  Serial.begin(115200);
  analogReference(DEFAULT);
  sample(); lastSample=millis();
}
void loop(){
  if(millis()-lastSample>=500) { lastSample=millis(); sample(); }
  motorTick();
  if(millis()-lastMotorReport>=450){lastMotorReport=millis();reportMotor();}
  while(Serial.available()) {
    char c=(char)Serial.read();
    if(c=='\n') { cmd[cmdLen]=0; handle(cmd); cmdLen=0; }
    else if(c!='\r') { if(cmdLen<sizeof(cmd)-1) cmd[cmdLen++]=c; else cmdLen=0; }
  }
}
