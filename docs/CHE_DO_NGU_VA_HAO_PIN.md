# Chế độ ngủ và hao pin trên Metalio S31

Phạm vi: Metalio E-ink 4 Plus / ESP32-S31, mã nguồn firmware hiện tại. Đây là phân tích đường chạy và phần cứng được bật/tắt trong mã nguồn, chưa phải kết quả đo dòng trên thiết bị. Không dùng số dòng của ESP32-S3/Pico để suy ra dòng toàn máy S31.

| Trạng thái | Hành vi thực tế | Kiểu hao pin dự kiến |
| --- | --- | --- |
| Đang đọc / Home, chưa khóa | CPU và các tài nguyên giao diện vẫn hoạt động; đèn, Wi-Fi, BLE tùy cài đặt. Từ RC22 bật PM để cho phép CPU hạ xung (hiện 80 MHz) trong lượt chờ tĩnh đủ điều kiện, giữ 320 MHz khi hoạt động; không bật tự ngủ nhẹ. | Vẫn tiêu thụ pin khi màn hình đứng yên. Đèn và phiên mạng làm tăng tiêu thụ; hình không đổi không đồng nghĩa CPU đã ngủ. |
| Khóa / ngủ nhẹ | Tắt đèn, ngừng radio, tạm dừng file/font, ngừng LCD, cho cảm ứng ngủ, hạ hai rail qua IO expander rồi gọi light sleep. Giữ RAM và ngữ cảnh để trở lại trang đang đọc. Có lát timer tối đa 60 giây để kiểm tra USB/đường đánh thức. | Thấp hơn trạng thái hoạt động, nhưng RAM và mạch đánh thức vẫn cần điện; timer/đánh thức giả có thể tạo các đợt tăng dòng. |
| Ngủ nhẹ n giờ rồi tắt nguồn | Chọn 1–24 giờ, mặc định 1 giờ. Đây là lựa chọn mặc định S31. Khóa thường cũng dùng quy trình theo số giờ đã chọn. Hết thời gian: lưu/đồng bộ cần thiết rồi thử tắt nguồn phần cứng. Nếu tắt nguồn chưa thành công, giữ khóa/ngủ nhẹ và thử lại; không tự công bố đã tắt. | n giờ đầu là hao pin ngủ nhẹ; sau khi tắt nguồn thành công là mức chờ thấp hơn. Thời gian giữ RAM tùy số giờ đã chọn. |
| Ngắt nguồn hoàn toàn | Lựa chọn trong menu Tắt nguồn; lưu tiến độ/checkpoint, đồng bộ RTC, đóng I/O, tắt đèn/nguồn màn rồi gọi điều khiển nguồn S31. Bật lại bằng nút nguồn và khởi động lại. | Dự kiến thấp nhất trong các trạng thái nghỉ: mạch nguồn/RTC và tự xả pin vẫn tồn tại, không được coi là 0 mA tuyệt đối. |
| Deep sleep của SoC | Có triển khai `APP_SLEEP_DEEP`: dừng radio, đèn, LCD, cảm ứng/rail, giữ chân rồi gọi `esp_deep_sleep_start()`. Là đường dự phòng/nội bộ, không phải lựa chọn riêng trong giao diện S31 hiện tại. | Thấp hơn ngủ nhẹ về phần SoC; dòng cả máy còn phụ thuộc mạch nguồn/rail và linh kiện còn được cấp điện. Chưa có số đo để so chính xác với tắt nguồn. |
| Khóa nhưng bị hoãn ngủ | USB/VBUS, phiên truyền/cập nhật/đồng bộ, I/O chưa sẵn sàng hoặc chuẩn bị ngủ thất bại có thể ngăn vào trạng thái ngủ thật. Có cơ chế đợi/thử lại; trong lúc đó CPU có thể vẫn chạy dù đã có hình khóa. | Cao hơn ngủ nhẹ thực sự. Cần đo khi đã rút USB; không dùng dòng lúc cắm cáp để kết luận hao pin chờ bằng pin. |

Lưu ý về tên chế độ:

