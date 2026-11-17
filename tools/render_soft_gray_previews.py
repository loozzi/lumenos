#!/usr/bin/env python3
"""Layout previews of the soft-gray firmware pages, using bundled fonts/icons.

Sample books/storage values are illustrative. These are not device captures.
Run: python3 tools/render_soft_gray_previews.py
"""
from pathlib import Path
from PIL import Image, ImageDraw
from render_glass_previews import W,H,font,text,icon,library_page,quantize

OUT=Path(__file__).resolve().parents[1]/'docs/local/soft-gray-previews'
BOOKS=[('Truyện Kiều','Nguyễn Du','EPUB · 1.2 MB'),('Số Đỏ','Vũ Trọng Phụng','EPUB · 1.8 MB'),
       ('Tắt Đèn','Ngô Tất Tố','EPUB · 2.6 MB'),('Dế Mèn phiêu lưu ký','Tô Hoài','EPUB · 3.4 MB'),
       ('Vợ Nhặt','Kim Lân','EPUB · 1.5 MB'),('Chí Phèo','Nam Cao','TXT · 0.8 MB')]
SAMPLE=library_page()

def card(img,rect,radius=24,selected=False):
 x,y,w,h=rect;ImageDraw.Draw(img).rounded_rectangle((x,y,x+w-1,y+h-1),radius,fill=187 if selected else 221)

def fit(img,xy,value,size,width,gray=0):
 while font(size).getlength(value)>width and len(value)>2:value=value[:-2]+'…'
 text(img,xy,value,size,gray)

