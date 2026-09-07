/* Web studies 13–16: vector LVGL renderer, 300×400 design units -> 240×320.
 * Shapes and the 5×7 font follow design/web-poc/app/explore. No framebuffer assets.
 * One non-scrollable touch surface avoids child widgets swallowing swipes. */
#include "ui_dot.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(rw_dot_font_7);
LV_FONT_DECLARE(rw_dot_font_8);
LV_FONT_DECLARE(rw_dot_font_9);
LV_FONT_DECLARE(rw_dot_font_10);
LV_FONT_DECLARE(rw_dot_font_18);

static lv_obj_t *screen;
static ui_model_t model;
static unsigned design = 13;
static bool touching;
static lv_point_t touch_start;
static uint32_t touch_tick;
static unsigned wave_phase;
static const uint32_t BG=0x060805, FG=0xdedfd3, DIM=0x909987, RED=0xff513d;

/* All coordinates below use the Web card's design units. */
static int px(float v) { return (int)lroundf(v * .8f); }
static void box(lv_layer_t *l,float x,float y,float w,float h,uint32_t color,int radius,bool outline)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(color); d.bg_opa=outline?0:255;
    d.border_color=lv_color_hex(color); d.border_width=outline?1:0;
    d.radius=px((float)radius);
    lv_area_t a={px(x),px(y),px(x+w)-1,px(y+h)-1};
    lv_draw_rect(l,&d,&a);
}
static void dot(lv_layer_t *l,float x,float y,float r,uint32_t c)
{
    int size=(int)lroundf(r*1.6f); if(size<1) size=1;
    int cx=px(x),cy=px(y);
    lv_area_t a={cx-size/2,cy-size/2,cx-size/2+size-1,cy-size/2+size-1};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(c); d.radius=LV_RADIUS_CIRCLE;
    lv_draw_rect(l,&d,&a);
}
static void line(lv_layer_t *l,float x,float y,float ex,float ey,uint32_t c)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){(float)px(x),(float)px(y)};
    d.p2=(lv_point_precise_t){(float)px(ex),(float)px(ey)};
    d.color=lv_color_hex(c); d.width=1; lv_draw_line(l,&d);
}
static void text(lv_layer_t *l,float x,float y,float w,int size,uint32_t c,const char *s,bool right)
{
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d);
    d.font=size==7?&rw_dot_font_7:size==9?&rw_dot_font_9:size==10?&rw_dot_font_10:
           size==18?&rw_dot_font_18:&rw_dot_font_8;
    d.color=lv_color_hex(c); d.text=s; d.text_local=1;
    d.align=right?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT;
    d.flag=LV_TEXT_FLAG_EXPAND;
    lv_area_t a={px(x),px(y),px(x+w)-1,px(y+30)-1};
    lv_draw_label(l,&d,&a);
}
static void rule(lv_layer_t *l,float y)
{ for(int x=26;x<274;x+=3) dot(l,(float)x,y,.5f,0x464d3c); }
static void ring(lv_layer_t *l,float x,float y,float r,int count,float dr,uint32_t c)
{
    for(int i=0;i<count;i++) {
        float a=(float)i*6.2831853f/(float)count;
        dot(l,x+cosf(a)*r,y+sinf(a)*r,dr,c);
    }
}
static const char *const letters="0123456789AKIMENRWXYOU- ";
static const uint8_t glyph[][7]={
    {14,17,17,17,17,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},{14,17,17,31,17,17,17},{17,18,20,24,20,18,17},
    {31,4,4,4,4,4,31},{17,27,21,21,17,17,17},{31,16,16,30,16,16,31},
    {17,25,25,21,19,19,17},{30,17,17,30,20,18,17},{17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{14,17,17,17,17,17,14},
    {17,17,17,17,17,17,14},{0,0,0,31,0,0,0},{0,0,0,0,0,0,0}
};
static void digits(lv_layer_t *l,float x,float y,float w,float h,const char *value)
{
    if(strcmp(value,"—")==0)value="-";
    size_t n=strlen(value); if(n>7) n=7;
    bool numeric=strspn(value,"0123456789-")==strlen(value);
    size_t cells=numeric&&n<3?3:n; if(cells==0) return;
    float vw=(float)cells*36-6, scale=fminf(w/vw,h/42);
    x+=(w-vw*scale)/2; y+=(h-42*scale)/2;
    for(size_t i=0;i<n;i++) {
        const char *g=strchr(letters,value[i]); size_t k=g?(size_t)(g-letters):22;
        for(int row=0;row<7;row++) for(int col=0;col<5;col++)
            if(glyph[k][row]&(1<<(4-col)))
                dot(l,x+((float)(cells-n+i)*36+(float)col*6+3)*scale,
                    y+((float)row*6+3)*scale,1.9f*scale,FG);
    }
}
static bool fresh(int i)
{ return model.link_ok && i>=0 && i<model.n_peers && model.peers[i].age==POS_PEER_FRESH; }
static const char *name(int i)
{ return model.link_ok&&i<model.n_peers?model.peers[i].callsign:"—"; }
static const char *direction(int i)
{ return !fresh(i)?"位置未更新":model.peers[i].along_m<0?"後方":"前方"; }
static void distance(int i,char *s,size_t cap)
{
    if(!fresh(i)||!isfinite(model.peers[i].dist_m)) snprintf(s,cap,"-");
    else snprintf(s,cap,"%.0f",(double)fminf(99999,fmaxf(0,model.peers[i].dist_m)));
}
static const char *voice(void)
{
    if(!model.link_ok) return "未接続";
    return model.voice==UI_VOICE_RX?"受信中":model.voice==UI_VOICE_TX?"送信中":
           model.voice==UI_VOICE_BUSY?"空き待ち":"待受";
}
static bool muted(void) { return model.n_peers>0 && model.peers[0].muted; }
static const char *connection(void)
{ return !model.link_ok?"未接続":muted()?"音声ミュート":"音声接続中"; }
static void audio_icon(lv_layer_t *l,float x,float y,uint32_t c)
{
    const int h[]={4,9,14,7,11,3};
    for(int i=0;i<6;i++) line(l,x+(float)i*2.5f,y+(14-h[i])/2.0f,x+(float)i*2.5f,y+(14+h[i])/2.0f,c);
}
static void arrow(lv_layer_t *l,float x,float y,float size,int peer)
{
    if(!fresh(peer)) return;
    float sign=model.peers[peer].along_m<0?1.f:-1.f;
    float ey=y+(sign<0?0:size),sy=y+(sign<0?size:0);
    line(l,x,sy,x+size,ey,RED); line(l,x+size,ey,x+size,ey-sign*size*.7f,RED);
    line(l,x+size,ey,x+size*.3f,ey,RED);
}
static void top(lv_layer_t *l)
{
    const char *titles[]={"RW—13 / HAKONE","RW—14 / LIVE AUDIO","RW—15 / RELATIVE POSITION","RW—16 / MICRO INTERFACE"};
    text(l,26,29,205,8,DIM,titles[design-13],false);
    if(design==15) {
        float far=250;
        for(int i=0;i<model.n_peers;i++) if(fresh(i)&&isfinite(model.peers[i].dist_m)) far=fmaxf(far,model.peers[i].dist_m);
        char s[24]; snprintf(s,sizeof(s),"%.0fm",(double)(ceilf(far/250)*250));
        text(l,230,29,44,8,DIM,s,true);
    } else {
        const char *s=!model.link_ok?"OFF":model.voice==UI_VOICE_RX?"RX":model.voice==UI_VOICE_TX?"TX":model.voice==UI_VOICE_BUSY?"BUSY":"IDLE";
        dot(l,model.voice==UI_VOICE_IDLE||model.voice==UI_VOICE_BUSY?236.f:252.f,33.5f,3,model.link_ok?RED:DIM);
        text(l,242,29,32,8,DIM,s,true);
    }
}
static void distance_page(lv_layer_t *l)
{
    char s[64],n[16]; distance(0,n,sizeof(n));
    snprintf(s,sizeof(s),"%s / %s",name(0),direction(0)); text(l,26,70,210,10,0xb8beaa,s,false);
    arrow(l,255,70,13,0); digits(l,26,104,231,95,n);
    text(l,26,216,130,8,DIM,direction(0),false); text(l,251,210,23,18,0xc9cfba,"m",true);
    rule(l,255);
    for(int i=1;i<=2;i++) {
        float x=i==1?26.f:162.f;
        text(l,x,283,30,8,DIM,name(i),false); distance(i,n,sizeof(n));
        digits(l,x+30,274,66,27,n);
        snprintf(s,sizeof(s),"m %s",direction(i)); text(l,x+30,311,82,8,DIM,s,true);
    }
    audio_icon(l,26,346,0xb2baa4);
    if(!model.link_ok) snprintf(s,sizeof(s),"再接続してください");
    else if(model.voice==UI_VOICE_RX) snprintf(s,sizeof(s),"%sから受信中",model.talker);
    else snprintf(s,sizeof(s),"%s",voice());
    text(l,48,348,143,8,0xb2baa4,s,false);
    snprintf(s,sizeof(s),"%d人 · %s",model.link_ok?model.n_peers+1:0,connection());
    text(l,178,349,96,7,DIM,s,true);
}
static void voice_page(lv_layer_t *l)
{
    char s[64],n[16]; distance(0,n,sizeof(n));
    digits(l,26,74,136,56,!model.link_ok?"-":model.voice==UI_VOICE_TX?"YOU":name(0));
    text(l,173,88,101,9,0xafb8a1,muted()&&model.voice==UI_VOICE_RX?"受信中・消音":voice(),true);
    if(model.voice==UI_VOICE_TX&&model.link_ok)snprintf(s,sizeof(s),"全員へ");
    else snprintf(s,sizeof(s),"%s %sm",direction(0),n);
    text(l,163,108,111,8,0xff6e58,s,true);
    /* Same SVG viewBox 330×90, centered in the 248×95 CSS box. */
    const int heights[]={1,2,2,3,1,3,5,4,6,3,4,2,1};
    bool active=model.link_ok&&(model.voice==UI_VOICE_TX||(model.voice==UI_VOICE_RX&&!muted()));
    float scale=248.f/330;
    for(int x=0;x<47;x++) for(int y=0;y<13;y++) {
        bool lit=active&&abs(y-6)<heights[(x+(int)wave_phase)%13];
        uint32_t c=lit?(x>16&&x<29?RED:0xd7d8cc):0x232521;
        dot(l,26+(4+(float)x*7)*scale,163.7f+(3+(float)y*7)*scale,1.6f*scale,c);
    }
    text(l,26,255,100,7,0x939e83,"受信音声",false); text(l,181,255,93,7,0x939e83,"VOICE / MONO",true);
    rule(l,285);
    const float xs[]={26,103,160,217};
    for(int i=0;i<4;i++) {
        uint32_t c=i==0&&model.link_ok?0xff6e58:0xb2baa4;
        ring(l,xs[i]+11,315,10.5f,23,.55f,c);
        char initial[2]={i<3&&model.link_ok&&i<model.n_peers?name(i)[0]:'-',0};
        text(l,xs[i]+7,310,14,8,c,i==3?"自":initial,false);
        text(l,xs[i]+27,311,38,8,c,i==3?"あなた":name(i),false);
        if(i==0&&model.voice==UI_VOICE_RX) audio_icon(l,76,308,c);
    }
    const char *hint=!model.link_ok?"再接続してください":model.voice==UI_VOICE_TX?"PTTを離すと終了":model.voice==UI_VOICE_RX?"受信しながら話せます":model.voice==UI_VOICE_BUSY?"空きを待っています":"物理PTTで話す";
    text(l,26,347,248,8,0x909a82,hint,false);
}
static void radar_page(lv_layer_t *l)
{
    const float sc=248.f/260,ox=26,oy=49+(251-220*sc)/2;
    float cx=ox+130*sc,cy=oy+110*sc;
    for(int j=0;j<3;j++) ring(l,cx,cy,(float)((int[]){38,69,99})[j]*sc,30+j*24,1.2f*sc,0x61655c);
    for(int t=-99;t<=99;t+=6) {dot(l,cx+(float)t*sc,cy,.5f,0x40453c);dot(l,cx,cy+(float)t*sc,.5f,0x40453c);}
    float range=250;
    for(int i=0;i<model.n_peers;i++) if(fresh(i)&&isfinite(model.peers[i].dist_m)) range=fmaxf(range,model.peers[i].dist_m);
    range=ceilf(range/250)*250;
    for(int i=0;i<model.n_peers;i++) if(fresh(i)&&isfinite(model.peers[i].dist_m)) {
        float x=(i==0?155.f:i==1?96.f:i==2?151.f:70+(float)i*15);
        float y=110+(model.peers[i].along_m<0?1.f:-1.f)*84*model.peers[i].dist_m/range;
        uint32_t c=i==0?RED:FG; float dx=ox+x*sc,dy=oy+y*sc;
        dot(l,dx,dy,3*sc,c); if(i==0) ring(l,dx,dy,12*sc,13,1,c);
        text(l,dx+(i==1?-35:i==0?21:10),dy-8,40,8,FG,name(i),false);
        if(i==0) {char d[16],s[20];distance(i,d,sizeof(d));snprintf(s,sizeof(s),"%sm",d);text(l,dx+20,dy+8,50,8,RED,s,false);}
    }
    lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.color=lv_color_hex(FG);
    d.p[0]=(lv_point_precise_t){(float)px(cx),(float)px(cy-10*sc)};
    d.p[1]=(lv_point_precise_t){(float)px(cx+8*sc),(float)px(cy+9*sc)};
    d.p[2]=(lv_point_precise_t){(float)px(cx-8*sc),(float)px(cy+9*sc)};lv_draw_triangle(l,&d);
    d.color=lv_color_hex(BG);d.p[0].y=(float)px(cy+5*sc);lv_draw_triangle(l,&d);
    rule(l,310); dot(l,29,342,3,model.link_ok?RED:DIM);
    text(l,40,336,40,10,FG,name(0),false);text(l,76,337,68,8,DIM,voice(),false);
    char n[16],s[40];distance(0,n,sizeof(n));digits(l,166,329,66,27,n);
    snprintf(s,sizeof(s),"m %s",direction(0));
    text(l,!fresh(0)?175.f:235.f,!fresh(0)?367.f:338.f,!fresh(0)?99.f:39.f,8,DIM,s,true);
}
static void matrix_page(lv_layer_t *l)
{
    const char *titles[]={"01 / VOICE","02 / DISTANCE","03 / GROUP","04 / MEMBERS"};
    for(int i=0;i<4;i++) {
        float x=i%2?154.f:26.f,y=i/2?183.f:62.f;
        box(l,x,y,120,112,0x343c2b,9,true);text(l,x+14,y+17,93,7,0x909b80,titles[i],false);
    }
    digits(l,40,105,92,32,!model.link_ok?"-":model.voice==UI_VOICE_TX?"YOU":name(0)); dot(l,43,153,3,model.link_ok?RED:DIM);
    text(l,53,149,82,8,0xff6e58,voice(),false);
    char n[16],s[40];distance(0,n,sizeof(n));digits(l,168,105,92,32,n);
    snprintf(s,sizeof(s),"m %s",direction(0));text(l,168,149,92,8,0xaab59a,s,false);arrow(l,248,150,7,0);
    snprintf(n,sizeof(n),"%d",model.link_ok?model.n_peers+1:0);digits(l,40,225,92,32,n);
    text(l,40,269,92,8,0xaab59a,"人が接続中",false);
    for(int i=1;i<=2;i++) {
        float y=i==1?231.f:265.f; text(l,168,y,34,9,0xaab59a,name(i),false);
        distance(i,n,sizeof(n));snprintf(s,sizeof(s),"%sm",n);text(l,208,y,52,9,0xd2d8c5,s,true);
    }
    text(l,26,317,30,7,0x929e81,"LINK",false);
    int level=model.link_ok?(int)lroundf(fmaxf(0,fminf(32,((float)model.link_rssi_dbm+100)*.72f))):0;
    for(int i=0;i<32;i++) dot(l,61.5f+(float)i*6,320.75f,1.5f,i<level?RED:0x303728);
    for(int i=0;i<4;i++) line(l,263+(float)i*3,326,263+(float)i*3,324-(float)i*3,DIM);
    ring(l,33,352,5,12,.5f,0xb2baa4);dot(l,33,352,1,0xb2baa4);
    text(l,48,347,145,8,0xb2baa4,"箱根ツーリング",false);text(l,183,348,91,7,DIM,connection(),true);
}
static void draw(lv_event_t *e)
{
    lv_layer_t *l=lv_event_get_layer(e);
    box(l,0,0,300,400,BG,0,false);box(l,0,0,300,400,0x343b2f,18,true);
    top(l);
    if(design==13) distance_page(l); else if(design==14) voice_page(l);
    else if(design==15) radar_page(l);else matrix_page(l);
}
void ui_dot_cancel_touch(void) { touching=false; }
static void touch(lv_event_t *e)
{
    lv_event_code_t code=lv_event_get_code(e);
    if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESS_LOST&&code!=LV_EVENT_RELEASED)return;
    lv_indev_t *indev=lv_event_get_indev(e);if(!indev)return;
    if(code==LV_EVENT_PRESSED) {lv_indev_get_point(indev,&touch_start);touch_tick=lv_tick_get();touching=true;}
    else if(code==LV_EVENT_PRESS_LOST) touching=false;
    else if(code==LV_EVENT_RELEASED && touching) {
        touching=false;lv_point_t p;lv_indev_get_point(indev,&p);
        int dx=p.x-touch_start.x,dy=p.y-touch_start.y;
        if(p.x<0||p.x>=240||p.y<0||p.y>=320||lv_tick_elaps(touch_tick)>1000)return;
        if(abs(dx)>=40&&abs(dx)*2>=abs(dy)*3)
            ui_show((ui_screen_t)(13+(design-13+(dx<0?1:3))%4));
    }
}
bool ui_dot_is_screen(ui_screen_t s) { return s>=UI_SCREEN_DOT13&&s<=UI_SCREEN_DOT16; }
lv_obj_t *ui_dot_create(void)
{
    screen=lv_obj_create(NULL);lv_obj_remove_style_all(screen);
    /* Tell LVGL that the custom-drawn root is opaque. Otherwise partial
     * refreshes can redraw an underlying screen through unchanged regions. */
    lv_obj_set_style_bg_color(screen,lv_color_hex(BG),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_set_size(screen,240,320);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen,draw,LV_EVENT_DRAW_MAIN,NULL);
    lv_obj_add_event_cb(screen,touch,LV_EVENT_ALL,NULL);
    memset(&model,0,sizeof(model));touching=false;wave_phase=0;
    return screen;
}
void ui_dot_show(unsigned id) {design=id;wave_phase=0;touching=false;lv_obj_invalidate(screen);}
void ui_dot_update(const ui_model_t *m)
{
    uint32_t old_time=model.animation_ms;
    model.animation_ms=m->animation_ms;
    bool content_changed=memcmp(&model,m,sizeof(model))!=0;
    model=*m;
    if(!ui_dot_is_screen(ui_current()))return;
    if(content_changed)lv_obj_invalidate(screen);
    unsigned next_phase=(m->animation_ms/160)%13;
    bool active=model.link_ok&&(model.voice==UI_VOICE_TX||(model.voice==UI_VOICE_RX&&!muted()));
    if(ui_current()==UI_SCREEN_DOT14 && active && old_time!=m->animation_ms && wave_phase!=next_phase) {
        wave_phase=next_phase;lv_area_t a={20,130,220,186};lv_obj_invalidate_area(screen,&a);
    }
}
