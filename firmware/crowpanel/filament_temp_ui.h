#pragma once
/* FilamentRecycler HMI V0.4 - CrowPanel DIS07050H V3.0, LVGL 9.
 * Included in Elecrow reference sketch after hardware initialization.
 * All actuator actions are SIMULATED by the Pi and confirmed on screen.
 */
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HardwareSerial fr_uart(0);
static const uint32_t C_BG=0xFFFFFF, C_TEXT=0x141A22, C_MUTED=0x687584;
static const uint32_t C_CARD=0xF7F9FC, C_LINE=0xDEE5ED, C_BLUE=0x1677EB;
static const uint32_t C_RED=0xE34646, C_ORANGE=0xED921B, C_GREEN=0x159859;
static const uint32_t C_Z[3]={0xDE4949,0xE99420,0x1677EB};
static const int GRAPH_N=60;
static float actual[3]={NAN,NAN,NAN}, desired[3]={175,190,205};
static float pid_cfg[3][3]={{5,.1,15},{5,.1,15},{5,.1,15}};
static float rpm=12.5f, diameter=NAN, diameterTarget=1.75f;
static float histT[3][GRAPH_N], histD[GRAPH_N];
static int histIdx=0, histCount=0;
static uint32_t lastHistory=0, lastState=0, lastHello=0;
static bool megaOnline=false, piOnline=false;
static char machineMode[20]="READY", rxLine[256];
static size_t rxLen=0;

enum FrPage {FR_DASH, FR_TEMP, FR_FILAMENT, FR_SETTINGS, FR_GRAPH_TEMP, FR_GRAPH_DIA, FR_MATERIAL};
enum FrSetting {FR_SET_TEMP,FR_SET_PID,FR_SET_MOTOR,FR_SET_SYSTEM};
static FrPage page=FR_DASH;
static FrSetting settingPage=FR_SET_TEMP;
static int selectedZone=0;
static lv_obj_t *root=nullptr, *statusLabel=nullptr;
static lv_obj_t *dashboardValue[6]={}, *detailLabels[7]={};
static lv_obj_t *charts[4]={}, *graphCharts[2]={};
static lv_chart_series_t *chartSeries[4]={}, *graphSeries[2]={};
static lv_obj_t *dialog=nullptr, *editVal=nullptr;
static float editNumber=0, editMin=0, editMax=0, editStep=1;
static char editCmd[48];
static char pendingAction[12];

