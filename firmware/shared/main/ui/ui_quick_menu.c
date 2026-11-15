/* SPDX-License-Identifier: Apache-2.0
 * 中文：浮于页面之上的快捷层。S31：两行四个带文字的圆角方键与两条横向滑杆；Pico保留四个液态玻璃圆键。
 * English: A quick layer over the page. S31: two rows of four labelled rounded-square tiles and two
 * horizontal sliders; Pico keeps its four liquid-glass circular keys.
 * 用户2026-11-15修订（S31）：仿参考图的扁平白色面板，启用为黑底白图标，横屏时整块居中。
 * User revision 2026-11-15 (S31): a flat white panel after the reference image; active tiles are black
 * with a white icon, and the block is centred in landscape.
 * 冻结：不访问硬件、设置或刷新，不留整帧缓存；输入零分配，关闭释放。
 * Frozen: no hardware, settings, presentation or full-frame cache; input allocates nothing and close frees controls.
 */
#include "ui_quick_menu.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_glass.h"
#include "esp_heap_caps.h"
#if DPREADER_BOARD_METALIO47
#include <string.h>

/* ---- 布局 / Layout ---- */
static uint8_t *s_underlay,*s_frame;
static int s_rotation,s_underlay_width,s_underlay_height;
static bool s_rotate_enabled=true;
static const ui_icon_t icons[UI_QUICK_BUTTON_COUNT]={
    UI_ICON_WIFI,UI_ICON_BLUETOOTH,UI_ICON_REFRESH_CW,UI_ICON_SPARKLES,
    UI_ICON_LOCK,UI_ICON_LIGHTBULB,UI_ICON_ROTATE_CCW_SQUARE,UI_ICON_MOON};
static const char *const labels[UI_QUICK_BUTTON_COUNT]={
    "Wi-Fi","Bluetooth","Đồng bộ","Làm mới","Khoá","Đèn","Xoay","Tối"};
static EpdRect key_rect(unsigned i) {
    return (EpdRect){UI_QUICK_CENTER_X(i)-UI_QUICK_RADIUS,UI_QUICK_CENTER_Y(i)-UI_QUICK_RADIUS,UI_QUICK_TILE,UI_QUICK_TILE};
}
EpdRect ui_quick_button_rect(unsigned i) { return key_rect(i); }
void ui_quick_menu_end(void) { heap_caps_free(s_underlay);s_underlay=s_frame=NULL;s_underlay_width=s_underlay_height=0; }

/* ---- 底图 / Underlay ---- */
static void save_top(const uint8_t *fb,int width,int height) {
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        size_t at=(size_t)y*width+x;uint8_t v=ui_read_pixel(fb,x,y);
        if(at&1)s_underlay[at/2]|=(uint8_t)(v<<4);else s_underlay[at/2]=v;
    }
}
static void restore_area(uint8_t *fb,EpdRect r) {
    for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x){
        if(x<0||y<0||x>=s_underlay_width||y>=s_underlay_height)continue;
        size_t at=(size_t)y*s_underlay_width+x;
        epd_draw_pixel(x,y,(uint8_t)(((s_underlay[at/2]>>((at&1)*4))&15)<<4),fb);
    }
}
static bool underlay_valid(const uint8_t *fb) {
    return s_underlay&&s_frame==fb&&s_rotation==epd_get_rotation()&&s_underlay_width==epd_rotated_display_width();
}
bool ui_quick_menu_restore(uint8_t *fb) {
    if(!underlay_valid(fb))return false;
    restore_area(fb,(EpdRect){0,0,s_underlay_width,s_underlay_height});
    return true;
}

