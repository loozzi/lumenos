## LumenOS 1.6.0 cho Metalio E-Ink 4 Plus (ESP32-S31)

- **Tra từ nhanh hơn:** lần tra đầu với một bộ StarDict lập mốc chỉ mục trong bộ nhớ; các lần sau chỉ đọc một đoạn nhỏ của chỉ mục thay vì cả tệp. Không tìm thấy thì gợi ý tối đa 5 từ gần nhất, chạm để tra lại.
- **Quản lý từ điển:** nút **Quản lý** trong màn tra để bật/tắt và sắp thứ tự các bộ trong `/dict`; thứ tự lưu ở `/dict/.lumen_order` trên thẻ.
- **Ghi chú cả cuốn:** trong **Các ghi chú**, tab **Cả cuốn** liệt kê mọi ghi chú của sách theo chương; chạm để tới đúng đoạn, kể cả ở chương khác. **Xuất ra thẻ** lưu tất cả thành tệp `ghi-chu.md` (Markdown).
- **Học từ vựng (mới):** trang **Từ vựng** học bằng thẻ, ôn lặp lại theo 5 hộp (1/2/4/8/16 ngày), 5/10/20 từ mới mỗi ngày. Bạn tự chép tệp CSV vào thư mục `/vocab` trên thẻ (cùng tên cột với bộ TOEIC 600 phổ biến: `english,type,vietnamese,…`); firmware không kèm dữ liệu. Mở từ trang **Tệp** hoặc bong bóng của Cú Lém. Cú Lém nhắc "Từ hôm nay" trên Trang chủ, màn khóa có thể hiện từ đang học. Hướng dẫn: [docs/HOC_TU_VUNG.md](https://github.com/loozzi/lumenos/blob/main/docs/HOC_TU_VUNG.md).
- Thêm nhiều bài kiểm thử trên máy tính và sửa các bài kiểm thử cũ.

Bản này chỉ phát hành **Full 1.6.0**. Love Book giữ nguyên **1.5.18-love** và không còn được cập nhật.

Máy đang chạy LumenOS 1.5.1 trở lên cập nhật qua Wi-Fi (OTA) hoặc chọn Cập nhật ứng dụng trên Web Flash, giữ cài đặt và sách. Máy dùng kênh dpReader cũ hoặc LumenOS 1.5.0 cần chuyển kênh qua USB một lần. Gói USB chính thức hỗ trợ cài sạch bố cục cũ; trên bố cục NVS256 chỉ ghi ứng dụng và vùng chọn khởi động.

Đã build và kiểm thử trên máy tính; chưa nạp/nghiệm thu trên thiết bị.

[Web Flash](https://loozzi.github.io/lumenos/) · [Changelog](https://github.com/loozzi/lumenos/blob/main/docs/CHANGELOG.md)