static const lv_font_t* font_big(){
#if LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#else
  return LV_FONT_DEFAULT;
#endif
}
static lv_obj_t* label(lv_obj_t *p,const char* txt,int x,int y,uint32_t color,bool large=false){
  lv_obj_t *o=lv_label_create(p); lv_label_set_text(o,txt); lv_obj_set_pos(o,x,y);
  lv_obj_set_style_text_color(o,lv_color_hex(color),0);
  if(large) lv_obj_set_style_text_font(o,font_big(),0);
  return o;
}
static void boxStyle(lv_obj_t *o,uint32_t bg,int radius=12){
  lv_obj_set_style_bg_color(o,lv_color_hex(bg),0);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
  lv_obj_set_style_border_color(o,lv_color_hex(C_LINE),0);
  lv_obj_set_style_border_width(o,1,0);
  lv_obj_set_style_radius(o,radius,0);
  lv_obj_set_style_shadow_width(o,0,0);
  lv_obj_set_style_pad_all(o,0,0);
  lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
}
static lv_obj_t* panel(lv_obj_t *p,int x,int y,int w,int h,uint32_t bg=C_CARD){
  lv_obj_t *o=lv_obj_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);boxStyle(o,bg);return o;
}
static lv_obj_t* button(lv_obj_t* p,const char* text,int x,int y,int w,int h,uint32_t bg,
                         lv_event_cb_t cb,void* data=nullptr){
  lv_obj_t* o=lv_button_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  boxStyle(o,bg,9);
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_t* t=lv_label_create(o);lv_label_set_text(t,text);
  lv_obj_set_style_text_color(t,lv_color_hex(bg==C_BLUE||bg==C_RED||bg==C_GREEN||bg==C_ORANGE?0xFFFFFF:C_TEXT),0);
  lv_obj_center(t);
  if(cb)lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data);
  return o;
}
static void sendLine(const char *s){fr_uart.println(s);}
static void drawPage();
static void updateDisplay();
static void nav(lv_event_t* e){
  page=(FrPage)(intptr_t)lv_event_get_user_data(e);drawPage();
}
static void chooseZone(lv_event_t* e){selectedZone=(int)(intptr_t)lv_event_get_user_data(e);drawPage();}
static void chooseSetting(lv_event_t* e){settingPage=(FrSetting)(intptr_t)lv_event_get_user_data(e);drawPage();}
static void closeDialog(lv_event_t*){if(dialog){lv_obj_delete(dialog);dialog=nullptr;editVal=nullptr;}}
static void confirmAction(lv_event_t*){
  if(!piOnline)return;
  char cmd[40];snprintf(cmd,sizeof(cmd),"ACTION,%s",pendingAction);sendLine(cmd);
  closeDialog(nullptr);
}
static void showConfirm(const char* action){
  if(dialog)return;
  snprintf(pendingAction,sizeof(pendingAction),"%s",action);
  dialog=panel(root,178,104,444,242,0xFFFFFF);
  lv_obj_move_foreground(dialog);
  label(dialog,"PROZESSBESTAETIGUNG",22,18,C_ORANGE,true);
  label(dialog,!strcmp(action,"START")?"Prozess wirklich starten?":
      !strcmp(action,"STOP")?"Prozess wirklich stoppen?":"Prozess pausieren?",22,76,C_TEXT);
  label(dialog,"Testmodus: Es werden keine Ausgaenge aktiviert.",22,112,C_MUTED);
  button(dialog,"Abbrechen",20,177,187,46,0xE8EDF3,closeDialog);
  button(dialog,"Bestaetigen",238,177,187,46,!strcmp(action,"STOP")?C_RED:C_GREEN,confirmAction);
}
static void clickStart(lv_event_t*){showConfirm("START");}
static void clickPause(lv_event_t*){showConfirm("PAUSE");}
static void clickStop(lv_event_t*){showConfirm("STOP");}
static void updateEditorLabel(){
  if(!editVal)return;
  char buf[32];snprintf(buf,sizeof(buf),"%.3f",editNumber);lv_label_set_text(editVal,buf);
}
static void adjustEditor(lv_event_t* e){
  int dir=(int)(intptr_t)lv_event_get_user_data(e);
  editNumber=fmaxf(editMin,fminf(editMax,roundf((editNumber+dir*editStep)*1000.0f)/1000.0f));
  updateEditorLabel();
}
static void saveEditor(lv_event_t*){
  if(!piOnline){
    if(editVal)lv_label_set_text(editVal,"PI OFFLINE");
    return;
  }
  char buf[100];snprintf(buf,sizeof(buf),"%s,%.3f",editCmd,editNumber);sendLine(buf);
  closeDialog(nullptr);
}
static void openEditor(const char* title,const char* command,float value,float minV,float maxV,float step){
  if(dialog)return;
  editNumber=value;editMin=minV;editMax=maxV;editStep=step;
  snprintf(editCmd,sizeof(editCmd),"%s",command);
  dialog=panel(root,196,98,410,254,0xFFFFFF);
  lv_obj_move_foreground(dialog);
  label(dialog,title,20,17,C_TEXT,true);
  label(dialog,"Wert einstellen",22,60,C_MUTED);
  button(dialog,"-",36,99,76,55,0xE8EDF3,adjustEditor,(void*)(intptr_t)-1);
  editVal=label(dialog,"",153,114,C_TEXT,true);updateEditorLabel();
  button(dialog,"+",296,99,76,55,0xE8EDF3,adjustEditor,(void*)(intptr_t)1);
  button(dialog,"Abbrechen",20,188,172,46,0xE8EDF3,closeDialog);
  button(dialog,"Speichern",216,188,172,46,C_BLUE,saveEditor);
}
// Code (1xx): temperature, (2xx): PID; (300): RPM; (400): filament target.
static void onEdit(lv_event_t* e){
  int code=(int)(intptr_t)lv_event_get_user_data(e),zone=selectedZone;
  char cmd[48];
  if(code>=100&&code<103){
    zone=code-100;snprintf(cmd,sizeof(cmd),"SET,TEMP,%d",zone);
    openEditor("Solltemperatur",cmd,desired[zone],0,250,1);return;
  }
  if(code>=200&&code<203){
    int term=code-200;const char* names[]={"KP","KI","KD"};
    snprintf(cmd,sizeof(cmd),"SET,PID,%d,%s",zone,names[term]);
    openEditor(names[term],cmd,pid_cfg[zone][term],0,term==0?100:(term==1?10:200),term==1?.01f:.5f);return;
  }
  if(code==300){openEditor("Motordrehzahl (Soll)","SET,MOTOR,RPM",rpm,0,150,.5f);return;}
  if(code==400){openEditor("Filamentdurchmesser Soll","SET,FILAMENT,TARGET",diameterTarget,1,3,.01f);return;}
}
static lv_obj_t* addChart(lv_obj_t* parent,int x,int y,int w,int h,int type,bool full){
  lv_obj_t* chart=lv_chart_create(parent);lv_obj_set_pos(chart,x,y);lv_obj_set_size(chart,w,h);
  lv_obj_set_style_bg_color(chart,lv_color_hex(0xFFFFFF),0);
  lv_obj_set_style_bg_opa(chart,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(chart,0,0);
  lv_obj_set_style_pad_all(chart,4,0);
  lv_chart_set_type(chart,LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart,GRAPH_N);
  lv_chart_set_div_line_count(chart,full?5:3,full?7:5);
  lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,type==3?130:-20,type==3?220:260);
  lv_chart_series_t *series=lv_chart_add_series(chart,lv_color_hex(type==3?C_BLUE:C_Z[type]),LV_CHART_AXIS_PRIMARY_Y);
  if(full){graphCharts[0]=chart;graphSeries[0]=series;}
  else {charts[type]=chart;chartSeries[type]=series;}
  return chart;
}
static void fillChart(lv_obj_t* chart,lv_chart_series_t* series,int type){
  if(!chart||!series)return;
  for(int i=0;i<GRAPH_N;i++){
    int idx=(histIdx+i)%GRAPH_N;
    float val= type==3?histD[idx]:histT[type][idx];
    int scaled=isfinite(val)?(int)roundf(val*(type==3?100:1)):LV_CHART_POINT_NONE;
    lv_chart_set_value_by_id(chart,series,i,scaled);
  }
  lv_chart_refresh(chart);
}
static void graphButton(lv_event_t* e){
  page=(FrPage)(intptr_t)lv_event_get_user_data(e);drawPage();
}
static void drawHeader(const char* title,bool back){
  panel(root,0,0,800,55,0xFFFFFF);
  if(back)button(root,"<",12,8,43,38,0xE8EDF3,nav,(void*)(intptr_t)FR_DASH);
  label(root,title,back?64:20,14,C_TEXT,true);
  statusLabel=label(root,"PI OFFLINE",595,18,C_RED);
}
static void drawFooter(){
  if(page!=FR_DASH)return;
  button(root,"STARTEN",19,414,239,53,C_GREEN,clickStart);
  button(root,"PAUSE",280,414,240,53,C_ORANGE,clickPause);
  button(root,"STOP",542,414,239,53,C_RED,clickStop);
}
static void dashCard(int index,const char* name,int x,int y,int w,int h,uint32_t accent){
  lv_obj_t* p=panel(root,x,y,w,h);
  label(p,name,12,10,accent);
  dashboardValue[index]=label(p,"--",12,35,C_TEXT,true);
  if(index<3){
    label(p,"Ist / Soll",12,71,C_MUTED);
    addChart(p,13,94,w-26,60,index,false);
    lv_obj_add_event_cb(p,nav,LV_EVENT_CLICKED,(void*)(intptr_t)FR_TEMP);
  }else if(index==4){
    addChart(p,13,79,w-26,65,3,false);
    lv_obj_add_event_cb(p,nav,LV_EVENT_CLICKED,(void*)(intptr_t)FR_FILAMENT);
  }else{
    lv_obj_add_event_cb(p,nav,LV_EVENT_CLICKED,(void*)(intptr_t)(index==3?FR_SETTINGS:FR_SETTINGS));
  }
  lv_obj_add_flag(p,LV_OBJ_FLAG_CLICKABLE);
}
static void drawDashboard(){
  drawHeader("FilamentRecycler",false);
  dashCard(0,"ZONE 1 - EINGANG",15,64,250,176,C_Z[0]);
  dashCard(1,"ZONE 2 - MITTE",275,64,250,176,C_Z[1]);
  dashCard(2,"ZONE 3 - DUESE",535,64,250,176,C_Z[2]);
  dashCard(3,"FOERDERSCHNECKE",15,248,250,157,C_GREEN);
  dashCard(4,"FILAMENTDURCHMESSER",275,248,250,157,C_BLUE);
  dashCard(5,"SYSTEM / EINSTELLUNGEN",535,248,250,157,C_MUTED);
  drawFooter();
}
static void drawTemperature(){
  drawHeader("Temperaturen",true);
  for(int i=0;i<3;i++){
    char name[30];snprintf(name,sizeof(name),"Zone %d",i+1);
    button(root,name,12,75+i*60,130,52,i==selectedZone?C_BLUE:0xE8EDF3,chooseZone,(void*)(intptr_t)i);
  }
  lv_obj_t* p=panel(root,155,65,631,325);
  detailLabels[0]=label(p,"Ist: --",18,14,C_TEXT,true);
  detailLabels[1]=label(p,"Soll: --",320,18,C_MUTED);
  addChart(p,18,65,590,195,selectedZone,true);
  button(p,"Graph gross",18,273,155,41,0xE8EDF3,graphButton,(void*)(intptr_t)FR_GRAPH_TEMP);
  button(p,"Sollwert aendern",352,273,255,41,C_BLUE,onEdit,(void*)(intptr_t)(100+selectedZone));
}
static void drawFilament(){
  drawHeader("Filamentdurchmesser",true);
  lv_obj_t* p=panel(root,14,65,772,325);
  detailLabels[0]=label(p,"Aktuell: --",20,17,C_TEXT,true);
  detailLabels[1]=label(p,"Soll: --",360,22,C_MUTED);
  addChart(p,20,75,728,195,3,true);
  button(p,"Graph gross",20,274,180,42,0xE8EDF3,graphButton,(void*)(intptr_t)FR_GRAPH_DIA);
  button(p,"Sollwert einstellen",476,274,270,42,C_BLUE,onEdit,(void*)(intptr_t)400);
  label(root,"Sensor noch nicht vorhanden: Istwert bleibt --",24,410,C_MUTED);
}
static void drawSettings(){
  drawHeader("Einstellungen",true);
  const char* names[]={"Temperaturen","PID-Regler","Motor","System"};
  for(int i=0;i<4;i++)button(root,names[i],12,76+i*77,170,65,i==(int)settingPage?C_BLUE:0xE8EDF3,
      chooseSetting,(void*)(intptr_t)i);
  lv_obj_t* p=panel(root,194,65,592,355);
  if(settingPage==FR_SET_TEMP){
    label(p,"SOLL-TEMPERATUREN",18,15,C_TEXT,true);
    for(int i=0;i<3;i++){
      char name[60];snprintf(name,sizeof(name),"Zone %d",i+1);
      label(p,name,22,82+i*76,C_Z[i]);
      detailLabels[i]=label(p,"--",175,82+i*76,C_TEXT,true);
      button(p,"Aendern",425,69+i*76,142,48,C_BLUE,onEdit,(void*)(intptr_t)(100+i));
    }
  }else if(settingPage==FR_SET_PID){
    label(p,"PID-PARAMETER",18,13,C_TEXT,true);
    for(int i=0;i<3;i++){
      char name[16];snprintf(name,sizeof(name),"Zone %d",i+1);
      button(p,name,18+i*185,54,175,45,i==selectedZone?C_BLUE:0xE8EDF3,chooseZone,(void*)(intptr_t)i);
    }
    const char* names2[]={"Kp - Proportional","Ki - Integral","Kd - Differential"};
    for(int i=0;i<3;i++){
      label(p,names2[i],20,128+i*65,C_MUTED);
      detailLabels[i]=label(p,"--",259,124+i*65,C_TEXT,true);
      button(p,"Aendern",423,115+i*65,141,43,C_BLUE,onEdit,(void*)(intptr_t)(200+i));
    }
  }else if(settingPage==FR_SET_MOTOR){
    label(p,"FOERDERSCHNECKE",20,18,C_TEXT,true);
    label(p,"Betrieb: TESTMODUS / Ausgaenge gesperrt",20,80,C_RED);
    label(p,"Soll-Drehzahl",20,140,C_MUTED);
    detailLabels[0]=label(p,"--",235,132,C_TEXT,true);
    button(p,"Aendern",423,126,141,52,C_BLUE,onEdit,(void*)(intptr_t)300);
    label(p,"Foerdermenge: wird spaeter kalibriert",20,230,C_MUTED);
  }else{
    label(p,"SYSTEM",20,17,C_TEXT,true);
    detailLabels[0]=label(p,"Raspberry Pi: --",20,90,C_TEXT);
    detailLabels[1]=label(p,"Arduino Mega: --",20,136,C_TEXT);
    label(p,"Parameter: auf Raspberry Pi gespeichert",20,190,C_MUTED);
    label(p,"Aktoren in dieser Version deaktiviert",20,235,C_RED);
    button(p,"Material / Spule (Vorschau)",22,279,337,45,0xE8EDF3,nav,(void*)(intptr_t)FR_MATERIAL);
  }
}
static void drawGraph(){
  drawHeader(page==FR_GRAPH_TEMP?"Temperaturverlauf":"Filamentdurchmesser-Verlauf",true);
  lv_obj_t *p=panel(root,15,65,770,355);
  graphCharts[0]=nullptr;graphSeries[0]=nullptr;
  addChart(p,22,25,726,262,page==FR_GRAPH_DIA?3:selectedZone,true);
  detailLabels[0]=label(p,"Ist: --",22,304,C_TEXT,true);
  detailLabels[1]=label(p,"Soll: --",358,307,C_MUTED);
  label(root,"Zeitfenster: letzte 60 Abtastungen (ca. 30 s)",22,435,C_MUTED);
}
static void drawMaterial(){
  drawHeader("Material / Spule",true);
  lv_obj_t *p=panel(root,15,65,770,335);
  label(p,"SPAETER: MATERIALVERBRAUCH / SPULENFUELLSTAND",20,25,C_TEXT,true);
  label(p,"Geplante Messwerte: g extrudiert, g verbleibend, Fuellgrad, Laufzeit",20,94,C_MUTED);
  label(p,"Aktuell keine Materialbilanz / kein Spulensensor angeschlossen.",20,145,C_MUTED);
}
static void drawPage(){
  root=lv_screen_active(); lv_obj_clean(root);
  lv_obj_set_style_bg_color(root,lv_color_hex(C_BG),0);
  lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
  lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
  memset(dashboardValue,0,sizeof(dashboardValue));
  memset(detailLabels,0,sizeof(detailLabels));
  memset(charts,0,sizeof(charts));
  memset(chartSeries,0,sizeof(chartSeries));
  memset(graphCharts,0,sizeof(graphCharts));
  memset(graphSeries,0,sizeof(graphSeries));
  dialog=nullptr;editVal=nullptr;statusLabel=nullptr;
  switch(page){
    case FR_DASH:drawDashboard();break;
    case FR_TEMP:drawTemperature();break;
    case FR_FILAMENT:drawFilament();break;
    case FR_SETTINGS:drawSettings();break;
    case FR_GRAPH_TEMP:case FR_GRAPH_DIA:drawGraph();break;
    case FR_MATERIAL:drawMaterial();break;
  }
  updateDisplay();
}
static void setLabel(lv_obj_t* o,const char* str){if(o)lv_label_set_text(o,str);}
static void updateDisplay(){
  char buf[80];
  if(statusLabel){
    const char *m=!strcmp(machineMode,"READY")?"BEREIT":!strcmp(machineMode,"PREHEAT")?"VORHEIZEN":!strcmp(machineMode,"PAUSED")?"PAUSE":machineMode;
    snprintf(buf,sizeof(buf),"%s | %s",piOnline?"PI OK":"PI OFFLINE",m);
    setLabel(statusLabel,buf);
    lv_obj_set_style_text_color(statusLabel,lv_color_hex(!piOnline?C_RED:!strcmp(machineMode,"PREHEAT")?C_ORANGE:C_GREEN),0);
  }
  if(page==FR_DASH){
    for(int i=0;i<3;i++){
      if(isfinite(actual[i]))snprintf(buf,sizeof(buf),"%.1f C",actual[i]);
      else snprintf(buf,sizeof(buf),"SENSOR ERR");
      setLabel(dashboardValue[i],buf);
    }
    snprintf(buf,sizeof(buf),"%.1f rpm",rpm);setLabel(dashboardValue[3],buf);
    if(isfinite(diameter))snprintf(buf,sizeof(buf),"%.2f mm",diameter);
    else snprintf(buf,sizeof(buf),"Sensor fehlt");
    setLabel(dashboardValue[4],buf);
    snprintf(buf,sizeof(buf),"Mega %s",megaOnline?"ONLINE":"OFFLINE");setLabel(dashboardValue[5],buf);
  }else if(page==FR_TEMP || page==FR_GRAPH_TEMP){
    snprintf(buf,sizeof(buf),"Ist: %s",isfinite(actual[selectedZone])?"":"ERR");
    if(isfinite(actual[selectedZone]))snprintf(buf,sizeof(buf),"Ist: %.1f C",actual[selectedZone]);
    setLabel(detailLabels[0],buf);
    snprintf(buf,sizeof(buf),"Soll: %.1f C",desired[selectedZone]);setLabel(detailLabels[1],buf);
  }else if(page==FR_FILAMENT || page==FR_GRAPH_DIA){
    if(isfinite(diameter))snprintf(buf,sizeof(buf),"Ist: %.3f mm",diameter);
    else snprintf(buf,sizeof(buf),"Ist: Sensor fehlt");
    setLabel(detailLabels[0],buf);
    snprintf(buf,sizeof(buf),"Soll: %.3f mm",diameterTarget);setLabel(detailLabels[1],buf);
  }else if(page==FR_SETTINGS){
    if(settingPage==FR_SET_TEMP){
      for(int i=0;i<3;i++){snprintf(buf,sizeof(buf),"%.1f C",desired[i]);setLabel(detailLabels[i],buf);}
    }else if(settingPage==FR_SET_PID){
      for(int i=0;i<3;i++){snprintf(buf,sizeof(buf),"%.3f",pid_cfg[selectedZone][i]);setLabel(detailLabels[i],buf);}
    }else if(settingPage==FR_SET_MOTOR){
      snprintf(buf,sizeof(buf),"%.1f rpm",rpm);setLabel(detailLabels[0],buf);
    }else{
      setLabel(detailLabels[0],piOnline?"Raspberry Pi: ONLINE":"Raspberry Pi: OFFLINE");
      setLabel(detailLabels[1],megaOnline?"Arduino Mega: ONLINE":"Arduino Mega: OFFLINE");
    }
  }
  for(int i=0;i<4;i++)fillChart(charts[i],chartSeries[i],i);
  if(graphCharts[0]) fillChart(graphCharts[0],graphSeries[0],page==FR_GRAPH_DIA?3:selectedZone);
}
static void addHistory(){
  for(int i=0;i<3;i++)histT[i][histIdx]=actual[i];
  histD[histIdx]=diameter;
  histIdx=(histIdx+1)%GRAPH_N;
  if(histCount<GRAPH_N)histCount++;
  updateDisplay();
}
static void processLine(char *line){
  if(!strcmp(line,"HELLO")){sendLine("ID,HMI,0.4");return;}
  if(!strcmp(line,"ACK,SAVED"))return;
  if(strncmp(line,"PID,",4)==0){
    int i;float kp,ki,kd;
    if(sscanf(line,"PID,%d,%f,%f,%f",&i,&kp,&ki,&kd)==4 && i>=0&&i<3){
      pid_cfg[i][0]=kp;pid_cfg[i][1]=ki;pid_cfg[i][2]=kd;
    }
    return;
  }
  if(strncmp(line,"STATE,",6)!=0)return;
  char *ctx=nullptr;char* tok[12]={};int count=0;
  for(char *p=strtok_r(line,",",&ctx);p && count<12;p=strtok_r(nullptr,",",&ctx))tok[count++]=p;
  // STATE,mode,mega,t1,t2,t3,sp1,sp2,sp3,rpm,diameter,diameterTarget
  if(count!=12)return;
  snprintf(machineMode,sizeof(machineMode),"%s",tok[1]);
  megaOnline=atoi(tok[2])!=0;
  for(int i=0;i<3;i++){
    float v=atof(tok[3+i]);actual[i]=strcmp(tok[3+i],"ERR")==0?NAN:v;
    desired[i]=atof(tok[6+i]);
  }
  rpm=atof(tok[9]);
  diameter=strcmp(tok[10],"ERR")==0?NAN:atof(tok[10]);
  diameterTarget=atof(tok[11]);
  piOnline=true;lastState=millis();
  if((uint32_t)(millis()-lastHistory)>=500){lastHistory=millis();addHistory();}
  else updateDisplay();
}
static void filament_ui_begin(){
  fr_uart.begin(115200,SERIAL_8N1,44,43);
  for(int i=0;i<GRAPH_N;i++){
    for(int j=0;j<3;j++)histT[j][i]=NAN;
    histD[i]=NAN;
  }
  page=FR_DASH;drawPage();sendLine("HELLO");
}
static void filament_ui_poll(){
  int budget=192;
  while(budget-- && fr_uart.available()){
    char c=(char)fr_uart.read();
    if(c=='\n'){rxLine[rxLen]=0;processLine(rxLine);rxLen=0;}
    else if(c!='\r'){
      if(rxLen<sizeof(rxLine)-1)rxLine[rxLen++]=c;
      else rxLen=0;
    }
  }
  if((uint32_t)(millis()-lastHello)>3000){lastHello=millis();sendLine("HELLO");}
  if(piOnline && (uint32_t)(millis()-lastState)>2500){
    piOnline=false;megaOnline=false;
    for(int i=0;i<3;i++)actual[i]=NAN;
    updateDisplay();
  }
}
