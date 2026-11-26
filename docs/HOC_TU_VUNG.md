# Học từ vựng (LumenOS Full, từ 1.6.0)

Trang **Từ vựng** giúp học từ bằng thẻ, có ôn lặp lại theo hộp Leitner. Firmware không kèm sẵn bộ từ nào: bạn tự chép tệp CSV vào thẻ nhớ. Love Book không có trang này.

## Chép bộ từ

1. Tạo thư mục `/vocab` ở gốc thẻ nhớ.
2. Chép một hoặc nhiều tệp `.csv` vào đó (tối đa 16 tệp, mỗi tệp không quá 2 MiB và 4000 từ). Có thể chép qua USB hoặc Web Portal.
3. Mở trang **Tệp**, vào thẻ nhớ, chạm một tệp `.csv` trong `/vocab`. Trang Từ vựng mở với bộ đó và nhớ nó làm bộ đang học.

Lối vào nhanh (từ 1.6.1): dòng **Học từ vựng ›** ở góc dưới trái thẻ Cú Lém trên Trang chủ (luôn hiện), hoặc gán tiện ích **Từ vựng** vào một ô tiện ích của Trang chủ.

Trước khi chép, có thể kiểm tệp trên máy tính:

```
python3 tools/vocab_pack_check.py my_words.csv
```

Công cụ chỉ đọc tệp và báo dòng lỗi, từ trùng, ô quá dài và cột bị bỏ qua theo đúng quy tắc của máy.

## Định dạng CSV

- UTF-8 (có hoặc không có BOM), dòng đầu là tên cột, phân cách bằng dấu phẩy. Ô có dấu phẩy, xuống dòng hoặc dấu nháy phải đặt trong `"…"`; dấu nháy bên trong viết `""`.
- Tên cột giống bộ dữ liệu TOEIC 600 phổ biến, nên có thể dùng thẳng tệp đó:

| Cột | Bắt buộc | Giới hạn | Hiện ở |
| --- | --- | --- | --- |
| `english` | có | 64 byte | mặt trước |
| `vietnamese` | có | 160 byte | mặt sau |
| `type` | không | 16 byte | mặt trước (n., v., adj.…) |
| `pronounce` | không | 64 byte | mặt trước, chỉ khi phông có đủ ký tự phiên âm |
| `explain` | không | 240 byte | mặt sau |
| `example` | không | 240 byte | mặt sau |
| `example_vietnamese` | không | 240 byte | mặt sau |
| `rank` | không | số | thứ tự học từ mới |

- Cột khác (ví dụ `image_url`, `topic`) bị bỏ qua. Ô dài hơn giới hạn được cắt ở ranh giới ký tự và thêm `…`. Xuống dòng trong ô thành dấu cách.
- Dòng thiếu `english` hoặc `vietnamese`, dòng không phải UTF-8 hợp lệ và từ trùng (không phân biệt hoa/thường ASCII) bị bỏ qua; trang Từ vựng báo số dòng bị bỏ.
- Có cột `rank` thì học từ mới theo `rank` tăng dần; không có thì theo thứ tự dòng.

Ví dụ:

```
english,type,vietnamese,example,example_vietnamese
invoice,n.,"hóa đơn, phiếu tính tiền",Please send the invoice today.,Vui lòng gửi hóa đơn hôm nay.
deadline,n.,hạn chót,The deadline is Friday.,Hạn chót là thứ Sáu.
```

## Học

- Màn chính hiện tên bộ (chạm để đổi bộ khi có nhiều tệp), số từ, số đã học, số đến hạn hôm nay và số từ mới còn lại. **Học N thẻ hôm nay** bắt đầu một lượt.
- Mặt trước là từ, loại từ và phiên âm. Chạm thẻ hoặc phím giữa để lật xem nghĩa, định nghĩa và câu ví dụ. Chọn **Quên** (phím trái) hoặc **Nhớ** (phím phải).
- **Hộp Leitner:** từ mới vào hộp 1. Nhớ thì lên một hộp; hộp 1 đến 5 lần lượt chờ 1, 2, 4, 8 và 16 ngày mới đến hạn lại. Quên thì về hộp 1 và xuất hiện lại cuối lượt (mỗi từ tối đa 3 lần một lượt).
- **Từ mới mỗi ngày:** 5, 10 (mặc định) hoặc 20, chạm để đổi. Lượt học gồm từ đến hạn trước, rồi từ mới cho đến hết hạn mức của ngày.
- Khi máy đã có giờ (Wi-Fi hoặc đặt giờ), ngày tính theo giờ Việt Nam. Khi chưa có giờ, máy dùng "ngày học": học xong hết thẻ của một lượt thì lần học sau được tính là ngày mới. Khi đổi giữa hai cách tính, các từ đã học được coi là đến hạn.

## Lưu tiến độ

- Tiến độ lưu trên thẻ ở `/vocab/.lumen/<tên bộ>.0.prog` và `.1.prog`, ghi luân phiên, có số thứ tự và tổng kiểm. Một bản hỏng thì máy dùng bản còn lại; cả hai hỏng thì bắt đầu lại và báo trên màn hình. Không dùng bộ nhớ cài đặt của máy.
- Máy ghi khi xong một lượt, sau mỗi 20 câu trả lời, khi rời trang và trước khi khóa máy; không ghi sau mỗi lần lật thẻ.
- Tiến độ gắn với chữ ở cột `english`, nên thêm, bớt hay đổi thứ tự dòng không làm lệch. Đổi chữ của một từ sẽ thành từ mới.
- Bộ đang học lưu ở `/vocab/.lumen/active.txt`. Rút thẻ khi đang học thì bỏ phần chưa lưu và không ghi gì lên thẻ.

## Từ hôm nay trên Trang chủ và màn khóa

- Sau mỗi lần mở trang hoặc xong lượt học, máy ghi `/vocab/.lumen/today.txt` (dưới 512 byte) với từ nên học hôm nay: từ đến hạn đầu tiên, nếu không có thì từ mới kế tiếp.
- **Trang chủ:** Cú Lém đọc "Từ hôm nay: …" trong bong bóng ở lần hiện đầu và mỗi lần chạm chẵn; chạm vào bong bóng khi đang hiện từ để mở trang Từ vựng. Tắt bằng **Cú Lém nhắc từ trên Trang chủ**.
- **Màn khóa:** một dải nhỏ ở cuối màn khóa ghi "từ (loại) — nghĩa"; với kiểu khóa dấu trang có chữ ký, dải nằm trên chữ ký. Tắt bằng **Hiện từ trên màn khóa**.
- Trang chủ và màn khóa chỉ dùng bản sao trong RAM, đọc thẻ nhiều nhất một lần mỗi lần gắn thẻ; vẽ màn hình không đọc thẻ và không đánh thức máy thêm. Ảnh khóa đã lưu sẵn dùng lúc khởi động có thể còn từ của lần khóa trước.

## Giới hạn

- Chưa có phát âm, hình ảnh hay tìm kiếm trong bộ từ. Chưa đồng bộ tiến độ giữa các máy.
- Chưa thử trên thiết bị thật; số liệu thời gian chỉ đo trên máy tính.

Kiểm thử: `tools/vocab_host_test.py`, `tools/vocab_pack_check_host_test.py`.