- S31 hiện chỉ có hai lựa chọn ở giao diện: **Ngủ nhẹ n giờ rồi tắt nguồn** và **Ngắt nguồn hoàn toàn**. Chúng điều khiển thao tác Tắt nguồn; khóa thông thường vẫn ngủ nhẹ ngay, rồi tắt sau số giờ đã chọn. Giá trị này lưu NVS; chọn bằng nút −/+ trong Cài đặt → Chế độ tắt nguồn / ngủ. Thời gian tự khóa (1/5/10 phút hoặc tắt) là bộ đếm riêng trước khi bắt đầu ngủ nhẹ.
- Enum `APP_SLEEP_LIGHT/DEEP/OFF` và giá trị sleep cũ vẫn có trong phần cài đặt dùng chung, nhưng giá trị `app_settings_sleep_mode()` không được đường khóa S31 hiện tại sử dụng để chọn chế độ.
- `SOFT_SLEEP` của PMU Pico không phải một chế độ riêng đang được dùng trên S31. Không đồng nhất nhãn “ngủ sâu” còn trong một số log/comment với việc chắc chắn đã gọi deep sleep; đường sau thời gian đã chọn ưu tiên tắt nguồn phần cứng.
- Nếu chuẩn bị ngủ hoặc tắt nguồn thất bại, không thể mặc định máy đã đạt mức tiêu thụ thấp chỉ vì màn hình hiển thị khóa.

Chưa có cơ sở để công bố mA, % pin/ngày hoặc số ngày chờ. Để có số đó cần đo dòng tại pin, rút USB, cùng độ sáng/cấu hình radio, tách bốn mốc: đang đọc, vừa khóa, sau thời gian đã chọn, tắt nguồn. Với dòng trung bình đo được `I` mA và dung lượng sử dụng thực `C` mAh: hao pin lý tưởng mỗi ngày xấp xỉ `24 × I / C × 100%`; đây chỉ là phép quy đổi, không thay thế phép đo.

Mã nguồn đối chiếu: [ngủ/khóa S31](../boards/metalio47/src/main/sleep.c), [điều khiển rail và nguồn](../components/metalio_hw/metalio_hw.c), [điều kiện USB](../components/metalio_hw/metalio_usb.c), [giao diện chọn chế độ](../main/apps/app_device_settings.c), [khóa tự động](../main/app/app_loop.c).

Chính sách và giới hạn thử CPU xem [CPU_POWER_POLICY.md](CPU_POWER_POLICY.md).

## Bảo vệ pin yếu (từ 1.5.18)

Mã: [battery_guard_logic.h](../firmware/shared/main/battery_guard_logic.h),
[battery_guard.c](../firmware/shared/main/battery_guard.c). Chỉ chạy trên S31 và chỉ dựa
vào SOC của BQ27220 (`pmu_battery_percent`). Đang sạc thì bỏ mọi hạn chế ngay.

| Mức | Điều kiện | Hành vi |
| --- | --- | --- |
| Cảnh báo | ≤15 % (rời mức khi ≥17 %) | % trên thanh trạng thái và cạnh vạch pin chân trang đổi sang nền đen |
| Tiết kiệm | ≤8 % (rời mức khi ≥10 %) | Đèn tối đa 40 % (không đổi giá trị đã lưu), tự khóa tối đa 1 phút kể cả khi tắt tự khóa, khóa máy tắt nguồn sau 10 phút |
| Nguy hiểm | ≤3 % liên tục 20 giây | Lưu tiến độ (`on_before_lock` và checkpoint), vẽ màn "Pin đã cạn" bằng GC, rồi `app_enter_host_sleep(APP_SLEEP_OFF)`. Khóa máy ở mức này tắt nguồn sau khoảng 5 giây nếu checkpoint đã sẵn |

- Không tắt máy khi có VBUS (`metalio_usb_blocks_sleep()`), khi đang cập nhật/đồng bộ
  (`pico_online_busy`), khi đang ở chế độ tải xuống/USB storage, khi đang phục hồi thẻ
  hay khi đang chạm. Tắt nguồn thất bại thì thử lại sau 60 giây.
- Dữ liệu SOC không hợp lệ không bao giờ đẩy lên mức nguy hiểm, và hủy bước xác nhận.
- Mức chỉ được xét khi máy đang thức. Trong một lần khóa dài, mức được tính một lần lúc
  khóa; giới hạn mặc định "tắt nguồn sau n giờ" vẫn là lớp bảo vệ chính.
- Khi đang khóa (ngủ nhẹ), bấm nút nguồn để đánh thức. Sau khi máy đã tắt nguồn (hết
  n giờ, chọn Ngắt nguồn hoàn toàn hoặc tắt vì cạn pin) thì **nhấn giữ** nút nguồn để
  bật, đúng như dòng chữ trên màn tắt máy.
