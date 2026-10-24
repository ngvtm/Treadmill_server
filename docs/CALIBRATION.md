# Quy trình calibrate — motor DS400 & cảm biến VL53L0X (GĐ3.2 + GĐ3.3)

Code đã sẵn sàng nhận số liệu: motor tra bảng `main/motor/motor_calibration.h`,
ngưỡng VL53 chỉnh qua menuconfig (branch `chore/safety-docs-kconfig`).
Tài liệu này là phần việc đo trên máy thật; **giá trị cuối chỉ chốt khi có số đo**,
không đoán.

## A. Thông tin motor cần có trước khi đo
1. Điện áp cấp thực tế cho DS400 (trong dải 6–24 VDC) — ghi lại số đo đồng hồ lúc chạy.
2. Dòng định mức và dòng khoá rotor của motor (nhãn motor/datasheet). Đối chiếu giới hạn L298N ~2 A/kênh và sụt áp ~2 V: nếu dòng khoá rotor vượt xa 2 A thì phải tính lại phương án driver/bảo vệ trước khi calibrate ở mức cao.

## B. Đo tốc độ băng theo mức (điền bảng này)
Chuẩn bị: đo chiều dài 1 vòng băng (mét), đánh dấu 1 điểm trắng trên băng. Mỗi mức: cho chạy ổn định 10 giây, đếm số vòng trong 60 giây (chụp/quay lại để đếm chắc), làm 2 điều kiện: không tải và có người chạy (~cân nặng người dùng chính). Tốc độ = vòng/phút × chiều dài vòng × 0,06 = km/h.

| Mức | Vòng/60s (không tải) | km/h | Vòng/60s (có người) | km/h | Ghi chú (rung/trượt) |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |
| 4 | | | | | |
| 5 | | | | | |
| 6 | | | | | |
| 7 | | | | | |
| 8 | | | | | |
| 9 | | | | | |
| 10 | | | | | |

Cách chỉnh bảng sau khi có số: mục tiêu các mức cách đều nhau theo km/h (ví dụ mỗi mức +1 km/h). Mức nào thừa tốc độ thì giảm duty của mức đó trong `MOTOR_DUTY_TABLE` (ước lượng tỉ lệ duty ≈ tỉ lệ tốc độ ở cùng tải), thiếu thì tăng; giữ bảng đơn điệu tăng; đo lại vòng 2 để xác nhận. Gửi bảng đã điền cho Muse, Muse tính duty mới + cập nhật bảng + chạy host test đơn điệu.

## C. Chốt ngưỡng VL53L0X (GĐ3.3)
- Để `IR_HEALTH_PERIOD_MS = 200` trong lúc bench (xem khoảng cách thô theo thời gian thực qua serial log).
- Ghi khoảng cách từng cảm biến ở các tư thế: đứng giữa băng, lệch trái/phải sát mép, chân trước/chân sau, trẻ em đứng cạnh vùng DANGER, người đi ngang phía sau máy.
- Gửi log/bảng khoảng cách cho Muse: chọn `VL53_DETECT_MAX_MM` (mặc định 700) sao cho phân biệt chắc chắn "trong vùng" vs "ngoài vùng" ở mọi tư thế, và chốt `IR_HEALTH_PERIOD_MS` bản dùng thật (hướng tới ~2000 ms khi không debug).
- Hai giá trị này vào menuconfig (menu "Treadmill Configuration"), không sửa code.
