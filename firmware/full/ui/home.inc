/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：从原应用文件迁移，保留原许可。/ English: Moved from the original app under its existing license. */
/* 中文：Full首页界面。/ English: Full Home interface. */
/* User reference2026-11-13: flat gray resume card with live progress. User 2026-11-15: a monthly reading-calendar card replaces the shortcut card and recent books. */
static void render_root(uint8_t *fb) {
    if(app_settings_simple_ui()){render_simple_root(fb);return;}
#ifdef DPREADER_LOCAL_LATENCY
    int64_t probe_home=esp_timer_get_time();
#define HOME_STAGE(name) do { int64_t probe_now=esp_timer_get_time();ESP_LOGI("latency","HOME stage=%s us=%lld",name,(long long)(probe_now-probe_home));probe_home=probe_now; } while(0)
#else
#define HOME_STAGE(name) do {} while(0)
#endif
    ui_nav_status(fb);
    struct tm date;
    if(date_now(&date)) {
        static const char *const weekdays[]={"Chủ nhật","Thứ Hai","Thứ Ba","Thứ Tư","Thứ Năm","Thứ Sáu","Thứ Bảy"};
        char heading[64];snprintf(heading,sizeof(heading),"%s, %d tháng %d",weekdays[date.tm_wday],date.tm_mday,date.tm_mon+1);
        ui_text_box(fb,(EpdRect){36,91,430,62},30,heading,EPD_DRAW_ALIGN_LEFT,false);
    } else ui_text(fb,36,91,52,"Trang chủ",EPD_DRAW_ALIGN_LEFT,false);
    char value[80];unsigned seconds=today_seconds();
    snprintf(value,sizeof(value),"%u phút",seconds/60);fit(value,24,180);label(fb,490,118,24,value,EPD_DRAW_ALIGN_RIGHT,0x30);
    ring(fb,540,118,30,7,seconds,app_settings_home_goal_minutes()*60);
    if(s_avatar_ready)ui_wallpaper_draw_avatar(fb,s_avatar,(EpdRect){588,88,60,60},30);
    else {epd_fill_circle(618,118,30,0x30,fb);label(fb,618,118,30,"K",EPD_DRAW_ALIGN_CENTER,0xff);}
    HOME_STAGE("header");
#if DPREADER_BOARD_METALIO47
    const uint8_t card_gray=app_settings_main_fast_refresh()?0xff:ui_soft_card_color(false);
#else
    const uint8_t card_gray=app_settings_main_fast_refresh()?0xff:0xee;
#endif
    ui_fill_round_rect(fb,(EpdRect){36,184,612,378},24,card_gray);
    home_bold_label(fb,60,222,26,"Đọc tiếp",EPD_DRAW_ALIGN_LEFT,card_gray);
    if(s_excerpt[0]) {
        char quote[BOOK_EXCERPT_CAP+12];snprintf(quote,sizeof(quote),"“%s”",s_excerpt);
        int px=22;while(px>16&&ui_text_fixed_width_px(px,quote)>426)--px;
        if(ui_text_fixed_width_px(px,quote)<=426)label_on(fb,622,222,px,quote,EPD_DRAW_ALIGN_RIGHT,0x60,card_gray);
    }
    HOME_STAGE("card_top");
    home_book_t *b=featured();
    if(b->path[0]) {
        // 按住整张卡片时封面轻抬4px；原边框不动，不追加黑框或整页刷新。
        // Lift the cover4px while holding any part of the card; keep its frame still without extra boxes/full refreshes.
        draw_cover(fb,b,(EpdRect){60,continue_card_pressed()?274:278,176,240});
        int rows=title_lines(fb,b->title,card_gray);
        if(b->author[0]) {char author[128];snprintf(author,sizeof(author),"%s",b->author);fit(author,25,358);label_on(fb,264,rows==2?406:367,25,author,EPD_DRAW_ALIGN_LEFT,0x50,card_gray);}
        snprintf(value,sizeof(value),"Đã đọc %u%%",b->has_progress?b->progress.pct:0);
        label_on(fb,264,482,30,value,EPD_DRAW_ALIGN_LEFT,0x20,card_gray);
        home_bold_label(fb,622,482,28,"Đọc tiếp ›",EPD_DRAW_ALIGN_RIGHT,card_gray);
        EpdRect track={264,514,358,4};
        if(app_settings_main_fast_refresh()) {
            for(int x=track.x;x<track.x+track.width;x+=3)
                ui_fill_rect_fast((EpdRect){x,track.y,1,track.height},UI_GRAY_BLACK,fb);
        } else ui_fill_round_rect(fb,track,2,UI_GRAY_LIGHT);
        unsigned pct=b->has_progress?b->progress.pct:0;if(pct>100)pct=100;
        int filled=track.width*(int)pct/100;
        if(filled)ui_fill_round_rect(fb,(EpdRect){track.x,511,filled,10},5,UI_GRAY_BLACK);
    } else label_on(fb,342,410,23,"Sau khi mở sách, bạn có thể đọc tiếp ngay tại đây",EPD_DRAW_ALIGN_CENTER,0x60,card_gray);
    HOME_STAGE("card_body");
    // 用户2026-11-15：工具卡与最近阅读由月阅读日历取代；只画已缓存的记录，不读卡。
    // User 2026-11-15: the monthly reading calendar replaces utilities and recent books; paint cached records only.
    const EpdRect month=home_calendar_rect();
    ui_fill_round_rect(fb,month,24,card_gray);
    if(!s_reading_days_valid) label_on(fb,342,month.y+month.height/2,23,"Vui lòng cài đặt ngày giờ trước",EPD_DRAW_ALIGN_CENTER,0x60,card_gray);
    else {
        static const char *const days[]={"T2","T3","T4","T5","T6","T7","CN"};
        const unsigned goal=app_settings_home_goal_minutes()*60,today=(unsigned)s_date.tm_mday;
        snprintf(value,sizeof(value),"Tháng %d",s_date.tm_mon+1);home_bold_label(fb,60,628,28,value,EPD_DRAW_ALIGN_LEFT,card_gray);
        snprintf(value,sizeof(value),"Mục tiêu %u phút/ngày",app_settings_home_goal_minutes());label_on(fb,622,628,19,value,EPD_DRAW_ALIGN_RIGHT,0x60,card_gray);
        for(int i=0;i<7;++i)label_on(fb,60+(i*564+282)/7,676,18,days[i],EPD_DRAW_ALIGN_CENTER,0x60,card_gray);
        // 周一为首列。/ Monday is the first column.
        const unsigned start=(unsigned)(s_date.tm_wday+6+35-(s_date.tm_mday-1)%7)%7;
        unsigned reached=0;
        for(unsigned day=1;day<=month_length(&s_date);++day) {
            unsigned cell=start+day-1;int cx=60+((int)(cell%7)*564+282)/7,cy=722+(int)(cell/7)*50;
            uint32_t read=s_reading_days[day-1];bool done=goal&&read>=goal;
            char number[4];snprintf(number,sizeof(number),"%u",day);
            if(done){++reached;epd_fill_circle(cx,cy,20,0x20,fb);label(fb,cx,cy,21,number,EPD_DRAW_ALIGN_CENTER,0xff);}
            else {
                if(read>=60){epd_draw_circle(cx,cy,20,0x60,fb);epd_draw_circle(cx,cy,19,0x60,fb);}
                label_on(fb,cx,cy,21,number,EPD_DRAW_ALIGN_CENTER,day>today?0x90:0x20,card_gray);
            }
            if(day==today)circle_frame(fb,cx,cy,25);
        }
        ui_fill_rect_fast((EpdRect){60,1004,564,1},0xb0,fb);
        static const char *const captions[]={"Hôm nay","Chuỗi hiện tại","Đạt mục tiêu"};
        unsigned streak=home_streak_days(goal);
        for(int i=0;i<3;++i) {
            if(i==0)snprintf(value,sizeof(value),"%u phút",seconds/60);
            else if(i==1)snprintf(value,sizeof(value),streak>=30?"30+ ngày":"%u ngày",streak);
            else snprintf(value,sizeof(value),"%u ngày",reached);
            home_bold_label(fb,154+i*188-ui_text_fixed_width_px(26,value)/2,1030,26,value,EPD_DRAW_ALIGN_LEFT,card_gray);
            label_on(fb,154+i*188,1060,17,captions[i],EPD_DRAW_ALIGN_CENTER,0x60,card_gray);
        }
    }
    HOME_STAGE("calendar");
    ui_nav_draw(fb,0);
    HOME_STAGE("nav");
#undef HOME_STAGE
}