/* ---- 方键 / Tiles ---- */
bool ui_quick_menu_draw_button(uint8_t *fb,unsigned i,bool enabled,bool pressed,EpdRect *dirty) {
    if(!fb||!dirty||i>=UI_QUICK_BUTTON_COUNT)return false;
    EpdRect r=key_rect(i);int cx=UI_QUICK_CENTER_X(i),cy=UI_QUICK_CENTER_Y(i);
    // 面板是纯白，直接铺白再画键，无需保存每个键的底图。
    // The panel is plain white, so repaint white instead of caching each tile's underlay.
    ui_fill_rect_fast(r,UI_GRAY_WHITE,fb);
    const bool disabled=i==6&&!s_rotate_enabled;
    if(enabled&&!disabled) {
        ui_fill_round_rect(fb,r,UI_QUICK_TILE_RADIUS,UI_GRAY_BLACK);
        ui_draw_icon_normalized(fb,cx,cy,40,icons[i],UI_GRAY_WHITE);
        if(pressed)ui_draw_control_frame(fb,(EpdRect){r.x+6,r.y+6,r.width-12,r.height-12},UI_QUICK_TILE_RADIUS-6,0xA0);
    } else {
        ui_fill_round_rect(fb,r,UI_QUICK_TILE_RADIUS,pressed?0xD0:UI_GRAY_WHITE);
        ui_draw_control_frame(fb,r,UI_QUICK_TILE_RADIUS,disabled?0xB0:0x50);
        ui_draw_icon_normalized(fb,cx,cy,40,icons[i],disabled?0xA0:UI_GRAY_BLACK);
        if(pressed)ui_draw_control_frame(fb,(EpdRect){r.x+4,r.y+4,r.width-8,r.height-8},UI_QUICK_TILE_RADIUS-4,UI_GRAY_BLACK);
    }
    *dirty=r;return true;
}
static void draw_label(uint8_t *fb,unsigned i) {
    const bool disabled=i==6&&!s_rotate_enabled;
    ui_text_fixed_ink_vc(fb,UI_QUICK_CENTER_X(i),UI_QUICK_CENTER_Y(i)+UI_QUICK_RADIUS+UI_QUICK_LABEL_GAP,22,labels[i],
        EPD_DRAW_ALIGN_CENTER,disabled?0x90:UI_GRAY_BLACK);
}

/* ---- 提示与状态 / Notice and status ---- */
// 提示胶囊浮在面板下方，覆盖下层页面；只在出错/状态变化时出现。
// The notice pill floats below the panel over the page and appears only for errors/state changes.
EpdRect ui_quick_menu_draw_notice(uint8_t *fb,const char *text) {
    const EpdRect p=ui_quick_panel_rect();
    const int width=p.width<520?p.width:520;
    EpdRect box={p.x+(p.width-width)/2,p.y+p.height+12,width,38};
    // 白色外晕把胶囊与下层文字隔开。/ A white halo separates the pill from the text beneath.
    ui_fill_round_rect(fb,(EpdRect){box.x-6,box.y-6,box.width+12,box.height+12},25,UI_GRAY_WHITE);
    ui_fill_round_rect(fb,box,19,UI_GRAY_WHITE);
    ui_draw_control_frame(fb,box,19,UI_GRAY_BLACK);
    ui_text_box(fb,(EpdRect){box.x+16,box.y+4,box.width-32,30},22,text,EPD_DRAW_ALIGN_CENTER,false);
    return (EpdRect){box.x-6,box.y-6,box.width+12,box.height+12};
}
void ui_quick_menu_draw_status(uint8_t *fb) { ui_nav_redraw_status(fb); }

