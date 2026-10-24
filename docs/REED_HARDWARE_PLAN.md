# Kế hoạch làm phần cứng khoá reed — từ đồ đang có: 1 reed + 1 nam châm

Ngày viết: 2026-10-24. Bản thiết kế nguyên lý nằm ở
`REED_SAFETY_KEY_DESIGN.md` (fail-safe, 2 đường A/B); tài liệu này
là kế hoạch **làm** — theo thứ tự, mỗi bước có cách kiểm tra và
tiêu chí đạt, và nói rõ bước nào làm được ngay với đồ đang có,
bước nào cần mua thêm.

## 0. Xác định đúng thứ đang có (làm trước, 15 phút, không cần board)

Reed switch có nhiều loại; lắp sai loại là mất fail-safe. Dùng
đồng hồ vạn năng ở thang đo thông mạch + nam châm đang có:

- [ ] **NO hay NC?** Để nam châm xa → đo 2 chân reed: không thông
      = NO (thường mở, cần cho thiết kế này); thông = NC (không
      dùng được trực tiếp — cần đổi loại hoặc đảo logic, báo lại).
- [ ] **Khoảng kích hoạt:** để nam châm lại gần từ từ, ghi khoảng
      cách (mm) lúc vừa thông mạch, rồi lúc vừa ngắt khi kéo ra.
      Con số này quyết định độ sâu hốc dock ở bước 2 (xem dưới).
- [ ] **Dạng vỏ:** ống thuỷ tinh trần hay module đóng gói sẵn có
      dây? Ống trần rất dễ vỡ — bắt buộc có vỏ bảo vệ trước khi
      gắn lên máy (bước 1). Module sẵn dây thì bỏ qua vỏ reed.
- [ ] **SPST hay SPDT?** Đếm chân: 2 chân = SPST (1 tiếp điểm);
      3 chân = SPDT (2 tiếp điểm: 1 thường mở + 1 thường đóng).

Kết quả bước 0 quyết định đi nhánh nào ở bước 3. Ghi lại 4 dòng
trên gửi cho em nếu có gì khác với giả định (NO, SPST).

## 1. BOM — cần mua/thu xếp thêm (ngoài reed + nam châm)

| Vật tư | Spec gợi ý | Dùng cho |
|---|---|---|
| Kẹp áo | Kẹp bảng tên / kẹp treo rèm loại kẹp chắc, không răng sắc | Giữ puck trên áo; điểm bứt phải là dock, không phải kẹp tuột khỏi áo |
| Dây xoắn co giãn | Dây xoắn kiểu dây điện thoại, dài theo bước 2 | Nối kẹp áo ↔ puck; xoắn để khỏi vướng chân |
| Vỏ puck | Hộp nhựa nhỏ / in 3D / ống co nhiệt + keo epoxy (bản thử) | Giữ nam châm, chịu được giật bứt lặp lại |
| Vỏ dock | Hộp nhựa nhỏ có nắp, bắt vít được | Giữ reed cố định trên máy, chống va đập |
| Dây nối dock → board | Cáp 2 lõi xoắn (đường B), dài đủ từ dock tới board | Tín hiệu GPIO23 + GND; đi xa motor, tránh bó chung dây nguồn motor |
| Tụ gốm 100 nF | 1 cái | Chống nhiễu ở GPIO23 (mắc GPIO23 → GND, đầu board) |
| Giắc nối | JST-XH 2 chân hoặc domino vặn vít | Ngắt dock để bảo trì; tuột giắc = hở mạch = dừng (đúng fail-safe) |
| Điện trở 10 kΩ | 1 cái (chỉ cần ở giai đoạn 2) | Pull-down chân ENA |
| Phụ | Co nhiệt, dây rút, vít + keo 2 mặt, cân kéo hành lý (đo lực bứt) | |

Khuyến nghị mua thêm (rẻ, quyết định giai đoạn 2): **1 reed NO
nữa** (cùng loại) hoặc đổi sang **1 reed SPDT**. Lý do ở bước 3.

