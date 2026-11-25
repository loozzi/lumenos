## LumenOS 1.5.18 cho Metalio E-Ink 4 Plus (ESP32-S31)

- **Báo pin yếu:** từ 15% trở xuống (khi không sạc), số % trên thanh trạng thái và cạnh vạch pin ở chân trang trang đọc chuyển sang nền đen.
- **Tiết kiệm khi pin rất yếu:** từ 8% trở xuống, đèn nền được giới hạn ở 40% và máy tự khóa sau 1 phút không thao tác. Chỉ áp dụng trong lúc pin yếu, không đổi cài đặt đã lưu; cắm sạc là trở lại bình thường. Khóa máy lúc này sẽ tắt nguồn sau 10 phút thay vì chờ đủ số giờ đã chọn.
- **Tắt máy an toàn khi cạn pin:** từ 3% trở xuống trong khoảng 20 giây, máy lưu tiến độ đọc, hiện màn "Pin đã cạn" rồi tắt nguồn. Sạc pin, rồi nhấn giữ nút nguồn để bật lại. Không tắt khi đang cắm sạc, đang cập nhật hay đang chạm.
- **Bảo vệ khi cập nhật:** cập nhật qua Wi-Fi hoặc thẻ nhớ cần pin từ 30% trở lên, hoặc phải cắm sạc.
- **Thời gian đọc còn lại:** trang thông tin thiết bị hiện ước tính "Còn khoảng … giờ đọc" sau khi máy đã đo đủ mức hao pin thực tế (tối thiểu 20 phút dùng và giảm 3%).
- **Đỡ hao pin khi để yên:** Trang chủ, Tệp, Cài đặt và màn chọn sách cũng được phép chờ lâu và hạ xung CPU khi không có thao tác, như trang đọc; khi rảnh, mỗi lượt chờ dài tối đa 1 giây thay vì 0,2 giây. Chạm và phím vẫn được phản hồi ngay. Kệ sách giữ nhịp cũ.

Áp dụng cho **Full 1.5.18** và **Love Book 1.5.18-love**.

Máy đang chạy LumenOS 1.5.1 trở lên cập nhật qua Wi-Fi (OTA) hoặc chọn Cập nhật ứng dụng trên Web Flash, giữ cài đặt và sách. Máy dùng kênh dpReader cũ hoặc LumenOS 1.5.0 cần chuyển kênh qua USB một lần. Gói USB chính thức hỗ trợ cài sạch bố cục cũ; trên bố cục NVS256 chỉ ghi ứng dụng và vùng chọn khởi động.

Đã build và kiểm thử trên máy tính; chưa nạp/nghiệm thu trên thiết bị.

[Web Flash](https://loozzi.github.io/lumenos/) · [Changelog](https://github.com/loozzi/lumenos/blob/main/docs/CHANGELOG.md)