/* ---- 横向滑杆 / Horizontal sliders ---- */
EpdRect ui_quick_slider_rect(bool warmth) {
    const int x0=ui_quick_slider_x0(warmth);
    return (EpdRect){x0-UI_QUICK_SLIDER_KNOB-4,ui_quick_slider_y(warmth)-UI_QUICK_SLIDER_KNOB-4,
        UI_QUICK_SLIDER_LENGTH+2*UI_QUICK_SLIDER_KNOB+8,2*UI_QUICK_SLIDER_KNOB+8};
}
EpdRect ui_quick_slider_change_rect(bool warmth,unsigned before,unsigned after) {
    if(before>UI_QUICK_SLIDER_RANGE)before=UI_QUICK_SLIDER_RANGE;
    if(after>UI_QUICK_SLIDER_RANGE)after=UI_QUICK_SLIDER_RANGE;
    if(before==after)return (EpdRect){0};
    const EpdRect area=ui_quick_slider_rect(warmth);
    const int x0=ui_quick_slider_x0(warmth);
    int a=x0+2*(int)before,b=x0+2*(int)after;
    int left=(a<b?a:b)-UI_QUICK_SLIDER_KNOB-4,right=(a>b?a:b)+UI_QUICK_SLIDER_KNOB+4;
    if(left<area.x)left=area.x;
    if(right>area.x+area.width)right=area.x+area.width;
    return (EpdRect){left,area.y,right-left,area.height};
}
void ui_quick_menu_draw_slider(uint8_t *fb,bool warmth,unsigned value) {
    if(value>UI_QUICK_SLIDER_RANGE)value=UI_QUICK_SLIDER_RANGE;
    const int x0=ui_quick_slider_x0(warmth),cy=ui_quick_slider_y(warmth);
    const int knob=x0+2*(int)value;
    ui_fill_rect_fast(ui_quick_slider_rect(warmth),UI_GRAY_WHITE,fb);
    // 细轨：已选部分深色，其余浅灰；圆形滑块白底深边。
    // Thin track: dark up to the knob, light grey beyond; a white knob with a dark ring.
    ui_fill_round_rect(fb,(EpdRect){x0,cy-3,UI_QUICK_SLIDER_LENGTH,6},3,0xC0);
    if(knob>x0)ui_fill_round_rect(fb,(EpdRect){x0,cy-3,knob-x0,6},3,UI_QUICK_FILL_DARK);
    epd_fill_circle(knob,cy,UI_QUICK_SLIDER_KNOB,UI_QUICK_FILL_DARK,fb);
    epd_fill_circle(knob,cy,UI_QUICK_SLIDER_KNOB-3,UI_GRAY_WHITE,fb);
}
static void draw_slider_icon(uint8_t *fb,bool warmth) {
    ui_draw_icon_normalized(fb,ui_quick_slider_icon_x(warmth),ui_quick_slider_y(warmth),34,
        warmth?UI_ICON_THERMOMETER:UI_ICON_SUN,UI_GRAY_BLACK);
}