- OTA tải xuống và cài từ thẻ nhớ bị từ chối khi SOC <30 % mà không cắm sạc.
- Ước tính thời gian đọc còn lại chỉ tính thời gian máy thức: các khoảng cách giữa hai
  vòng UI dài hơn 2 giây (ngủ khóa, thao tác chặn) không được tính. Cần SOC giảm ít
  nhất 3 % trong ít nhất 20 phút thức; không lưu qua khởi động lại; SOC tăng thì đặt lại mốc.

- Từ 1.5.19, khi chân trang không hiện pin (công tắc **Giờ & pin ở chân trang** tắt), trang đọc hiện thẻ thông báo khoảng 3 giây ở mốc cảnh báo và mốc tiết kiệm, mỗi mốc một lần cho mỗi lần xả pin (`battery_guard_pending_notice`/`battery_guard_ack_notice`). Sạc pin hoặc pin hồi về mức thấp hơn thì lần sau nhắc lại. Mức nguy hiểm không có thông báo này, vì đã có màn "Pin đã cạn".

## Tự đo pin trên máy (từ 1.5.19)

Mục **Cài đặt → Nhật ký & Chẩn đoán → Xuất log ra thẻ nhớ** ghi thêm các dòng `Power:` và `Latency` (bộ đếm `metalio_power_stats`, chỉ cộng dồn trong RAM, không lưu qua khởi động lại):

| Trường | Ý nghĩa |
| --- | --- |
| `wait_idle_ms`, `waits_idle` | Tổng thời gian và số lượt chờ được phép hạ xung CPU |
| `wait_legacy_ms`, `waits_legacy` | Tổng thời gian và số lượt chờ theo nhịp 8/40 ms |
| `scans GC/GL/DU` | Số lần quét màn hình theo từng chế độ |
| `light_sleeps`, `light_sleep_ms`, `maintenance_wakes` | Số lần, tổng thời gian ngủ khóa và số lần thức bảo trì 60 giây |
| `lamp_on_ms`, `lamp_level_ms` | Thời gian đèn bật, và tích phân mức sáng × thời gian (đã tính giới hạn tiết kiệm) |
| `lock_soc`, `unlock_soc` | Pin lúc khóa và lúc mở khóa gần nhất |
| `Latency turn/ui/unlock/boot` | Histogram và p50/p95: lật trang, thao tác ở trang khác (từ lúc xử lý input tới khi hiển thị xong), mở khóa (tới khung hình đầu), khởi động (tới khi UI sẵn sàng) |

Cách đo:

1. Rút USB. Giữ cùng sách, font, độ sáng và trạng thái Wi-Fi/Bluetooth giữa các lần so sánh.
2. Khởi động lại máy, sạc đầy hoặc ghi lại % pin ban đầu.
3. Chạy từng kịch bản rồi xuất log ngay sau đó: (a) đọc 30 phút, lật trang mỗi 30 giây; (b) để ở Trang chủ 10 phút; (c) khóa máy 8 giờ; (d) đọc 30 phút bằng remote Bluetooth.
4. So sánh % pin giảm, `wait_idle_ms` so với `wait_legacy_ms`, số lần quét màn hình và `lamp_level_ms` giữa các bản firmware. Công thức quy đổi dòng điện trung bình ở đầu tài liệu này vẫn dùng được nếu đo được dòng điện bằng thiết bị ngoài.

Bộ đếm chỉ ghi nhận; không thay đổi bất kỳ quyết định ngủ, hạ xung hay làm mới màn hình nào.

### Hạng mục đã xem xét nhưng không thay đổi

- Lát hẹn giờ 60 giây trong lúc ngủ khóa: mỗi lần thức bảo trì chỉ vài chục ms, chiếm
  dưới 0,1 % thời gian. Kéo dài lát này thì gần như không tiết kiệm được gì mà lại đụng
  tới lịch sử reset do glitch nguồn của bản 1.2.3, nên giữ nguyên.
- Đọc trạng thái sạc mỗi 2 giây: chỉ là vài lượt I²C ngắn, ghép vào vòng UI vốn đã thức,
  nên giữ nguyên để icon sạc hiện ngay khi cắm.
- Tham số kết nối BLE: trước đây đặt khi kết nối thì bị controller từ chối (HCI 0x12).
  Slave latency chỉ giúp thiết bị ngoại vi tiết kiệm, không giúp máy đọc vốn là central.
  Tăng interval thì đổi lấy độ trễ phím, nên giữ mặc định của stack.
- Tự ngủ nhẹ / tickless idle: chưa bật. Việc giữ LCD/DMA, PSRAM 200 MHz và đèn LEDC
  qua light sleep cần đo trên thiết bị; mô phỏng host không chứng minh được an toàn điện.
