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
 for i,(name,label) in enumerate([('HOUSE','Trang chủ'),('LIBRARY_BIG','Kệ sách'),('FOLDER_OPEN','Tệp tin'),('SLIDERS_HORIZONTAL','Cài đặt')]):
  cx=(2*i+1)*W//8
  if i==active:card(img,(cx-70,1106,140,104),20,True)
  icon(img,name,cx,1141,44);text(img,(cx,1170),label,17,anchor='mt')
 return img

def home():
 img=shell(0)
 text(img,(36,103),'Chủ nhật, 15 tháng 11',30)
 text(img,(490,104),'28 phút',24,anchor='rt')
 d=ImageDraw.Draw(img);d.ellipse((510,88,570,148),outline=68,width=7)
 d.ellipse((588,88,648,148),fill=238);text(img,(618,99),'L',30,anchor='mt')
 card(img,(36,184,612,378))
 text(img,(60,204),'Đọc tiếp',26)
 cover(img,0,(60,278,176,240))
 text(img,(264,292),'Truyện Kiều',40)
 text(img,(264,350),'Nguyễn Du',25,85)
 text(img,(264,465),'Đã đọc 42%',30)
 text(img,(622,466),'Đọc tiếp ›',28,anchor='rt')
 d.rounded_rectangle((264,514,622,518),2,fill=204);d.rounded_rectangle((264,511,414,520),5,fill=0)
 # Sample month: minutes read on 1..15 November 2026 against a 30-minute goal.
 minutes=[35,0,42,31,12,0,50,33,0,38,45,31,40,36,28]
 card(img,(36,590,612,490))
 text(img,(60,610),'Tháng 11',28);text(img,(622,614),'Mục tiêu 30 phút/ngày',19,85,anchor='rt')
 for i,name in enumerate(['T2','T3','T4','T5','T6','T7','CN']):text(img,(60+(i*564+282)//7,664),name,18,85,anchor='mt')
 for day in range(1,31):
  cell=6+day-1;cx=60+(cell%7*564+282)//7;cy=722+cell//7*50
  read=minutes[day-1] if day<=15 else 0;gray=0 if day<=15 else 150
  if read>=30:d.ellipse((cx-20,cy-20,cx+20,cy+20),fill=32);gray=255
  elif read:d.ellipse((cx-20,cy-20,cx+20,cy+20),outline=96,width=2)
  if day==15:d.ellipse((cx-25,cy-25,cx+25,cy+25),outline=48,width=3)
  text(img,(cx,cy-12),str(day),21,gray,anchor='mt')
 d.line((60,1004,623,1004),fill=176)
 for i,(value,caption) in enumerate([('28 phút','Hôm nay'),('5 ngày','Chuỗi hiện tại'),('10 ngày','Đạt mục tiêu')]):
  text(img,(154+i*188,1016),value,26,anchor='mt');text(img,(154+i*188,1050),caption,17,85,anchor='mt')
 return img

def library():
 img=shell(1,'Tủ sách');d=ImageDraw.Draw(img)
 icon(img,'CHEVRON_DOWN',263,120,26)
 for i,name in enumerate(['SEARCH','TEXT_ALIGN_START','PENCIL',None]):
  x=416+i*60;d.ellipse((x,94,x+51,145),fill=221)
  if name:icon(img,name,x+26,120,28)
  else:d.rectangle((x+15,119,x+36,121),fill=0);d.rectangle((x+25,109,x+27,130),fill=0)
 x=36
 for i,label in enumerate(['Tất cả','Đang đọc','Chưa đọc','Đã xong','Yêu thích']):
  w=int(font(18).getlength(label))+30
  d.rounded_rectangle((x,160,x+w-1,199),20,fill=0 if i==0 else 221)
  text(img,(x+w//2,169),label,18,255 if i==0 else 0,anchor='mt');x+=w+9
 for i in range(9):
  title,percent=(BOOKS[i][0],[42,100,0,17,0,73][i]) if i<6 else [('Lão Hạc',100),('Bỉ Vỏ',5),('Vang bóng một thời',0)][i-6]
  x=46+(i%3)*214;y=212+(i//3)*262
  cover(img,i%6,(x,y,164,214))
  while font(19).getlength(title)>164:title=title[:-2]+'…'
  text(img,(x,y+230),title,19,anchor='lm')
  d.rounded_rectangle((x,y+247,x+109,y+252),3,fill=208)
  if percent:d.rounded_rectangle((x,y+247,x+max(6,110*percent//100)-1,y+252),3,fill=0)
  text(img,(x+164,y+250),'Xong' if percent==100 else f'{percent}%' if percent else 'Mới',15,80,anchor='rm')
 icon(img,'CHEVRON_LEFT',190,1031,26)
 text(img,(342,1020),'9 cuốn · 01/01',20,anchor='mt')
 icon(img,'CHEVRON_RIGHT',494,1031,26)
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