def cover(img,i,rect):
 x=34+(i%3)*218;y=226+(i//3)*373
 img.paste(SAMPLE.crop((x,y,x+178,y+238)).resize((rect[2],rect[3]),Image.Resampling.LANCZOS),(rect[0],rect[1]))

def shell(active,title=None):
 img=Image.new('L',(W,H),255)
 text(img,(36,18),'10:24',40)
 icon(img,'WIFI',505,40,28)
 text(img,(595,21),'92%',32,anchor='rt')
 d=ImageDraw.Draw(img);d.rounded_rectangle((606,29,645,51),6,outline=0,width=2);d.rectangle((610,33,639,47),fill=0);d.rectangle((648,36,652,43),fill=0)
 if title:text(img,(36,91),title,52)
 navigation(img,active)
 return img

def navigation(img,active):
 # Theo ui_nav_draw (S31): viên nang Trang chủ/Kệ sách/Tệp tin và nút Cài đặt tròn riêng.
 # Mirrors ui_nav_draw (S31): an outlined capsule for Home/Shelf/Files plus a separate round Settings key.
 d=ImageDraw.Draw(img)
 top,h,margin,gap,border,inset=1102,92,36,14,3,7
 gear_x=W-margin-h;capsule_w=gear_x-gap-margin
 d.rounded_rectangle((margin,top,margin+capsule_w-1,top+h-1),h//2,fill=0)
 d.rounded_rectangle((margin+border,top+border,margin+capsule_w-1-border,top+h-1-border),h//2-border,fill=255)
 tabs=[('HOUSE','Trang chủ'),('LIBRARY_BIG','Kệ sách'),('FOLDER','Tệp tin')]
 ix,iy,iw,ih=margin+inset,top+inset,capsule_w-2*inset,h-2*inset
 wide=0
 if 0<=active<=2:
  wide=26+40+12+int(font(30).getlength(tabs[active][1]))+30
  wide=max(200,min(wide,iw-2*110))
 idle=(iw-wide)//(2 if wide else 3);x=ix;cy=top+h//2
 for t,(name,label) in enumerate(tabs):
  w=ix+iw-x if t==2 else wide if t==active and wide else idle
  if t==active:
   d.rounded_rectangle((x,iy,x+w-1,iy+ih-1),ih//2,fill=0)
   cx=x+26+20;icon(img,name,cx,cy,40,255);text(img,(cx+20+12,cy),label,30,255,anchor='lm')
  else:icon(img,name,x+w//2,cy,40)
  x+=w
 d.ellipse((gear_x,top,gear_x+h-1,top+h-1),fill=0)
 if active!=3:d.ellipse((gear_x+border,top+border,gear_x+h-1-border,top+h-1-border),fill=255)
 icon(img,'SETTINGS',gear_x+h//2,cy,44,255 if active==3 else 0)

OWL_HARNESS = r"""
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {int x,y,width,height;} EpdRect;
enum EpdFontFlags {EPD_DRAW_ALIGN_CENTER};
static int W,H;
static void px(int x,int y,uint8_t c,uint8_t*fb){if(x>=0&&y>=0&&x<W&&y<H)fb[y*W+x]=c;}
static void ui_fill_rect_fast(EpdRect r,uint8_t c,uint8_t*fb){for(int y=r.y;y<r.y+r.height;y++)for(int x=r.x;x<r.x+r.width;x++)px(x,y,c,fb);}
static void epd_fill_circle(int cx,int cy,int r,uint8_t c,uint8_t*fb){for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++)if(x*x+y*y<=r*r)px(cx+x,cy+y,c,fb);}
static void epd_draw_circle(int cx,int cy,int r,uint8_t c,uint8_t*fb){for(int a=0;a<4000;a++){float t=a*6.2831853f/4000;px(cx+lroundf(r*cosf(t)),cy+lroundf(r*sinf(t)),c,fb);}}
static float side(float ax,float ay,float bx,float by,float cx,float cy){return (ax-cx)*(by-cy)-(bx-cx)*(ay-cy);}
static void epd_fill_triangle(int x0,int y0,int x1,int y1,int x2,int y2,uint8_t c,uint8_t*fb){
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){float a=side(x,y,x0,y0,x1,y1),b=side(x,y,x1,y1,x2,y2),d=side(x,y,x2,y2,x0,y0);
  if(!(((a<0)||(b<0)||(d<0))&&((a>0)||(b>0)||(d>0))))px(x,y,c,fb);}}
static void label(uint8_t*fb,int x,int y,int p,const char*t,enum EpdFontFlags a,uint8_t g){(void)fb;(void)x;(void)y;(void)p;(void)t;(void)a;(void)g;}
#include "owl.inc"
int main(int argc,char**argv){
 int size=atoi(argv[1]),mood=atoi(argv[2]),wink=atoi(argv[3]),bg=atoi(argv[4]);
 W=size+8;H=size*124/120+8;uint8_t*fb=malloc(W*H);for(int i=0;i<W*H;i++)fb[i]=bg;
 pet_owl_draw(fb,4,4,size,(pet_mood_t)mood,wink,false);
 fwrite(fb,1,W*H,stdout);return 0;}
"""
PET_SOURCE = Path(__file__).resolve().parents[1]/'firmware/full/ui/pet.inc'
OWL_MOODS = {'sleep':0,'miss':1,'wait':2,'reading':3,'happy':4,'proud':5}

def owl(size, mood='reading', wink=False, bg=255):
 """Draw the owl with the firmware's own C drawing code (firmware/full/ui/pet.inc)."""
 import subprocess, tempfile
 source = PET_SOURCE.read_text()
 part = source[source.index('typedef enum { PET_SLEEP'):source.index('/* ---- 心情与文案')]
 with tempfile.TemporaryDirectory() as tmp:
  tmp = Path(tmp)
  (tmp/'owl.inc').write_text(part)
  (tmp/'owl.c').write_text(OWL_HARNESS)
  subprocess.run(['cc','-O1','-o',str(tmp/'owl'),str(tmp/'owl.c'),'-lm'],check=True)
  raw = subprocess.run([str(tmp/'owl'),str(size),str(OWL_MOODS[mood]),str(int(wink)),str(bg)],check=True,capture_output=True).stdout
 return Image.frombytes('L',(size+8,size*124//120+8),raw)

def bubble(img,rect,tail_y,tail_right):
 x,y,w,h=rect;d=ImageDraw.Draw(img)
 d.rounded_rectangle((x,y,x+w-1,y+h-1),20,fill=255,outline=32,width=2)
 edge=x+w-2 if tail_right else x+1;tip=edge+14 if tail_right else edge-14
 d.polygon([(edge,tail_y-10),(tip,tail_y+4),(edge,tail_y+10)],fill=255)
 d.line([(edge,tail_y-10),(tip,tail_y+4),(edge,tail_y+10)],fill=32,width=2)

def home():
 img=shell(0)
 text(img,(36,103),'Chủ nhật, 15 tháng 11',30)
 text(img,(490,104),'28 phút',24,anchor='rt')
 d=ImageDraw.Draw(img);d.ellipse((510,88,570,148),outline=68,width=7)
 d.ellipse((588,88,648,148),fill=238);text(img,(618,99),'L',30,anchor='mt')
 card(img,(36,184,612,378))
 text(img,(60,204),'Đọc tiếp',26)
 # Góc thẻ: cú nhỏ trích câu trong chương đang đọc. / Corner owl quoting the current chapter.
 bubble(img,(192,194,354,72),230,True)
 text(img,(206,214),'“Trăm năm trong cõi người ta, chữ',18,48,anchor='lm')
 text(img,(206,246),'tài chữ mệnh khéo là ghét nhau.”',18,48,anchor='lm')
 img.paste(owl(72,'reading',bg=221),(562,188))
 cover(img,0,(60,278,176,240))
 text(img,(264,292),'Truyện Kiều',40)
 text(img,(264,350),'Nguyễn Du',25,85)
 text(img,(264,465),'Đã đọc 42%',30)
 text(img,(622,466),'Đọc tiếp ›',28,anchor='rt')
 d.rounded_rectangle((264,514,622,518),2,fill=204);d.rounded_rectangle((264,511,414,520),5,fill=0)
 # Sample: minutes read on 1..15 November 2026 against a 30-minute goal.
 minutes=[35,0,42,31,12,0,50,33,0,38,45,31,40,36,28]
 total30=sum(minutes)
 card(img,(36,590,612,300))
 text(img,(60,626),'Cú Lém',28,anchor='lm')
 text(img,(622,626),'Cấp 4 · Mọt sách',19,64,anchor='rm')
 img.paste(owl(150,'reading',bg=221),(54,682))
 bubble(img,(236,656,388,136),728,False)
 text(img,(254,707),'Thêm 2 phút nữa là đạt mục',22,16,anchor='lm')
 text(img,(254,741),'tiêu!',22,16,anchor='lm')
 text(img,(236,818),'Kinh nghiệm 30 ngày',17,64,anchor='lm')
 text(img,(624,818),f'{total30} / 600',17,48,anchor='rm')
 d.rounded_rectangle((236,834,623,843),5,fill=204)
 d.rounded_rectangle((236,834,236+388*(total30-300)//300,843),5,fill=0)
 text(img,(236,866),'1 phút đọc = 1 điểm · Chuỗi 5 ngày',16,96,anchor='lm')
 card(img,(36,910,612,170))
 text(img,(60,944),'Tuần này',24,anchor='lm')
 week=minutes[8:15]
 text(img,(622,944),f'{sum(week)} phút · Cả tháng ›',18,64,anchor='rm')
 for i,name in enumerate(['T2','T3','T4','T5','T6','T7','CN']):
  cx=60+(i*564+282)//7;cy=1036;day=9+i;read=week[i];gray=0
  text(img,(cx,988),name,18,96,anchor='mm')
  if read>=30:d.ellipse((cx-20,cy-20,cx+20,cy+20),fill=32);gray=255
  elif read:d.ellipse((cx-20,cy-20,cx+20,cy+20),outline=96,width=2)
  if day==15:d.ellipse((cx-25,cy-25,cx+25,cy+25),outline=48,width=3)
  text(img,(cx,cy),str(day),21,gray,anchor='mm')
 return img

def library():
 img=shell(1,'Tủ sách');d=ImageDraw.Draw(img)
 icon(img,'CHEVRON_DOWN',263,120,26)
 d.rounded_rectangle((412,92,579,147),28,fill=255,outline=0,width=2)
 d.rectangle((434,119,453,121),fill=0);d.rectangle((443,110,445,129),fill=0)
 text(img,(464,120),'Nhập sách',19,anchor='lm')
 d.ellipse((592,92,647,147),fill=221)
 for k in (-1,0,1):d.ellipse((617+k*13,117,622+k*13,122),fill=0)
 d.rounded_rectangle((36,160,503,215),28,fill=221)
 icon(img,'SEARCH',70,188,24,64);text(img,(96,188),'Tìm theo tên sách, tác giả…',19,96,anchor='lm')
 d.rounded_rectangle((516,160,647,215),28,fill=221)
 icon(img,'TEXT_ALIGN_START',546,188,24);text(img,(566,188),'Gần đây',17,anchor='lm')
 tabs=[('Tất cả','9'),('Đang đọc','3'),('Chưa đọc','3'),('Đã xong','3'),('Yêu thích','2')]
 widths=[font(19).getlength(n)+6+font(15).getlength(c) for n,c in tabs]
 gap=(612-sum(widths))/4;x=36;cy=250
 d.line((36,275,647,275),fill=144)
 for i,((name,count),w) in enumerate(zip(tabs,widths)):
  text(img,(x,cy),name,19,0 if i==0 else 64,anchor='lm')
  text(img,(x+w,cy),count,15,96,anchor='rm')
  if i==0:d.rounded_rectangle((x,272,x+w,275),2,fill=0)
  x+=w+gap
 rows=[('Truyện Kiều','Nguyễn Du',42,'EPUB · Máy',0),('Dế Mèn phiêu lưu ký','Tô Hoài',17,'EPUB · Thẻ nhớ',3),
       ('Chí Phèo','Nam Cao',73,'TXT · Thẻ nhớ',5),('Tắt Đèn','Ngô Tất Tố',0,'EPUB · Máy',2),
       ('Số Đỏ','Vũ Trọng Phụng',100,'EPUB · Thẻ nhớ',1)]
 for r,(title,author,pct,source,c) in enumerate(rows):
  top=284+r*144
  cover(img,c,(36,top+8,96,128))
  lines=[title]
  if font(24).getlength(title)>492:
   words=title.split();first=''
   while words and font(24).getlength((first+' '+words[0]).strip())<=492:first=(first+' '+words.pop(0)).strip()
   second=' '.join(words)
   while font(24).getlength(second)>492:second=second[:-2]+'…'
   lines=[first,second]
  two=len(lines)==2
  for i,line in enumerate(lines):text(img,(156,top+(26 if two else 34)+30*i),line,24,anchor='lm')
  text(img,(156,top+(88 if two else 70)),author,18,80,anchor='lm')
  my=top+(118 if two else 106)
  d.rounded_rectangle((156,my-4,355,my+3),4,fill=208)
  if pct:d.rounded_rectangle((156,my-4,156+max(8,200*pct//100)-1,my+3),4,fill=0)
  text(img,(370,my),'Đã xong' if pct==100 else f'{pct}%' if pct else 'Chưa đọc',16,anchor='lm')
  text(img,(648,my),source,16,96,anchor='rm')
  if r<4:d.line((36,top+143,647,top+143),fill=144)
 icon(img,'PAGER_CHEVRON_FIRST',72,1053,36,170);icon(img,'CHEVRON_LEFT',160,1053,36,170)
 text(img,(342,1053),'9 cuốn · 01/02',21,anchor='mm')
 icon(img,'CHEVRON_RIGHT',524,1053,36);icon(img,'PAGER_CHEVRON_LAST',612,1053,36)
 return img

def pager(img):
 icon(img,'CHEVRON_LEFT',190,1040,26)
 text(img,(342,1024),'Trang 1 / 1',22,anchor='mt')
 icon(img,'CHEVRON_RIGHT',494,1040,26)

def files():
 img=shell(2,'Tệp tin');card(img,(36,171,612,104))
 text(img,(58,184),'Thẻ nhớ',27);text(img,(624,187),'32 GB',21,anchor='rt')
 text(img,(58,216),'Đã dùng 12.4 GB / 32 GB',19,85)
 d=ImageDraw.Draw(img);d.rounded_rectangle((58,253,623,260),4,fill=160);d.rounded_rectangle((58,253,277,260),4,fill=64)
 text(img,(42,284),'Truyền tệp',21)
 for i,(name,label,detail) in enumerate([('WIFI','WiFi','Cùng mạng'),('RADIO_TOWER','Điểm phát','Không cần mạng'),('SAVE','USB','Cáp máy tính')]):
  x=36+i*208;card(img,(x,318,196,76),20);icon(img,name,x+32,356,34)
  text(img,(x+58,330),label,19);text(img,(x+58,360),detail,15,85)
 text(img,(36,428),'Thư mục gốc · 7 mục',21)
 card(img,(537,404,111,50),18);text(img,(592,416),'Tên ↑',19,anchor='mt')
 rows=[('FOLDER','Books','Thư mục sách'),('FOLDER','Documents',None),('FOLDER','fonts','Thư mục phông'),('FOLDER','pictures',None),
       ('BOOK_OPEN','Dế Mèn phiêu lưu ký.epub','3.4 MB'),('BOOK_OPEN','Số Đỏ.txt','412.6 KB')]
 for i,(name,label,detail) in enumerate(rows):
  y=468+i*84;folder=name=='FOLDER'
  if i:d.line((96,y,628,y),fill=160,width=2)
  icon(img,name,73,y+42,46 if folder else 40)
  if folder:
   text(img,(101,y+42),label,23,anchor='lm')
   if detail:text(img,(596,y+42),detail,17,85,anchor='rm')
  else:text(img,(101,y+13),label,23);text(img,(101,y+47),detail,19,85)
  icon(img,'CHEVRON_RIGHT',630,y+42,24,96)
 icon(img,'CHEVRON_LEFT',190,1031,26,170)
 text(img,(342,1020),'Trang 1 / 2',20,anchor='mt')
 icon(img,'CHEVRON_RIGHT',494,1031,26)
 return img

def settings():
 img=shell(3,'Cài đặt');card(img,(36,180,612,106))
 card(img,(57,192,82,82),20,True);text(img,(98,207),'L',40,anchor='mt')
 text(img,(164,211),'LumenOS',32);text(img,(588,218),'Sửa',20,85,anchor='rt');icon(img,'CHEVRON_RIGHT',625,233,26)
 rows=[('WIFI','Kết nối','WiFi · Bluetooth · KOSync · OPDS'),('SUN','Hiển thị','Phông chữ · Làm mới · Nguồn & Ngủ'),
       ('SLIDERS_HORIZONTAL','Cá nhân hóa','Tủ sách · Màn hình khóa · Chữ ký'),('BLUETOOTH','Điều khiển','Phím cứng & Cử chỉ'),
       ('DOWNLOAD','Cập nhật & Khôi phục','Cập nhật · Sao lưu · Chẩn đoán')]
 for i,(name,label,detail) in enumerate(rows):
  y=314+i*136;card(img,(36,y,612,124),22);icon(img,name,78,y+56,44)
  text(img,(122,y+14),label,27);fit(img,(122,y+71),detail,19,466,85)
  icon(img,'CHEVRON_RIGHT',625,y+56,26)
 return img

def child_shell(title):
 img=shell(3)
 icon(img,'ARROW_LEFT',58,114,48)
 text(img,(108,91),title,34)
 return img

def group_page(display=False):
 title='Hiển thị' if display else 'Kết nối'
 img=child_shell(title)
 text(img,(42,191),title,20)
 rows=[('TYPE','Phông chữ hệ thống','Mặc định · 100%'),('CONTRAST','Độ tương phản hệ thống','130%'),
       ('SLIDERS_HORIZONTAL','Chế độ làm mới Trang chủ','Thường'),('REFRESH_CW','Làm mới kỹ Trang chủ','Tắt'),
       ('EYE_CLOSED','Tự động khóa màn hình','5 phút'),('LOCK','Nhấn đúp mở khóa','Bật'),
       ('POWER','Chế độ tắt nguồn / ngủ','Sleep và tắt nguồn'),('SUN','Đèn nền & Rung','Độ sáng & Nhiệt màu'),
       ('CONTRAST','Đảo màu','Tắt')] if display else [
       ('WIFI','Kết nối WiFi','Đã kết nối'),('EYE_CLOSED','Tự tắt WiFi khi rảnh','5 phút'),
       ('BLUETOOTH','Điều khiển lật trang BLE','Tắt'),('EYE_CLOSED','Tự tắt Bluetooth khi rảnh','Tắt'),
       ('REFRESH_CW','Đồng bộ KOSync','Chưa cấu hình'),('LIBRARY_BIG','Kho sách OPDS','Kho trực tuyến'),
       ('CLOCK','Ngày & Giờ','Cài đặt')]
 for i,(name,label,value) in enumerate(rows):
  y=240+i*82;card(img,(36,y+4,612,74),18);icon(img,name,67,y+41,30)
  fit(img,(100,y+23),label,25,328)
  if display and i==5:
   d=ImageDraw.Draw(img);d.rounded_rectangle((555,y+22,623,y+60),19,fill=0);d.ellipse((588,y+25,620,y+57),fill=255)
  else:
   while font(20).getlength(value)>148 and len(value)>2:value=value[:-2]+'…'
   text(img,(594,y+26),value,20,68,anchor='rt');icon(img,'CHEVRON_RIGHT',625,y+41,26)
 return img

def wifi_page():
 img=child_shell('Mạng WiFi');text(img,(42,182),'Mạng hiện tại',24)
 card(img,(36,224,612,154),22)
 icon(img,'WIFI',74,265,28);text(img,(104,242),'Home WiFi',29)
 text(img,(64,298),'Đã kết nối',23);text(img,(64,338),'192.168.1.24',19,68)
 card(img,(501,244,123,56),16,True);text(img,(562,259),'Kết nối',24,anchor='mt')
 card(img,(501,315,123,52),16);text(img,(562,329),'Quên',24,anchor='mt')
 text(img,(42,414),'Mạng tìm thấy',24);text(img,(642,414),'3 mạng',21,anchor='rt')
 for i,(name,detail) in enumerate([('Home WiFi','Sóng mạnh · Có mật khẩu'),('Reader Studio','Sóng tốt · Có mật khẩu'),('Guest Network','Sóng tốt · Mạng mở')]):
  y=465+i*95;card(img,(36,y+4,612,87),18)
  text(img,(64,y+12),name,28);text(img,(64,y+57),detail,21,68);icon(img,'CHEVRON_RIGHT',615,y+47,26)
 card(img,(36,883,612,66),16);text(img,(342,902),'Quét lại',24,anchor='mt')
 text(img,(42,976),'Hỗ trợ WiFi 2.4 GHz (kênh 1–13)',21)
 text(img,(42,1024),'WiFi vẫn giữ kết nối sau khi thoát trang.',19,68)
 return img

def font_page():
 img=child_shell('Phông chữ hệ thống')
 card(img,(36,197,612,118),22);text(img,(60,220),'Cỡ chữ hệ thống',27);text(img,(589,228),'100%',27,anchor='rt')
 text(img,(60,274),'Chạm để chọn cỡ chữ',18,68)
 text(img,(40,344),'Xem trước hiệu ứng',22)
 card(img,(36,398,612,154),22);text(img,(60,420),'Đọc sách mỗi ngày cùng LumenOS',29)
 text(img,(60,494),'Aa 123',25)
 text(img,(40,580),'Chọn phông chữ',22)
 for i,label in enumerate(['Phông mặc định (Tích hợp)','Bitter Regular','Phông trên thẻ nhớ']):
  y=640+i*94;card(img,(36,y,612,78),20,i==0);text(img,(60,y+20),label,26)
  if i==0:ImageDraw.Draw(img).ellipse((607,y+33,619,y+45),fill=0)
 return img

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 sheet=Image.new('RGB',(W*2+72,H*2+156),'#e4e4e1');d=ImageDraw.Draw(sheet)
 for i,(name,draw) in enumerate([('Trang chủ',home),('Tủ sách',library),('Tệp tin',files),('Cài đặt',settings)]):
  img=quantize(draw());img.save(OUT/f'{i+1}-page.png')
  x=24+(i%2)*(W+24);y=42+(i//2)*(H+48)
  sheet.paste(img,(x,y));d.text((x,y-30),name,font=font(23),fill='#333333')
 d.text((24,2*H+116),'LAYOUT PREVIEW · SAMPLE DATA · NOT A DEVICE CAPTURE',font=font(20),fill='#444444')
 sheet.save(OUT/'pages.png')
 children=Image.new('RGB',(W*2+72,H*2+156),'#e4e4e1');cd=ImageDraw.Draw(children)
 for i,(name,draw) in enumerate([('Kết nối',lambda:group_page(False)),('Hiển thị',lambda:group_page(True)),('Mạng WiFi',wifi_page),('Phông chữ hệ thống',font_page)]):
  img=quantize(draw());img.save(OUT/f'child-{i+1}.png')
  x=24+(i%2)*(W+24);y=42+(i//2)*(H+48)
  children.paste(img,(x,y));cd.text((x,y-30),name,font=font(23),fill='#333333')
 cd.text((24,2*H+116),'LAYOUT PREVIEW · SAMPLE DATA · NOT A DEVICE CAPTURE',font=font(20),fill='#444444')
 children.save(OUT/'children.png')
 print(OUT/'pages.png')
if __name__=='__main__':main()
