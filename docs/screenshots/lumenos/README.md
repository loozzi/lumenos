# Minh họa LumenOS 1.5.0

- `startup.png`: kết quả của mã C vẽ màn khởi động, chạy bằng host harness `tools/metalio_startup_host_test.py` trong cây kiểm thử Metalio47. Logo lấy từ asset 4bpp nhúng; không phải ảnh chụp thiết bị.
- `home.png`, `library.png`, `files.png`, `settings.png`: minh họa bố cục Full bằng `tools/render_soft_gray_previews.py`.
- `reader.png`: minh họa trình đọc bằng `tools/render_glass_previews.py`.

Các minh họa dùng phông và icon trong kho, với sách Truyện Kiều của Nguyễn Du cùng dữ liệu mẫu về tiến độ, giờ, mạng và dung lượng. Bìa là hình dựng minh họa. Chúng không xác nhận đầu ra quang học, hiệu năng, trạng thái phần cứng hoặc việc đã nạp thiết bị. Ảnh ngoài thư mục này giữ lịch sử dpReader cũ.

Tạo lại từ thư mục gốc, với Pillow và trình biên dịch C:

```bash
python3 tools/render_readme_previews.py
```

Lệnh tạo cây host test trong `build/readme-host/`, kiểm tra pixel logo và lưu các PNG tại đây; không build Bin, kết nối hay nạp máy đọc sách.
