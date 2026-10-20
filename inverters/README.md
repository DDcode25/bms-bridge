# Инверторы 48V: как они говорят с BMS

Сводка для bms-bridge. Ссылки — [SOURCES.md](SOURCES.md), мануалы — [manuals/](manuals/),
спецификации — [protocols/](protocols/), распиновки — [../pinouts/](../pinouts/README.md).
Состояние на 2026-10-21.

| Инвертор | Физика к BMS | Протокол, который берём | Настройка на инверторе | Комментарий |
|---|---|---|---|---|
| **Deye / Sunsynk** SUN-xK-SG0xLP1 | RJ45 «BMS 485/CAN»: CAN 4/5, RS485 1/2 и 7/8 | **Pylon CAN** 500k | Batt Mode = Lithium, **Lithium Mode 00** | 00 = Pylon CAN. По форумам режим 004 (Victron) — запасной вариант |
| **Easun** ISolar SMG II 6.2KP | RJ45, только RS485 (1 B, 2 A, 7 A, 8 B) | **Pylon RS485** (Voltronic-клон) | Программа 05: «Li2» (по мануалу; варианты LI1–LI4) | CAN нет |
| **Anenji** ANJ-6200W-48V | RJ45, только RS485 (как у Easun); у 5.5 kVA — 5-pin | **Pylon RS485** (есть также PACE 232/485) | В меню типа батареи: PYL | CAN нет |
| **Voltronic Axpert** VM IV / MAX / KING | RJ45 BMS: RS232 1/2, RS485 3(B)/5(A), CAN 6/7, GND 8 | Pylon RS485 (9600) или нативный Voltronic BMS-485 | Тип батареи «LIb» (из списка), «LIC» — сторонняя | Нативный протокол — `protocols/Voltronic-Inverter-and-BMS-485-protocol-20201202.docx` |
| **Victron** MultiPlus-II 48V | BMS-порта нет: батарея → GX (Cerbo) VE.Can 7/8 | **Victron CAN-bus BMS** (≈ те же 0x351/355/356/35A/35E, что Pylon CAN) | GX: CAN profile «CAN-bus BMS 500 kbit/s»; DVCC | GX шлёт keep-alive. Официальная спека под NDA |
| Growatt SPF/ES (для справки) | RJ45 CAN 4/5 | Growatt CAN или Pylon | Пр. 05 = LI, пр. 36 = L51…L99 | |
| Must PV18 (для справки) | RJ45 | Pylon CAN | Пункт 41 = 8 | По форуму greenpowertalk |
| SMA Sunny Island (для справки) | CAN 4/5 | SMA CAN (0x351…0x35F) | Lithium-Ion, ёмкость | Требует непрерывного потока кадров |

## Вывод для bms-bridge

Чтобы покрыть все целевые инверторы, достаточно **двух выходов**:

1. **Pylon CAN (500k)** — Deye/Sunsynk, Victron (через GX), Growatt, Must, Luxpower,
   SMA. Набор кадров 0x351, 0x355, 0x356, 0x359, 0x35C, 0x35E (+ слушать 0x305 от
   инвертора). Это уже совпадает с решением в проекте.
2. **Pylon RS485 LV (V3.3/V3.5, 9600 8N1, ASCII-кадры)** — Easun, Anenji, Voltronic/Axpert.
   Ответы на опрос инвертора (команды 0x42 аналог. данные, 0x44 аварии,
   0x47 системные параметры, 0x92 параметры заряда/разряда и т. д.). Инвертор — мастер.

Разъём: один RJ45 «как у Deye» (CAN 4/5 + RS485 1/2 и 7/8) работает напрямую с
Deye, Easun, Anenji, Growatt; для Voltronic (RS485 3/5) и Victron (CAN 7/8) нужны
переходные кабели (см. [../pinouts/](../pinouts/README.md)).
