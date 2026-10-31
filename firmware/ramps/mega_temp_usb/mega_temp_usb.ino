/* FilamentRecycler V0.4 - Arduino Mega 2560 / RAMPS 1.6
 * Three NTC thermistors + volatile configuration received from Raspberry Pi.
 * SAFETY: No motor or heater output pins configured. START does not actuate anything.
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
  Serial.begin(115200);
  analogReference(DEFAULT);
  sample(); lastSample=millis();
}
void loop(){
  if(millis()-lastSample>=500) { lastSample=millis(); sample(); }
  while(Serial.available()) {
    char c=(char)Serial.read();
    if(c=='\n') { cmd[cmdLen]=0; handle(cmd); cmdLen=0; }
    else if(c!='\r') { if(cmdLen<sizeof(cmd)-1) cmd[cmdLen++]=c; else cmdLen=0; }
  }
}
