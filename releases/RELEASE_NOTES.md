## LumenOS 1.5.1 cho Metalio E‑Ink 4 Plus (ESP32-S31)

Bản phát hành chính thức đầu tiên của LumenOS trên kênh `loozzi/lumenos`.

- **Quick Settings mới** (vuốt xuống): bảng trắng gọn với tám nút có nhãn — Wi-Fi, Bluetooth, Đồng bộ, Làm mới, Khoá, Đèn, Xoay, Tối — và hai thanh trượt ngang cho độ sáng, nhiệt độ màu. Khi đọc ngang, bảng thu thành một hàng.
- **Xoay**: chạm để đọc sách ngang hoặc dọc, không dùng cảm biến. Thanh công cụ, bảng cài đặt, ghi chú và màn khóa vẫn hiển thị dọc.
- **Tối**: bật/tắt đảo màu toàn màn hình ngay trong Quick Settings.
- Chọn kết nối hoặc quên riêng từng mạng Wi-Fi đã lưu; thử các mạng còn lại khi mất kết nối.
- CPU chờ đọc ở 80 MHz với remote Bluetooth ổn định; giãn tự kết nối lại khi remote vắng lâu.
- Giữ bộ đệm màn hình qua Light Sleep; xuất log an toàn hơn và thêm thông tin chẩn đoán.

Có hai bản **Full** (`1.5.1`) và **Love Book** (`1.5.1-love`). Chọn đúng bản muốn dùng.

**Cách cài:** LumenOS dùng kênh cập nhật mới, nên máy đang chạy dpReader hoặc LumenOS 1.5.0 cần cài 1.5.1 qua USB một lần (gói `dpReader-metalio47-1.5.1-USB.zip` hoặc Web Flash). Từ 1.5.1 trở đi máy cập nhật qua Wi-Fi từ kênh này. Máy đã dùng bố cục NVS 256 KiB (dpReader 1.4.2 trở lên) giữ nguyên cài đặt khi chọn cập nhật ứng dụng.

Bản này chưa được nghiệm thu đầy đủ trên thiết bị; hãy sao lưu cấu hình ra thẻ SD trước khi cài.

[Web Flash](https://loozzi.github.io/lumenos/) · [Thay đổi theo phiên bản](https://github.com/loozzi/lumenos/blob/main/docs/CHANGELOG.md)