## 2. Puck + dock + dây (làm trước, chưa cần board)

**Puck (phía người):**
- [ ] Gắn nam châm vào vỏ puck sao cho mặt nam châm hướng ra
      ngoài đúng 1 mặt (mặt áp vào dock). Dán cố định, không để
      nam châm xoay được trong vỏ.
- [ ] Nối dây xoắn: một đầu vào puck, một đầu vào kẹp áo.
- [ ] Chiều dài dây: đo khoảng cách từ vị trí dock (bước dock)
      tới người đứng giữa băng + **15 cm chùng**. Dây quá dài =
      ngã xa mới bứt; dây quá ngắn = bứt oan khi chạy. Chốt số
      cuối bằng test búp bê ở bước 5.

**Dock (phía máy):**
- [ ] Vị trí: tay vịn / bảng điều khiển phía trước, kiểu khoá máy
      chạy truyền thống — **xa motor DC** (từ trường rò của motor
      có thể ảnh hưởng reed ở khoảng rất gần).
- [ ] Gắn reed vào vỏ dock, mặt reed hướng ra ngoài đúng hướng
      puck áp vào. Hốc đặt puck sâu sao cho khi puck ngồi đúng vị
      trí, khoảng cách nam châm–reed **≤ 50% khoảng kích hoạt**
      đo ở bước 0 (biên chống rung).
- [ ] Puck phải ngồi chắc khi chạy rung nhưng bứt ra bằng một cái
      giật nhẹ theo hướng ngã ra sau — test bằng tay trước khi
      nối điện: giật 20 lần, 20/20 lần bứt sạch, không kẹt, không
      làm rơi vỏ dock.

## 3. Đấu dây — 2 giai đoạn (quan trọng: đừng đi tắt)

### Thiết kế dây nối dock → ESP32 (cụ thể — giả định dây dài 30 cm)

Phân biệt 2 đoạn dây khác nhau, đừng lẫn: **(a)** dây cố định từ dock trên thanh trước về hộp ESP32
(đoạn này mới là câu hỏi "dùng dây gì", giả định dài 30 cm); **(b)** dây xoắn co giãn từ puck tới kẹp áo
(đoạn này chỉ chịu lực cơ học khi bứt, không mang điện — xem mục 2).

**Đường B — firmware (làm ngay): cặp xoắn 24AWG lõi mềm (stranded), 1 sợi GPIO23 + 1 sợi GND.**
Vì sao loại này, tính ra sao ở 30 cm:
- Tín hiệu là mức logic DC, dòng qua pull-up nội ESP32 (~45 kΩ) chỉ ~73 µA. Sụt áp trên dây:
  24AWG = 84 mΩ/m, đi+về 0,6 m = 0,05 Ω → 73 µA × 0,05 Ω ≈ **4 µV — không đáng kể**.
- Điện dung cặp xoắn ~50–100 pF/m × 0,3 m = 15–30 pF — so với tụ 100 nF đã mắc tại board thì
  không ảnh hưởng gì (tín hiệu DC, không có timing).
- Nhiễu mới là thứ cần lo, không phải sụt áp: xoắn đôi triệt nhiễu cảm ứng từ PWM motor, tụ 100 nF
  tại đầu board lọc phần còn lại, và dây đi nẹp riêng, tách khỏi dây nguồn motor.
- Về điện, gần như dây tín hiệu nào cũng chạy được ở 30 cm; chọn 24AWG lõi mềm vì bền cơ học,
  dễ bấm giắc, không quá to làm nặng dock. Giắc: **JST-XH 2,54 mm 2 chân ở đầu hộp** (tháo được khi
  bảo trì); đầu dock hàn + co nhiệt chống rung. Nếu đoạn dây phải đi sát motor (dưới ~10 cm):
  dùng cặp xoắn có shield, shield nối GND **chỉ ở đầu hộp** (không nối 2 đầu — tránh vòng mass).

