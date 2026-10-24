# Thiết kế khoá an toàn reed switch (breakaway) — chống ngã cho người chạy

Phiên bản 2026-10-24. Tình huống bảo vệ đã chốt: **trẻ là người chạy trên băng, khoá đeo trên người chạy**, nam châm **kẹp áo + dây co giãn**, mức can thiệp: **cắt cứng phần cứng + báo firmware**.

## 1. Nguyên lý
Reed switch là tiếp điểm đóng bằng từ trường: nam châm ở gần = đóng, rời ra = mở. Nó không "phát hiện ngã" — cơ cấu cơ học biến cú ngã thành sự kiện mất nam châm:

```
Áo trẻ --[kẹp]--> puck nam châm ))) từ trường ((( reed trong dock trên máy
                     ^                                        |
                     +----------- dây co giãn ----------------+
   Trẻ ngã / bị cuốn ra sau quá chiều dài dây
        -> puck bị giật bứt khỏi dock -> reed MỞ -> dừng máy
```

## 2. Đấu dây fail-safe (2 đường độc lập cùng 1 tiếp điểm)
Trạng thái "được phép chạy" = tiếp điểm reed ĐÓNG. Mọi lỗi (bứt nam châm, đứt dây, tuột giắc, gãy reed) đều = hở mạch = dừng.

**Đường A — cắt cứng phần cứng (đường chính, không phụ thuộc firmware):**
Tiếp điểm reed mắc **nối tiếp trên đường tín hiệu PWM từ ESP32 GPIO5 xuống chân ENA của L298N**, chân ENA phía driver có điện trở pull-down 10 kΩ xuống GND:
```
ESP32 GPIO5 (PWM) ----[ reed ]----+----> L298N ENA
                                  |
                                 10kΩ
                                  |
                                 GND
```
- Reed chỉ đóng cắt dòng tín hiệu logic (chân ENA là ngõ vào trở kháng cao, dòng µA) nên không lo dính/hàn tiếp điểm như khi đóng cắt dòng motor.
- Reed mở -> ENA bị kéo xuống 0 -> driver ngắt motor ngay, kể cả khi ESP32 treo (đây là lớp độc lập mà P0-2 trong SAFETY_SUGGESTIONS.md đang thiếu).
- Khi puck gắn đúng, tiếp điểm đóng ổn định nên PWM truyền nguyên vẹn; hiện tượng nảy tiếp điểm chỉ xảy ra đúng lúc bứt (mà lúc đó mục tiêu là dừng).

**Đường B — firmware (gate + báo cáo + latch):**
```
3V3 --[pull-up nội ESP32]-- GPIO23 ----[ reed (cùng tiếp điểm hoặc tiếp điểm thứ 2 của reed 2 cực) ]---- GND
```
- Nếu dùng reed 1 tiếp điểm: nối GPIO23 vào nút giữa reed và ENA ở đường A (điểm có pull-down 10k) — nhưng lúc đó logic mức bị đảo (đóng = HIGH). Khuyến nghị dùng **reed loại 2 tiếp điểm (SPDT/form C) hoặc 2 reed song song** để 2 đường độc lập thật sự; phương án tối thiểu 1 tiếp điểm dùng điểm chung và đảo logic trong `safety_key.c` (`read_key_closed()`), ghi rõ khi lắp.
- Thêm tụ 100 nF từ GPIO23 xuống GND chống nhiễu dọc đường dây dài từ dock về board.
- GPIO mặc định 23, đổi được qua menuconfig (Treadmill Safety Key).

## 3. Hành vi firmware (đã code trên branch `feat/reed-safety-key`)
- Poll GPIO mỗi 5 ms qua logic thuần `safety_key_logic.h` (host test 18 checks):
  - xác nhận "có key": tiếp điểm đóng liên tục **100 ms** (chậm để tin);
  - xác nhận "mất key": tiếp điểm mở liên tục **30 ms** (nhanh để dừng, đủ lọc nảy tiếp điểm/rung);
- Mất key khi đã từng có key -> **TRIP chốt (latch)**: callback gọi `treadmill_hard_cutoff()` (cắt tức thì, không qua dừng mềm), server gửi alert mới `TREADMILL_ALERT_SAFETY_KEY_OUT = 6`, smartkey hiện "KHOA AN TOAN / DA BI BUT RA" và khoá điều khiển.
- Gắn lại puck **không** tự cho chạy lại: lần bấm ON chủ động tiếp theo (khi puck đã gắn) mới là hành động xác nhận xoá latch — và nếu puck chưa gắn thì ON bị từ chối với đúng alert SAFETY_KEY_OUT thay vì CMD_REJECTED chung chung.
- Không có puck lúc khởi động: ON bị từ chối (giống điều kiện zone SAFE của VL53). Lớp reed bảo vệ *người đeo key*; người đến gần không đeo key vẫn do lớp ToF phụ trách.

## 4. Thông số cơ học phải chốt bằng thử nghiệm (trên búp bê/hình nộm — không thử trên trẻ)
| Hạng mục | Cách đo | Tiêu chí |
|---|---|---|
| Lực bứt puck | Cân kéo (force gauge), kéo theo hướng ngã ra sau và hướng xuống | Bứt chắc chắn trước lực có thể lôi trẻ; không bứt oan khi chạy rung mạnh (test 10 phút chạy tốc độ tối đa, người lớn) |
| Chiều dài dây co giãn | Đánh dấu điểm ngã xa nhất còn an toàn; đo tổng quãng: ngã -> bứt -> băng dừng hẳn ở tốc độ tối đa | Tổng quãng < khoảng cách từ điểm ngã tới khe con lăn sau, trừ biên an toàn >= 30 cm |
| Độ trễ dừng từ lúc bứt | Quay chậm/đo GPIO->ENA bằng logic analyzer hoặc camera tốc độ cao | Nằm trong ngân sách P0-1 (motor bắt đầu dừng <= 300 ms; riêng đường A phần cứng phải ~tức thì) |
| Đứt dây / tuột giắc / gãy reed | Rút giắc, cắt dây thử ở trạng thái đang chạy (búp bê) | Mọi trường hợp đều dừng, không trường hợp nào giữ motor |
| Độ bền | Bứt/gắn lại 1.000 chu kỳ, đo lại lực bứt | Lực bứt không suy giảm quá 20% |
| Bứt oan | Chạy bình thường đủ các mức tốc độ, nhảy nhẹ, vung tay | 0 lần bứt oan trong buổi test |

## 5. Giới hạn đã biết
- Chỉ bảo vệ người **có đeo key đúng cách**; trẻ không chịu kẹp áo thì lớp này vô hiệu (vẫn còn ToF).
- Dây là vật lạ trên người chạy: dùng dây xoắn co giãn, điểm kẹp ở áo phải chắc, điểm bứt phải là đầu nam châm (không được rách áo trước khi bứt).
- Reed + nam châm phải tránh đặt sát motor DC (từ trường rò của motor có thể ảnh hưởng tiếp điểm ở khoảng cách rất gần) — dock đặt ở tay vịn/bảng điều khiển phía trước như khoá máy chạy truyền thống, xa motor.
