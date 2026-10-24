# GĐ2 — Motor feedback: encoder + INA219 + ENA interlock

Thiết kế + đấu dây cho phần cứng an toàn motor (P0-3). Code đã có
trong `main/motor_feedback/` (logic thuần `motor_feedback_logic.h`,
glue `motor_feedback.c`); tài liệu này là phần còn lại: dây, mạch
ngoài, và những số phải đo.

## 1. Encoder (tốc độ + chiều băng)

- Loại: encoder xung 2 kênh A/B gắn vào trục roller hoặc bánh xe đo
  băng (không gắn vào trục motor nếu có hộp giảm tốc — số xung/vòng
  phải tính theo vòng **băng**, xem mục 4).
- Dây:
  - A → GPIO4, B → GPIO33 (menuconfig "Treadmill Motor Feedback").
    Hai chân này không phải strapping, có pull-up nội — firmware đã
    bật pull-up, encoder open-collector dùng được trực tiếp.
  - GND encoder → GND board. Nguồn encoder (5 V hay 3.3 V) theo
    datasheet encoder; **không** đưa 5 V vào chân GPIO.
- PCNT đếm sườn lên của A; mức của B chọn chiều đếm (thấp = thuận,
  cao = ngược). Bộ lọc nhiễu 1000 chu kỳ APB (~12.5 µs) loại bỏ
  rung tiếp điểm.
- Hỏng encoder thì sao: firmware **không** đoán mò. Mất PCNT →
  stall/no-motion tạm ngưng, chỉ còn bảo vệ quá dòng, log cảnh báo
  mỗi 10 s ("motor feedback DEGRADED"). Mất INA219 thì ngược lại:
  dòng coi như đang kẹt (fail-closed) nên băng không chạy mà encoder
  im sẽ trip STALL.

## 2. INA219 (dòng motor)

- Module INA219 mắc **nối tiếp** đường nguồn motor (thường là dây
  +24 V vào L298N, phía trước driver), shunt 0.1 Ω mặc định.
- Cùng bus I2C0 với VL53 (SDA GPIO21 / SCL GPIO22, 100 kHz),
  địa chỉ mặc định 0x40 (A0/A1 chọn 0x40–0x43).
- Tầm đo mặc định ±3.2 A (shunt 0.1 Ω, LSB 100 µA, Cal = 4096).
  **Phải đo dòng kẹt DS400 trước khi chốt:** nếu dòng kẹt > 3.2 A
  thì thay shunt nhỏ hơn (ví dụ 0.05 Ω → ±6.4 A) và cập nhật cả
  shunt lẫn LSB trong menuconfig — công thức Cal =
  40960000 / (shunt_mΩ × lsb_µA).
- Ngưỡng mặc định (PLACEHOLDER, xem mục 4): quá dòng 2.5 A / 500 ms,
  kẹt 500 mA / 2 s, không chuyển động 100 mA / 2 s, cooldown sau
  trip 10 s.

## 3. Interlock ENA: pull-down + heartbeat (phần cứng)

Hai lớp, đều nằm ngoài firmware:

### 3a. Pull-down ENA (bắt buộc, 1 điện trở)

- 10 kΩ từ chân ENA của L298N xuống GND.
- Tác dụng: khi ESP32 reset, chưa chạy code, hoặc dây PWM tuột,
  ENA không trôi nổi ở mức cao → motor không tự chạy. Cùng mẫu
  với đường reed đã làm (pull-down phía ENA).
- Lưu ý: pull-down này **không** chống được firmware treo mà vẫn
  xuất PWM (LEDC giữ duty cũ) — đó là việc của heartbeat.

### 3b. Heartbeat (khuyến nghị, 1 IC + 1 cổng AND)

- ESP32 xuất sóng vuông 10 Hz (chu kỳ 100 ms) ra GPIO12.
  **GPIO12 là chân strapping (MTDI, chọn điện áp flash):** mạch
  interlock không được kéo chân này lên lúc boot, và bắt buộc có
  pull-down 10 kΩ để boot đọc mức thấp (flash 3.3 V). Sau boot,
  ESP32 lái chân này bình thường.
- Mạch ngoài (mẫu dùng 74HC123 retriggerable monostable):
  - Heartbeat → ngõ vào retrigger của 74HC123, timeout ~250 ms
    (R/C chọn sao cho > 2.5 chu kỳ heartbeat).
  - Ngõ ra Q của 74HC123 → 1 ngõ vào cổng AND (ví dụ 74HC08);
    ngõ còn lại là PWM từ ESP32 GPIO5 (đã qua tiếp điểm reed).
  - Ngõ ra AND → ENA của L298N.
  - Còn heartbeat thì Q = cao, PWM đi qua bình thường. Firmware
    treo / task heartbeat chết / ESP32 reset → sau ~250 ms Q rớt
    → ENA thấp → motor dừng, không cần phần mềm.
- Phương án thay thế: TPL5010 (watchdog timer chuyên dụng, DONE
  phải được xung trước timeout nếu không WAKE tác động) — đắt hơn
  nhưng chính xác hơn 74HC123. Nguyên tắc giống nhau: mất heartbeat
  → cắt ENA bằng phần cứng.
- Giới hạn nói thẳng: heartbeat chỉ cắt khi **firmware ngừng
  toggle**. Nếu firmware chạy sai logic nhưng vẫn toggle đều
  (ví dụ kẹt trong vòng lặp vẫn gọi toggle), heartbeat không cứu
  được — đó là việc của watchdog task + các latch lỗi phần mềm.

## 4. Những số phải đo (user cập nhật sau)

| Hạng mục | Cách đo | Cập nhật vào |
|---|---|---|
| Dòng chạy không tải / có người / kẹt DS400 | Kẹp dòng DC ở dây +24 V vào L298N, từng mức tốc 1–10 | Ngưỡng quá dòng, kẹt |
| Xung/vòng encoder | Quay roller 1 vòng, đếm xung A | `TREADMILL_ENCODER_PULSES_PER_REV` |
| Chu vi vòng băng (mm) | Đo dây băng quanh 1 vòng | `TREADMILL_BELT_LOOP_MM` |
| Tốc độ băng từng mức (mm/s) | Đánh dấu băng, bấm giờ 60 s | Bảng tốc độ (cho check mismatch tương lai) |
| Lực kéo heartbeat timeout | Rút nguồn ESP32 khi đang chạy, đo thời gian tới lúc băng dừng hẳn | Xác nhận ≤ ~250 ms + quán tính băng |

Checklist flash: (1) chạy băng, xem log `MOTOR_FB` — belt mm/s phải
tăng theo tốc độ, current mA > 0; (2) chặn băng đứng lại bằng vật
nặng (không dùng tay trần khi băng đang chạy) → sau 2 s phải thấy
`MOTOR FAULT kind=STALL` + hard cutoff;
(3) rút dây encoder A khi đang chạy → sau 2 s STALL (fail-closed);
(4) rút nguồn board khi đang chạy → ENA rớt trong ~250 ms (kiểm tra
bằng đồng hồ hoặc LED mắc ở ENA).