**Đường A — cắt cứng (giai đoạn 2): cặp xoắn 24AWG riêng**, cùng cách tính: đoạn này gồm
ESP32 → dock rồi dock → cổng AND → ENA; tín hiệu là PWM 5 kHz vào đầu vào logic (dòng ~µA),
tiếp điểm reed ~100–200 mΩ nối tiếp → sụt áp cỡ µV — không vấn đề. Pull-down 10 kΩ mắc ở chân ENA
(đầu L298N), không mắc ở dock.

**Dài hơn 30 cm thì sao:** cùng loại dây này chạy tốt tới ~1 m cho đường B (sụt áp vẫn cỡ µV,
điện dung ~100 pF vẫn << 100 nF). Quá 1 m về điện vẫn chạy nhưng phải kiểm tra nhiễu thực tế
(rung máy + PWM motor) bằng test bứt 100 lần không false trip.

**Không dùng:** dây đơn không kèm GND (thành anten hút nhiễu motor); dây lõi cứng ở chỗ rung/gập
(gãy ngầm khó tìm); chung GND với dây nguồn motor (ground bounce → false trip); dây quá to
(20AWG trở xuống: nặng, khó đi, không cần thiết vì dòng chỉ cỡ µA–mA).

### Vật tư mua cho đoạn dây này (đủ cho dock → hộp 30 cm + dự phòng)

- **Dây silicone 24AWG lõi mềm: 1 m đỏ + 1 m đen** (xoắn 2 sợi lại thành cặp; 30 cm chạy thực tế,
  còn lại để bấm giắc, đi dây và dự phòng). Rẻ hơn: dây silicone 24AWG bán theo mét, mua 1 m mỗi màu.
- **Ống silicon bọc ngoài đoạn dây: 1 m, đường kính trong 5 mm** (luồn cặp xoắn vào trong, chống mài
  mòn + rung; đầu ống cố định bằng ống co nhiệt Ø6 mm hoặc dây rút). Từ khoá tìm mua: "ống silicon
  bọc dây", "ống cao su silicon ID 5mm".
- **Ống co nhiệt cho mối nối: hộp đủ cỡ 1,5–10 mm** (dùng Ø3 mm bọc mối hàn ở dock, Ø5–6 mm bọc
  đầu ống silicon và đuôi giắc JST). Hơ bằng bật lửa hoặc máy sấy tóc ở nấc nóng, hơ đều tay.
- **Giắc JST-XH 2,54 mm 2 chân: 2 bộ** (housing + chân bấm) cho đầu hộp; nếu không có kìm bấm JST
  thì hàn dây trực tiếp vào chân giắc rồi bọc co nhiệt — chấp nhận được cho 1 bộ duy nhất.
- Lưu ý mua: dây silicone thật có vỏ mềm như cao su, bẻ không gãy, chịu nhiệt hàn tốt; tránh dây
  PVC cứng (vỏ bóng, bẻ gập kêu rắc) vì rung máy lâu ngày nứt vỏ.

### Giai đoạn 1 — làm ngay với 1 reed đang có (không sửa firmware)

Dùng tiếp điểm duy nhất cho **đường B (firmware)**: reed nối
`GPIO23 → GND`, tụ 100 nF mắc `GPIO23 → GND` ở đầu board, giắc
nối ở giữa để tháo được. Firmware hiện tại đọc đúng kiểu này
(pull-up nội, đóng = kéo xuống GND, xác nhận có key 100 ms, mất
key 30 ms → hard cutoff + alert SAFETY_KEY_OUT + latch).

Giai đoạn 1 đã cho: ON bị từ chối khi không có puck, rút puck khi
đang chạy → hard cutoff qua firmware + báo smartkey + latch tới
khi bấm ON chủ động lúc puck đã gắn lại. Chưa có: cắt cứng khi
ESP32 treo (đường A) — đó là giai đoạn 2.

