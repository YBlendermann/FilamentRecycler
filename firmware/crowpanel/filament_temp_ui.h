#pragma once
/* FilamentRecycler HMI V0.5 - dark layout - CrowPanel DIS07050H V3.0, LVGL 9.
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
static const uint32_t C_BG=0x0C1319, C_TEXT=0xF6F7F9, C_MUTED=0xA1AFB9;
static const uint32_t C_CARD=0x161F27, C_LINE=0x34434E, C_BLUE=0xFF7925;
static const uint32_t C_RED=0xE73D45, C_ORANGE=0xFF7925, C_GREEN=0x159859;
static const uint32_t C_Z[3]={0xFF7925,0xF3B33A,0xE9474C};
static const int GRAPH_N=60;
static float actual[3]={NAN,NAN,NAN}, desired[3]={175,190,205};
static float pid_cfg[3][3]={{5,.1,15},{5,.1,15},{5,.1,15}};
static float rpm=12.5f, diameter=NAN, diameterTarget=1.75f;
static bool motorTestRunning=false, motorTestForward=true;
static int motorTestHz=200;
static uint32_t motorTestBegin=0, lastMotorKeep=0;
static lv_obj_t *motorTestLabel=nullptr, *motorSpeedLabel=nullptr, *motorDirLabel=nullptr;
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
static lv_obj_t *dashAux[12]={}, *feedValue[3]={}, *coolValue=nullptr;
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
  dialog=panel(root,178,104,444,242,C_CARD);
  lv_obj_move_foreground(dialog);
  label(dialog,"PROZESSBESTAETIGUNG",22,18,C_ORANGE,true);
  label(dialog,!strcmp(action,"START")?"Prozess wirklich starten?":
      !strcmp(action,"STOP")?"Prozess wirklich stoppen?":"Prozess pausieren?",22,76,C_TEXT);
  label(dialog,"Testmodus: Es werden keine Ausgaenge aktiviert.",22,112,C_MUTED);
  button(dialog,"Abbrechen",20,177,187,46,0x2A343E,closeDialog);
  button(dialog,"Bestaetigen",238,177,187,46,!strcmp(action,"STOP")?C_RED:C_GREEN,confirmAction);
}
static void clickStart(lv_event_t*){showConfirm("START");}
static void clickPause(lv_event_t*){showConfirm("PAUSE");}
static void clickStop(lv_event_t*){showConfirm("STOP");}
static void openMotorSettings(lv_event_t*){settingPage=FR_SET_MOTOR;page=FR_SETTINGS;drawPage();}
static void updateMotorTestUi();
static void changeTestSpeed(lv_event_t* e){
  if(motorTestRunning)return;
  int delta=(int)(intptr_t)lv_event_get_user_data(e);
  motorTestHz+=delta;
  if(motorTestHz<50)motorTestHz=50;
  if(motorTestHz>1600)motorTestHz=1600;
  updateMotorTestUi();
}
static void toggleMotorDirection(lv_event_t*){
  if(motorTestRunning)return;
  motorTestForward=!motorTestForward;
  updateMotorTestUi();
}
static void stopMotorTest(lv_event_t*){sendLine("MTEST,STOP");}
static void confirmMotorTest(lv_event_t*){
  if(!piOnline || !megaOnline){closeDialog(nullptr);return;}
  char cmd[48];snprintf(cmd,sizeof(cmd),"MTEST,START,%d,%d",motorTestHz,motorTestForward?1:0);
  sendLine(cmd);
  motorTestBegin=millis();lastMotorKeep=0;
  closeDialog(nullptr);
}
static void startMotorTest(lv_event_t*){
  if(dialog || !piOnline || !megaOnline || motorTestRunning)return;
  dialog=panel(root,178,104,444,242,C_CARD);
  lv_obj_move_foreground(dialog);
  label(dialog,"E0-MOTORTEST STARTEN?",20,17,C_ORANGE,true);
  label(dialog,"Nur ohne Last. Motorstrom/VREF vorher pruefen!",20,66,C_TEXT);
  label(dialog,"Max. 15 s; Stop jederzeit. Keine Heizfreigabe.",20,104,C_MUTED);
  button(dialog,"Abbrechen",20,178,187,44,0x2A343E,closeDialog);
  button(dialog,"Start bestaetigen",231,178,192,44,C_GREEN,confirmMotorTest);
}
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
  dialog=panel(root,196,98,410,254,C_CARD);
  lv_obj_move_foreground(dialog);
  label(dialog,title,20,17,C_TEXT,true);
  label(dialog,"Wert einstellen",22,60,C_MUTED);
  button(dialog,"-",36,99,76,55,0x2A343E,adjustEditor,(void*)(intptr_t)-1);
  editVal=label(dialog,"",153,114,C_TEXT,true);updateEditorLabel();
  button(dialog,"+",296,99,76,55,0x2A343E,adjustEditor,(void*)(intptr_t)1);
  button(dialog,"Abbrechen",20,188,172,46,0x2A343E,closeDialog);
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
  lv_obj_set_style_bg_color(chart,lv_color_hex(C_CARD),0);
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
// Industrial iconography drawn with native LVGL geometry so no external icon font is needed.
static lv_obj_t* iconDot(lv_obj_t* parent,int x,int y,int size,uint32_t color){
  lv_obj_t* d=lv_obj_create(parent);
  lv_obj_set_pos(d,x,y);lv_obj_set_size(d,size,size);
  lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_color(d,lv_color_hex(color),0);
  lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(d,0,0);lv_obj_set_style_pad_all(d,0,0);
  lv_obj_clear_flag(d,LV_OBJ_FLAG_SCROLLABLE);
  return d;
}
static void thermoIcon(lv_obj_t* p,int x,int y,uint32_t c){
  lv_obj_t* stem=panel(p,x+6,y,8,22,c);
  lv_obj_set_style_border_width(stem,0,0);
  lv_obj_set_style_radius(stem,6,0);
  iconDot(p,x+2,y+16,17,c);
  iconDot(p,x+6,y+20,9,C_CARD);
  iconDot(p,x+8,y+22,5,c);
}
static void fanIcon(lv_obj_t* p,int x,int y,uint32_t c){
  iconDot(p,x+8,y+8,7,c);
  iconDot(p,x+7,y-1,10,c);
  iconDot(p,x+16,y+9,10,c);
  iconDot(p,x-1,y+12,10,c);
  iconDot(p,x+10,y+10,3,C_CARD);
}
static void metricIcon(lv_obj_t* p,int x,int y,uint32_t c){
  lv_obj_t* ring=iconDot(p,x,y,20,c);
  lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);
  lv_obj_set_style_border_width(ring,2,0);
  lv_obj_set_style_border_color(ring,lv_color_hex(c),0);
  label(p,"/",x+5,y-2,c);
}
static void gearIcon(lv_obj_t* p,int x,int y,uint32_t c){label(p,LV_SYMBOL_SETTINGS,x,y,c,true);}
static void chipIcon(lv_obj_t* p,int x,int y,uint32_t c){
  lv_obj_t* chip=panel(p,x,y,20,19,C_CARD);
  lv_obj_set_style_radius(chip,3,0);
  lv_obj_set_style_border_color(chip,lv_color_hex(c),0);
  lv_obj_set_style_border_width(chip,2,0);
  iconDot(chip,6,5,6,c);
}
static void sidebarLink(int y,const char* glyph,const char* txt,FrPage target,bool active){
  lv_obj_t* b=button(root,txt,4,y,99,47,active?0x2A343E:C_BG,
                       !strcmp(txt,"Motor")?openMotorSettings:nav,
                       (void*)(intptr_t)target);
  lv_obj_t* a=label(b,glyph,8,15,C_ORANGE);
  lv_obj_t* l=lv_obj_get_child(b,0);
  if(l){lv_obj_align(l,LV_ALIGN_LEFT_MID,36,0);lv_obj_set_style_text_font(l,LV_FONT_DEFAULT,0);}
  (void)a;
}
static void drawHeader(const char* title,bool back){
  panel(root,0,0,800,51,C_BG);
  if(back)button(root,LV_SYMBOL_LEFT,10,5,38,38,0x2A343E,nav,(void*)(intptr_t)FR_DASH);
  label(root,title,back?55:17,12,C_TEXT,true);
  // Actual runtime status is set by updateDisplay().
  statusLabel=label(root,"PI OFFLINE",650,15,C_RED);
  lv_obj_t* sep=panel(root,0,49,800,1,C_LINE);
  lv_obj_set_style_border_width(sep,0,0);
}
static void drawFooter(){
  if(page!=FR_DASH)return;
  // Control buttons only. Confirmation dialog remains mandatory.
  button(root,LV_SYMBOL_PLAY " Starten",115,432,214,40,C_ORANGE,clickStart);
  button(root,LV_SYMBOL_PAUSE " Pause",343,432,214,40,0x29343D,clickPause);
  button(root,LV_SYMBOL_STOP " Stop",571,432,215,40,0x75262D,clickStop);
}
static void dashCardBase(int x,int y,int w,int h,const char* title,uint32_t accent,FrPage dest){
  lv_obj_t* p=panel(root,x,y,w,h,C_CARD);
  lv_obj_add_flag(p,LV_OBJ_FLAG_CLICKABLE);
  if(dest==FR_SETTINGS && !strcmp(title,"Extruder-Foerderung"))
    lv_obj_add_event_cb(p,openMotorSettings,LV_EVENT_CLICKED,nullptr);
  else lv_obj_add_event_cb(p,nav,LV_EVENT_CLICKED,(void*)(intptr_t)dest);
  label(p,title,33,10,C_TEXT);
  label(p,LV_SYMBOL_RIGHT,w-18,10,C_MUTED);
  if(dest==FR_TEMP)thermoIcon(p,12,9,accent);
  else if(dest==FR_FILAMENT)metricIcon(p,10,9,accent);
  else if(dest==FR_SETTINGS)gearIcon(p,10,9,accent);
  else fanIcon(p,10,9,accent);
}
static void drawDashboard(){
  drawHeader("FilamentRecycler",false);
  // Narrow 108 px sidebar and five compact cards on the 800x480 screen.
  panel(root,0,51,108,429,0x101920);
  sidebarLink(64,LV_SYMBOL_HOME,"Home",FR_DASH,true);
  sidebarLink(115,LV_SYMBOL_CHARGE,"Temp",FR_TEMP,false);
  sidebarLink(166,LV_SYMBOL_SETTINGS,"Motor",FR_SETTINGS,false);
  sidebarLink(217,LV_SYMBOL_EYE_OPEN,"Filament",FR_FILAMENT,false);
  sidebarLink(268,LV_SYMBOL_SETTINGS,"Setup",FR_SETTINGS,false);
  // Row 1: shared 3-zone card, feeding card and cooling card.
  dashCardBase(115,58,272,173,"Extruder-Temperaturen",C_ORANGE,FR_TEMP);
  dashCardBase(394,58,247,173,"Extruder-Foerderung",C_ORANGE,FR_SETTINGS);

  dashCardBase(648,58,141,173,"Kuehlstrecke",C_ORANGE,FR_SETTINGS);
  // Row 2: filament + machine status.
  dashCardBase(115,238,329,187,"Filamentdurchmesser",C_ORANGE,FR_FILAMENT);
  dashCardBase(451,238,338,187,"Systemstatus",C_ORANGE,FR_SETTINGS);
  // Temperature zone readouts and three overlaid micro history charts.
  const int xx[3]={127,217,308};
  const char* zname[3]={"Einzug","Mitte","Duese"};
  for(int i=0;i<3;i++){
    label(root,zname[i],xx[i],92,C_MUTED);
    dashboardValue[i]=label(root,"--",xx[i],112,C_Z[i],true);
    dashAux[i]=label(root,"Soll: --",xx[i],142,C_MUTED);
    lv_obj_t* mini=addChart(root,126+i*88,175,78,43,i,false);
    lv_obj_add_event_cb(mini,nav,LV_EVENT_CLICKED,(void*)(intptr_t)FR_TEMP);
  }
  // Feed data: show only available measurements, never fabricate a flow rate.
  label(root,"Schnecke",408,93,C_MUTED);
  dashboardValue[3]=label(root,"-- rpm",408,113,C_ORANGE,true);
  label(root,"Foerdermenge",515,93,C_MUTED);
  feedValue[0]=label(root,"-- g/min",515,115,C_ORANGE);
  label(root,"Filamentzug",408,155,C_MUTED);
  feedValue[1]=label(root,"-- m/min",408,176,C_ORANGE);
  label(root,"Soll-RPM",522,155,C_MUTED);
  feedValue[2]=label(root,"-- rpm",522,176,C_TEXT);
  // Cooling fan percentage unavailable until hardware module is installed.
  fanIcon(root,699,96,C_ORANGE);
  coolValue=label(root,"-- %",684,138,C_ORANGE,true);
  label(root,"Luefter",689,171,C_MUTED);
  // Diameter chart and target.
  label(root,"Ist",129,275,C_MUTED);
  dashboardValue[4]=label(root,"-- mm",129,292,C_ORANGE,true);
  label(root,"Soll",276,275,C_MUTED);
  dashAux[3]=label(root,"1.75 mm",276,294,C_TEXT);
  addChart(root,129,340,298,66,3,false);
  // Status card: compact, legible system flags.
  label(root,"Mega",468,277,C_MUTED);
  dashboardValue[5]=label(root,"--",593,277,C_TEXT);
  label(root,"Raspberry Pi",468,309,C_MUTED);
  dashAux[4]=label(root,"--",593,309,C_TEXT);
  label(root,"Temperatursensoren",468,341,C_MUTED);
  dashAux[5]=label(root,"--",650,341,C_TEXT);
  label(root,"Aktoren",468,373,C_MUTED);
  label(root,"TEST / AUS",593,373,C_ORANGE);
  drawFooter();
}
static void drawTemperature(){
  drawHeader("Temperaturen",true);
  for(int i=0;i<3;i++){
    char name[30];snprintf(name,sizeof(name),"Zone %d",i+1);
    button(root,name,12,75+i*60,130,52,i==selectedZone?C_BLUE:0x2A343E,chooseZone,(void*)(intptr_t)i);
  }
  lv_obj_t* p=panel(root,155,65,631,325);
  detailLabels[0]=label(p,"Ist: --",18,14,C_TEXT,true);
  detailLabels[1]=label(p,"Soll: --",320,18,C_MUTED);
  addChart(p,18,65,590,195,selectedZone,true);
  button(p,"Graph gross",18,273,155,41,0x2A343E,graphButton,(void*)(intptr_t)FR_GRAPH_TEMP);
  button(p,"Sollwert aendern",352,273,255,41,C_BLUE,onEdit,(void*)(intptr_t)(100+selectedZone));
}
static void drawFilament(){
  drawHeader("Filamentdurchmesser",true);
  lv_obj_t* p=panel(root,14,65,772,325);
  detailLabels[0]=label(p,"Aktuell: --",20,17,C_TEXT,true);
  detailLabels[1]=label(p,"Soll: --",360,22,C_MUTED);
  addChart(p,20,75,728,195,3,true);
  button(p,"Graph gross",20,274,180,42,0x2A343E,graphButton,(void*)(intptr_t)FR_GRAPH_DIA);
  button(p,"Sollwert einstellen",476,274,270,42,C_BLUE,onEdit,(void*)(intptr_t)400);
  label(root,"Sensor noch nicht vorhanden: Istwert bleibt --",24,410,C_MUTED);
}
static void drawSettings(){
  drawHeader("Einstellungen",true);
  const char* names[]={"Temperaturen","PID-Regler","Motor","System"};
  for(int i=0;i<4;i++)button(root,names[i],12,76+i*77,170,65,i==(int)settingPage?C_BLUE:0x2A343E,
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
      button(p,name,18+i*185,54,175,45,i==selectedZone?C_BLUE:0x2A343E,chooseZone,(void*)(intptr_t)i);
    }
    const char* names2[]={"Kp - Proportional","Ki - Integral","Kd - Differential"};
    for(int i=0;i<3;i++){
      label(p,names2[i],20,128+i*65,C_MUTED);
      detailLabels[i]=label(p,"--",259,124+i*65,C_TEXT,true);
      button(p,"Aendern",423,115+i*65,141,43,C_BLUE,onEdit,(void*)(intptr_t)(200+i));
    }
  }else if(settingPage==FR_SET_MOTOR){
    label(p,"E0 - TMC2208 MOTORTEST",19,12,C_TEXT,true);
    label(p,"STEP-Frequenz (kein gemessener RPM-Wert)",19,55,C_MUTED);
    button(p,"-50",18,85,100,47,0x2A343E,changeTestSpeed,(void*)(intptr_t)-50);
    motorSpeedLabel=label(p,"200 STEP/s",159,92,C_ORANGE,true);
    button(p,"+50",450,85,120,47,0x2A343E,changeTestSpeed,(void*)(intptr_t)50);
    label(p,"Drehrichtung (nur im Stillstand)",19,145,C_MUTED);
    button(p,"Richtung wechseln",19,171,200,43,0x2A343E,toggleMotorDirection);
    motorDirLabel=label(p,"Vorwaerts",252,177,C_TEXT);
    button(p,LV_SYMBOL_PLAY " MOTOR START",19,236,244,52,C_GREEN,startMotorTest);
    button(p,LV_SYMBOL_STOP " MOTOR STOP",278,236,286,52,C_RED,stopMotorTest);
    motorTestLabel=label(p,"Teststatus: --",20,311,C_MUTED);
  }else{
    label(p,"SYSTEM",20,17,C_TEXT,true);
    detailLabels[0]=label(p,"Raspberry Pi: --",20,90,C_TEXT);
    detailLabels[1]=label(p,"Arduino Mega: --",20,136,C_TEXT);
    label(p,"Parameter: auf Raspberry Pi gespeichert",20,190,C_MUTED);
    label(p,"Aktoren in dieser Version deaktiviert",20,235,C_RED);
    button(p,"Material / Spule (Vorschau)",22,279,337,45,0x2A343E,nav,(void*)(intptr_t)FR_MATERIAL);
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
  memset(dashAux,0,sizeof(dashAux));
  memset(feedValue,0,sizeof(feedValue));coolValue=nullptr;
  memset(detailLabels,0,sizeof(detailLabels));
  memset(charts,0,sizeof(charts));
  memset(chartSeries,0,sizeof(chartSeries));
  memset(graphCharts,0,sizeof(graphCharts));
  memset(graphSeries,0,sizeof(graphSeries));
  dialog=nullptr;editVal=nullptr;statusLabel=nullptr;
  motorTestLabel=nullptr;motorSpeedLabel=nullptr;motorDirLabel=nullptr;
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
static void updateMotorTestUi(){
  char buf[56];snprintf(buf,sizeof(buf),"%d STEP/s",motorTestHz);setLabel(motorSpeedLabel,buf);
  setLabel(motorDirLabel,motorTestForward?"Vorwaerts":"Rueckwaerts");
  setLabel(motorTestLabel,motorTestRunning?"Teststatus: MOTOR LAEUFT":"Teststatus: MOTOR AUS");
}
static void updateDisplay(){
  char buf[80];
  updateMotorTestUi();
  if(statusLabel){
    const char *m=!strcmp(machineMode,"READY")?"BEREIT":!strcmp(machineMode,"PREHEAT")?"VORHEIZEN":!strcmp(machineMode,"PAUSED")?"PAUSE":machineMode;
    snprintf(buf,sizeof(buf),"%s",piOnline?m:"PI OFFLINE");
    setLabel(statusLabel,buf);
    lv_obj_set_style_text_color(statusLabel,lv_color_hex(!piOnline?C_RED:!strcmp(machineMode,"PREHEAT")?C_ORANGE:C_GREEN),0);
  }
  if(page==FR_DASH){
    for(int i=0;i<3;i++){
      if(isfinite(actual[i]))snprintf(buf,sizeof(buf),"%.1f C",actual[i]);
      else snprintf(buf,sizeof(buf),"ERR");
      setLabel(dashboardValue[i],buf);
      snprintf(buf,sizeof(buf),"Soll %.0f C",desired[i]);setLabel(dashAux[i],buf);
    }
    // Only command target, not measured RPM: until encoder feedback exists.
    setLabel(dashboardValue[3],"-- rpm");
    setLabel(feedValue[0],"-- g/min");setLabel(feedValue[1],"-- m/min");
    snprintf(buf,sizeof(buf),"%.1f",rpm);setLabel(feedValue[2],buf);
    setLabel(coolValue,"-- %");
    if(isfinite(diameter))snprintf(buf,sizeof(buf),"%.2f mm",diameter);
    else snprintf(buf,sizeof(buf),"-- mm");
    setLabel(dashboardValue[4],buf);
    snprintf(buf,sizeof(buf),"%.2f mm",diameterTarget);setLabel(dashAux[3],buf);
    setLabel(dashboardValue[5],megaOnline?"ONLINE":"OFFLINE");
    setLabel(dashAux[4],piOnline?"ONLINE":"OFFLINE");
    bool sensors=megaOnline;
    for(int i=0;i<3;i++)sensors &= isfinite(actual[i]);
    setLabel(dashAux[5],sensors?"OK":"PRUEFEN");
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
  if(!strncmp(line,"MOTOR,TEST,",11)){
    int state=0,hz=0,dir=0;
    if(sscanf(line,"MOTOR,TEST,%d,%d,%d",&state,&hz,&dir)==3){
      motorTestRunning=(state==1);
      if(motorTestRunning){motorTestHz=hz;motorTestForward=dir==1;}
      updateMotorTestUi();
    }
    return;
  }
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
  if(motorTestRunning && piOnline && (uint32_t)(millis()-motorTestBegin)<15000UL &&
     (uint32_t)(millis()-lastMotorKeep)>=350UL){
    lastMotorKeep=millis();sendLine("MTEST,KEEP");
  }
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
    motorTestRunning=false;
    for(int i=0;i<3;i++)actual[i]=NAN;
    updateDisplay();
  }
}
