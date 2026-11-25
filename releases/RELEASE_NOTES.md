## LumenOS 1.5.19 cho Metalio E-Ink 4 Plus (ESP32-S31)

- **Nhắc pin yếu khi tắt chân trang:** nếu công tắc **Giờ & pin ở chân trang** đang tắt, trang đọc hiện thông báo khoảng 3 giây khi pin còn 15% ("hãy sạc sớm") và 8% ("đã giảm đèn để tiết kiệm"). Mỗi mốc chỉ nhắc một lần cho mỗi lần xả pin; sạc xong thì lần sau nhắc lại.
- **Kệ sách đỡ hao pin khi để yên:** sau 1,5 giây không thao tác và khi bìa đã tải xong, Kệ sách cũng được chờ lâu và hạ xung CPU như Trang chủ và trang đọc. Chạm và phím vẫn phản hồi ngay.
- **Tự đo pin:** tệp log (Cài đặt → Nhật ký & Chẩn đoán → Xuất log ra thẻ nhớ) có thêm số liệu điện năng (thời gian chờ hạ xung, số lần quét màn hình, thời gian ngủ khóa, thời gian và độ sáng đèn, pin lúc khóa/mở khóa) và độ trễ (lật trang, thao tác, mở khóa, khởi động) dạng p50/p95.
- Kiểm thử trên máy tính được gom thành một cổng kiểm tra trước phát hành; một số bài kiểm tra cũ được cập nhật theo giao diện hiện tại.

Bản này chỉ phát hành **Full 1.5.19**. Love Book giữ nguyên **1.5.18-love** và không còn được cập nhật.

Máy đang chạy LumenOS 1.5.1 trở lên cập nhật qua Wi-Fi (OTA) hoặc chọn Cập nhật ứng dụng trên Web Flash, giữ cài đặt và sách. Máy dùng kênh dpReader cũ hoặc LumenOS 1.5.0 cần chuyển kênh qua USB một lần. Gói USB chính thức hỗ trợ cài sạch bố cục cũ; trên bố cục NVS256 chỉ ghi ứng dụng và vùng chọn khởi động.

Đã build và kiểm thử trên máy tính; chưa nạp/nghiệm thu trên thiết bị.

[Web Flash](https://loozzi.github.io/lumenos/) · [Changelog](https://github.com/loozzi/lumenos/blob/main/docs/CHANGELOG.md)
