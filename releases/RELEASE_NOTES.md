## LumenOS 1.5.15 cho Metalio E-Ink 4 Plus (ESP32-S31)

- **Trang đọc rộng hơn:** bỏ thanh trạng thái phía trên, chữ bắt đầu sát mép trên hơn nên mỗi trang thêm dòng.
- **Chân trang một hàng:** tên chương bên trái (không có thì hiện tên sách); số trang, giờ và % sách bên phải, bật/tắt riêng trong Status & Footer. Công tắc "Thanh trạng thái" đổi thành **Giờ ở chân trang**.
- Pin và Wi-Fi/Bluetooth không còn hiện khi đọc. Kiểu tiến độ "Thanh Đáy" giữ vạch sát mép dưới; các kiểu thanh khác không vẽ thanh.

Áp dụng cho **Full 1.5.15** và **Love Book 1.5.15-love**.

Máy đang chạy LumenOS 1.5.1 trở lên cập nhật qua Wi-Fi (OTA) hoặc chọn Cập nhật ứng dụng trên Web Flash, giữ cài đặt và sách. Máy dùng kênh dpReader cũ hoặc LumenOS 1.5.0 cần chuyển kênh qua USB một lần. Gói USB chính thức hỗ trợ cài sạch bố cục cũ; trên bố cục NVS256 chỉ ghi ứng dụng và vùng chọn khởi động.

Đã build và kiểm thử trên máy tính; chưa nạp/nghiệm thu trên thiết bị.

[Web Flash](https://loozzi.github.io/lumenos/) · [Changelog](https://github.com/loozzi/lumenos/blob/main/docs/CHANGELOG.md)