### Giai đoạn 2 — khi có thêm 1 tiếp điểm (reed thứ 2 hoặc SPDT)

Thêm **đường A (cắt cứng)**: tiếp điểm thứ 2 mắc nối tiếp trên
đường `ESP32 GPIO5 (PWM) → chân ENA của L298N`, thêm pull-down
10 kΩ từ chân ENA xuống GND. Hai tiếp điểm cùng đóng/mở bởi 1
nam châm trong puck (đặt 2 reed sát nhau trong dock). Từ lúc này,
rút puck → ENA rớt xuống 0 ngay cả khi ESP32 treo.

**Không nên đi tắt:** dùng 1 tiếp điểm chung cho cả 2 đường rồi
nối GPIO23 vào nút giữa reed–ENA. Nút đó mang tín hiệu PWM 5 kHz
khi reed đóng, không phải mức logic ổn định — firmware poll 5 ms
sẽ đọc lúc cao lúc thấp, latch hoạt động không đáng tin. Tiết
kiệm 1 reed không đáng để đổi lấy cảm biến chập chờn.

## 4. Kiểm tra trên bàn (chưa chạy băng)

- [ ] Dock puck → serial log phải có `safety key docked`.
- [ ] Rút puck → log `SAFETY KEY PULLED` + smartkey hiện
      "KHOA AN TOAN / DA BI BUT RA" + điều khiển bị khoá.
- [ ] Không puck → bấm ON (giữ nút 1,5 s) → bị từ chối, alert đúng
      SAFETY_KEY_OUT (không phải CMD_REJECTED chung).
- [ ] Gắn lại puck rồi bấm ON → chạy được (ON này chính là hành
      động xác nhận xoá latch).
- [ ] Tuột giắc dock → coi như mất puck: ON bị từ chối, đang chạy
      thì dừng (fail-safe của giắc).
- [ ] Giai đoạn 2: đo điện áp chân ENA khi rút puck → phải về 0
      ngay cả khi không rút nguồn board.

## 5. Kiểm tra trên băng (búp bê/hình nộm trước, người sau)

- [ ] Búp bê đứng giữa băng, tốc độ thấp: kéo dây giật puck →
      băng dừng; ghi thời gian từ lúc bứt tới lúc băng dừng hẳn.
- [ ] Đo lực bứt bằng cân kéo, kéo theo hướng ngã ra sau và
      hướng xuống; đo quãng đường ngã → bứt → dừng hẳn ở tốc độ
      tối đa. Tiêu chí (từ bản thiết kế): tổng quãng < khoảng cách
      tới khe con lăn sau trừ biên ≥ 30 cm.
- [ ] Chạy búp bê đủ các mức tốc, rung mạnh, vung tay: **0 lần
      bứt oan** trong buổi test.
- [ ] Đứt dây / tuột giắc / gãy reed giả lập khi đang chạy (búp
      bê): mọi trường hợp đều dừng.
- [ ] Độ bền: bứt/gắn 1.000 chu kỳ (làm dần bằng tay hoặc jig đơn
      giản), lực bứt không giảm quá 20%.
- [ ] Chỉ sau khi tất cả trên đạt, người lớn tự test ở tốc độ thấp
      nhất, đứng trên thanh ray 2 bên, tay vịn chắc, giật puck chủ
      động — **không test ngã thật trên người, không test trên trẻ**.

## 6. Gửi lại cho em sau khi làm

1. Kết quả bước 0 (NO/NC, khoảng kích hoạt, dạng vỏ, SPST/SPDT).
2. Ảnh puck + dock + vị trí gắn trên máy.
3. Chiều dài dây cuối, lực bứt đo được, thời gian/quãng dừng.
4. Serial log các bước ở mục 4 (đặc biệt dòng `safety key docked`
   và `SAFETY KEY PULLED`).
5. Bất kỳ lần bứt oan nào (tốc độ, tư thế, lực ước lượng).
