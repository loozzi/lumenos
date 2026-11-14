# App trên LumenOS

Kho App là danh sách ứng dụng được biên dịch cùng firmware, dùng được ngoại tuyến
và không cần thẻ nhớ. Bản đầu tiên có Cờ caro. Thêm app mới hiện cần cập nhật
firmware; chưa có tải/cài/xóa app độc lập hoặc chạy mã từ thẻ nhớ.

Thanh dưới theo thứ tự **Trang chủ | App | Kệ sách | Tệp tin**. Nút Cài đặt trong
App luôn mở cài đặt hệ thống, kể cả khi người dùng đã đổi các lối tắt Home.
Home bìa toàn màn hình có mục App trong menu bật lên. Love Book vẫn dùng trình
chọn sách khi vào Kệ sách, không nạp lại phần thư viện đã lược bỏ.

## Cờ caro

Vào App → Cờ caro. Hai người chơi cùng máy, X đi trước rồi luân phiên với O.
Chạm ô trống trên bàn 15×15 để đặt quân. Nước mới nhất có nền xám.

- Từ năm quân liên tiếp theo hàng ngang, dọc hoặc hai đường chéo là thắng.
- Chuỗi dài hơn năm vẫn thắng; bị chặn hai đầu vẫn thắng.
- Bàn đầy mà không có người thắng thì hòa. Ván kết thúc không nhận nước mới.
- Đi lại bỏ đúng một nước, kể cả nước thắng; lượt trả cho người vừa đi.
- Ván mới cần chạm xác nhận lần nữa nếu đã có nước đi. Chạm nơi khác hủy xác nhận và không đặt quân.
- Phím trái về App, giữa làm mới toàn màn hình, phải đi lại một nước. Trên trang App, phím giữa mở Cờ caro; trái/phải chuyển tab theo thứ tự thanh dưới.

Chuyển trang, khóa màn hình và Light Sleep giữ ván trong RAM. Reboot, mất điện
hoặc tắt nguồn làm mất ván. Không ghi nước đi vào NVS/thẻ, không có tác vụ nền,
đồng hồ ván hay đối thủ máy. Giao diện chỉ yêu cầu làm mới khi thao tác thay đổi
trạng thái; không có animation hoặc vòng lặp quét bàn cờ liên tục.

## Thêm app

Danh mục ở `firmware/shared/main/apps/app_apps.c`: mỗi mục gồm tên, mô tả và
`app_desc_t` đích. App có trang và vòng đời riêng, dùng hợp đồng `app_desc_t`
trong `main/app/app.h`; hàm render chỉ vẽ. App không gọi trực tiếp trang khác
để vẽ hoặc tự thay vòng lặp chính. Gán `ctx->request_app` để điều hướng.

Khi thêm nguồn, cập nhật `main/CMakeLists.txt`, ánh xạ `firmware/layout.json`
và liên kết tương thích dưới `main/`. Đăng ký trang trong `app_registry.c`.
Giữ định danh các trang cũ: Home=0, sách=1, tệp=2, Cài đặt=3, ảnh=4,
chọn sách=5; App=6, Cờ caro=7. Thứ tự hiển thị thanh dưới được ánh xạ riêng
trong `ui_nav.c`, không đổi định dạng bản ghi khởi động cũ. App/game sau reboot
về Home; khôi phục trong Light Sleep vẫn dùng trang hiện tại của vòng lặp.

Luật cờ thuần C ở `caro.c/.h`, không phụ thuộc SDK/UI, trạng thái dưới 1 KiB.
Trang game không giữ tệp hay tài nguyên Wi-Fi/Bluetooth, dùng bộ vẽ/làm mới
hiện hành và giữ vùng nội dung tách khỏi thanh dưới.

## Kiểm thử

```sh
python3 tools/board_source_view.py --board metalio47 --tests --output build/apps-host
(cd build/apps-host && python3 tools/apps_host_test.py)
```

Bộ host biên dịch luật và callback UI thực với header hợp đồng trang thực;
kiểm tra thắng bốn hướng, chuỗi nối giữa, hòa, đi lại, biên tọa độ, xác nhận,
chuyển tab và giữ ván khi gọi lại vòng đời trang. UI dùng bộ vẽ giả để kiểm tra
vùng vẽ không đè thanh dưới. Cần thử trên máy thật để đánh giá cảm ứng, độ rõ
quân cờ, bóng mờ và khóa/đánh thức phần cứng; host test không thay nghiệm thu đó.
