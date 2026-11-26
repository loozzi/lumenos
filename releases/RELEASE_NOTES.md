## LumenOS 1.6.1 cho Metalio E-Ink 4 Plus (ESP32-S31)

- **Sửa đồng hồ chạy chậm sau khi tắt/mở máy:** trước đây máy ghi giờ của chip ESP (bị trôi khi khóa ngủ lâu) đè lên đồng hồ RTC mỗi lần tắt máy. Nay RTC còn giờ đúng thì không bị ghi đè; chỉ ghi khi đồng bộ qua mạng, đặt giờ trong Cài đặt hoặc qua Web Portal. Sau khi mở khóa, máy lấy lại giờ từ RTC. **Sau khi cập nhật, hãy đồng bộ giờ lại một lần** (bật Wi-Fi hoặc đặt giờ trong Cài đặt).
- **Lối vào Từ vựng:** dòng **Học từ vựng ›** luôn hiện ở góc dưới trái thẻ Cú Lém trên Trang chủ; tiện ích **Từ vựng** mới cho các ô tiện ích.
- Gồm mọi tính năng của 1.6.0: tra từ nhanh và quản lý từ điển, ghi chú cả cuốn và xuất Markdown, trang Từ vựng.

Bản này chỉ phát hành **Full 1.6.1**. Love Book giữ nguyên **1.5.18-love** và không còn được cập nhật.

Máy đang chạy LumenOS 1.5.1 trở lên cập nhật qua Wi-Fi (OTA) hoặc chọn Cập nhật ứng dụng trên Web Flash, giữ cài đặt và sách. Máy dùng kênh dpReader cũ hoặc LumenOS 1.5.0 cần chuyển kênh qua USB một lần.

Đã build và kiểm thử trên máy tính; chưa nạp/nghiệm thu trên thiết bị.

[Web Flash](https://loozzi.github.io/lumenos/) · [Changelog](https://github.com/loozzi/lumenos/blob/main/docs/CHANGELOG.md)