/* ---- 合成 / Composition ---- */
void ui_quick_menu_draw(uint8_t *fb,unsigned active,bool rotate_enabled,unsigned brightness,unsigned warmth) {
    ui_quick_menu_end();
    const int width=epd_rotated_display_width(),height=UI_QUICK_HEIGHT;
    if(!fb||width<684||epd_rotated_display_height()<height)return;
    s_rotate_enabled=rotate_enabled;
    s_underlay=heap_caps_malloc((size_t)width*height/2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    s_frame=fb;s_rotation=epd_get_rotation();s_underlay_width=width;s_underlay_height=height;
    if(s_underlay)save_top(fb,width,height);
    const EpdRect panel=ui_quick_panel_rect();
    ui_fill_round_rect(fb,panel,32,UI_GRAY_WHITE);
    ui_draw_control_frame(fb,panel,32,0x50);
    for(unsigned i=0;i<UI_QUICK_BUTTON_COUNT;++i){
        EpdRect dirty;ui_quick_menu_draw_button(fb,i,(active>>i)&1u,false,&dirty);draw_label(fb,i);
    }
    draw_slider_icon(fb,false);draw_slider_icon(fb,true);
    ui_quick_menu_draw_slider(fb,false,ui_quick_brightness_position(brightness));
    ui_quick_menu_draw_slider(fb,true,warmth);
    // 把手在面板底边内侧。/ The grabber sits inside the panel's bottom edge.
    ui_fill_round_rect(fb,(EpdRect){panel.x+panel.width/2-26,panel.y+panel.height-16,52,6},3,0x90);
    ui_quick_menu_draw_status(fb);
}
#else

static int center_x(int button) { return (button * 2 + 1) * UI_LOCK_WIDTH / 8; }
static EpdRect glass_rect(void) { return (EpdRect){18,88,648,168}; }
enum { KEY_SIDE=2*UI_QUICK_RADIUS, KEY_BYTES=KEY_SIDE*KEY_SIDE/2, TOP_BYTES=UI_LOCK_WIDTH*UI_QUICK_HEIGHT/2 };
static uint8_t *s_underlay, *s_key_pixels, *s_key_frame;
static int s_key_rotation;
static const ui_icon_t s_icons[]={UI_ICON_WIFI,UI_ICON_BLUETOOTH,UI_ICON_REFRESH_CW,UI_ICON_LOCK};
static EpdRect key_rect(int button) {
    return (EpdRect){center_x(button)-UI_QUICK_RADIUS,UI_QUICK_CIRCLE_Y-UI_QUICK_RADIUS,KEY_SIDE,KEY_SIDE};
}
void ui_quick_menu_end(void) {
    heap_caps_free(s_underlay);heap_caps_free(s_key_pixels);s_underlay=s_key_pixels=s_key_frame=NULL;
}
static void draw_solid_key(uint8_t *fb,int button) {
    int cx=center_x(button);
    epd_fill_circle(cx,UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-1,UI_GRAY_WHITE,fb);
    epd_draw_circle(cx,UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-1,0x60,fb);
    epd_draw_circle(cx,UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-2,0x60,fb);
}
static void draw_badge(uint8_t *fb,int button,bool active) {
    if(button>1||!active)return;
    int cx=center_x(button);
    epd_fill_circle(cx+30,UI_QUICK_CIRCLE_Y-30,5,UI_GRAY_WHITE,fb);
    epd_draw_circle(cx+30,UI_QUICK_CIRCLE_Y-30,5,0x50,fb);
}
bool ui_quick_menu_draw_button(uint8_t *fb,int button,bool active,bool pressed,EpdRect *dirty) {
    if(!fb||!dirty||button<0||button>=4||epd_rotated_display_width()<UI_LOCK_WIDTH||epd_rotated_display_height()<UI_QUICK_HEIGHT)return false;
    EpdRect r=key_rect(button);
    if(s_key_pixels&&s_key_frame==fb&&s_key_rotation==epd_get_rotation()) {
        const uint8_t *pixels=s_key_pixels+button*KEY_BYTES;
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
            size_t at=(size_t)y*r.width+x;
            epd_draw_pixel(r.x+x,r.y+y,(uint8_t)(((pixels[at/2]>>((at&1)*4))&15)<<4),fb);
        }
    } else {
        // 内存不足时圆内实心回退，不重算玻璃，也不在圆外刷白方框。
        // Low memory uses a solid circular fallback, never reblurring or whitening a square around it.
        draw_solid_key(fb,button);
        ui_draw_icon(fb,center_x(button),UI_QUICK_CIRCLE_Y,44,s_icons[button],UI_GRAY_BLACK);
    }
    draw_badge(fb,button,active);
    if(pressed){
        epd_draw_circle(center_x(button),UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-7,UI_GRAY_BLACK,fb);
        epd_draw_circle(center_x(button),UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-8,UI_GRAY_BLACK,fb);
    }
    *dirty=r;return true;
}
void ui_quick_menu_draw_status(uint8_t *fb) {
    ui_fill_rect_fast((EpdRect){0,0,UI_LOCK_WIDTH,76},UI_GRAY_WHITE,fb);
    ui_nav_status(fb);
}

// 只缓存顶部，展开直接取现有画面，收起精确还原；圆键也从清晰底图折射。
// Cache only the top: open from the existing frame, restore it exactly on close, and refract keys from sharp art.
static uint8_t *save_underlay(const uint8_t *fb) {
    uint8_t *saved=heap_caps_malloc(TOP_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!saved)return NULL;
    for(int y=0;y<UI_QUICK_HEIGHT;++y)for(int x=0;x<UI_LOCK_WIDTH;++x){
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;
        uint8_t v=ui_read_pixel(fb,x,y);
        if(at&1)saved[at/2]|=(uint8_t)(v<<4);else saved[at/2]=v;
    }
    return saved;
}
bool ui_quick_menu_restore(uint8_t *fb) {
    if(!s_underlay||s_key_frame!=fb||s_key_rotation!=epd_get_rotation())return false;
    for(int y=0;y<UI_QUICK_HEIGHT;++y)for(int x=0;x<UI_LOCK_WIDTH;++x){
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;
        epd_draw_pixel(x,y,(uint8_t)(((s_underlay[at/2]>>((at&1)*4))&15)<<4),fb);
    }
    return true;
}
static void restore_circle(uint8_t *fb,const uint8_t *saved,int cx) {
    const int cy=UI_QUICK_CIRCLE_Y,r=UI_QUICK_RADIUS;
    for(int y=cy-r;y<cy+r;++y)for(int x=cx-r;x<cx+r;++x){
        // 与玻璃圆角的半像素距离一致，圆外仍保留面板，不留下方形补丁。
        // Match the glass circle's half-pixel boundary, retaining the outer panel without square patches.
        int dx=2*(x-cx)+1,dy=2*(y-cy)+1;
        if(dx*dx+dy*dy>4*r*r)continue;
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;
        epd_draw_pixel(x,y,(uint8_t)(((saved[at/2]>>((at&1)*4))&15)<<4),fb);
    }
}
void ui_quick_menu_draw(uint8_t *fb,bool wifi,bool bluetooth,bool soften_text) {
    ui_quick_menu_end();
    if(!fb||epd_rotated_display_width()<UI_LOCK_WIDTH||epd_rotated_display_height()<UI_QUICK_HEIGHT)return;
    EpdRect panel=glass_rect();
    uint8_t *underlay=s_underlay=save_underlay(fb);
    s_key_frame=fb;s_key_rotation=epd_get_rotation();
    ui_fill_round_rect(fb,panel,32,UI_GRAY_WHITE);
    ui_draw_control_frame(fb,panel,32,0x50);
    for(int i=0;i<4;++i){
        int cx=center_x(i);
        draw_solid_key(fb,i);
        ui_draw_icon(fb,cx,UI_QUICK_CIRCLE_Y,44,s_icons[i],UI_GRAY_BLACK);
    }
    // 玻璃工作区已释放，再保存四圆键，峰值不叠加；不包含可变化的状态点和按压环。
    // Release glass workspaces before saving four keys, excluding mutable badges and press rings from the baseline.
    s_key_pixels=heap_caps_malloc(UI_QUICK_BUTTON_COUNT*KEY_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(s_key_pixels){
        s_key_frame=fb;s_key_rotation=epd_get_rotation();
        for(int i=0;i<4;++i){
            EpdRect r=key_rect(i);uint8_t *pixels=s_key_pixels+i*KEY_BYTES;
            for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
                size_t at=(size_t)y*r.width+x;uint8_t v=ui_read_pixel(fb,r.x+x,r.y+y);
                if(at&1)pixels[at/2]|=(uint8_t)(v<<4);else pixels[at/2]=v;
            }
        }
    }
    draw_badge(fb,0,wifi);draw_badge(fb,1,bluetooth);
    ui_fill_round_rect(fb,(EpdRect){UI_LOCK_WIDTH/2-26,240,52,6},3,0x70);
    // 状态栏保持清晰；其余底图直接透出，不给整个页面加模糊或白色蒙层。
    // Keep status crisp and the rest of the page exposed, without a full-page blur or white veil.
    ui_quick_menu_draw_status(fb);
}
#endif
