# Распиновки разъёмов связи: инверторы и батареи

Все разъёмы — RJ45 (8P8C), если не сказано иное. Нумерация контактов —
стандартная: защёлка вниз, контакты к себе, слева направо 1…8 (см. картинки).
Источник каждой строки указан; картинки — страницы из PDF в этой папке.
Состояние на 2026-10-21.

> ⚠ «A/B» в RS485 разные производители называют наоборот. Если связи нет —
> первым делом поменять A и B местами.
> ⚠ GND в CAN: Victron рекомендует не подключать (петли земли); некоторым
> изолированным трансиверам он нужен.

## Сводка: какой контакт куда

| Устройство | Порт | CAN-H | CAN-L | RS485-A | RS485-B | GND | Прочее |
|---|---|---|---|---|---|---|---|
| **Deye / Sunsynk** (SUN-xK-SG0xLP1) | BMS 485/CAN | 4 | 5 | 2, 7 | 1, 8 | 3, 6 | — |
| Deye / Sunsynk | RS485/METER (счётчик, не BMS!) | — | — | 5 | 4 | — | — |
| **Easun ISolar SMG II 6.2KP** | BMS (RS485) | — | — | 2, 7 | 1, 8 | — | только RS485 |
| **Anenji ANJ-6200W-48V** (6.2 kVA) | BMS (RS485) | — | — | 2, 7 | 1, 8 | — | только RS485 |
| Anenji 5.5 kVA | BMS, 5-pin (USB-B форм-фактор) | — | — | 2 (бел.) | 1 (красн.) | 3, 4 | 5 NC |
| **Voltronic Axpert VM IV** (и клоны) | BMS | 6 | 7 | 5 | 3 | 8 | 1 RS232 TX, 2 RS232 RX |
| Voltronic Axpert VMIII / KING | BMS (RS485) | — | — | 5 | 3 | — | |
| **Victron** GX (Cerbo, VE.Can / BMS-Can) | VE.Can | 7 | 8 | — | — | 3 | нужен терминатор 120 Ом |
| Growatt (SPF / ES) | BMS | 4 | 5 | — | — | — | RS485 по данным форума 1/2 (не проверено) |
| Luxpower | BMS | 4 | 3 | — | — | — | SNA-серия: 4/5 как Deye |
| SRNE | BMS (RS485) | — | — | 7 | 8 | — | |
| SMA (Sunny Island, VOLTA-гайд) | BMS | 4 | 5 | — | — | — | |
| SMA Sunny Tripower SE | клеммник | клемма 1 | клемма 3 | — | — | клемма 4 | 2 Enable |

| Батарея | Порт | CAN-H | CAN-L | RS485-A | RS485-B | GND | Прочее |
|---|---|---|---|---|---|---|---|
| **Pylontech US2000/US3000** | CAN | 4 | 5 | — | — | 2 (Victron Type B) | |
| Pylontech US2000/US3000 | RS485 | — | — | 7 | 8 | 6 | |
| Pylontech US2000C/US3000C/UP5000/Force-L | CAN | 4 | 5 | — | — | 6 (Victron Type A) | |
| «Типичная» Pylon-совместимая батарея (Sunsynk) | CAN | 1 | 5 | — | — | — | ⚠ так в документе |
| то же | RS485 | — | — | 2, 8 | 1, 7 | 3, 6 | |
| VOLTA (пример китайской LFP), порт A | RS485 | — | — | 2, 5 | 1, 4 | 3, 6 | |
| VOLTA, порт B | CAN | 4, 8 | 1, 5 | — | — | 2, 7 | |
| **BMZ ESS** (7.0/9.0/X) | CAN к инвертору | 4 | 5 | — | — | 2 | Victron Type B; RS485 «INTERNAL» между батареями |
| **BMZ Hyperion** (BMU, к SMA STP SE) | RJ45 | 1 | 2 | — | — | 5 | 6 Enable; 12 V питание по RJ45 в цепочке модулей |
| BMZ Hyperion BMS, сервисный DB9 | DB9 | 2 | 7 | — | — | — | 120 Ом, кабель 1:1 |
| **BMZ 618031** (наш модуль) | X1 / X2 | ? | ? | ? | ? | ? | **не известно** — прозвонить плату 216-003-02 |

### Гипотеза для BMZ 618031
Victron относит «BMZ» (вообще) к кабелю Type B → у батареи CAN-H 4, CAN-L 5,
GND 2. Но у Hyperion другая схема (H 1, L 2, GND 5, Enable 6, +12 V в цепочке).
Пока не прозвоним — подключать к трансиверу через резисторы/осторожно, на
RJ45 может быть **+12 V** питания логики.

## Готовые кабели (переходники)

| Откуда → куда | Контакты |
|---|---|
| Pylontech CAN → Deye/Sunsynk | 4→4 (H), 5→5 (L) — прямой |
| Pylontech CAN → Victron (Type B) | бат. 4→7 (H), 5→8 (L), 2→3 (GND) |
| Pylontech C-серия → Victron (Type A) | бат. 4→7, 5→8, 6→3 |
| Pylontech RS485 → Axpert VMIII/KING | бат. 7 (A)→5, 8 (B)→3 |
| Батарея RS485 (1B/2A) → Axpert/Voltronic | 1→3 (B), 2→5 (A) |
| Батарея RS485 (1B/2A) → SRNE | 1→8 (B), 2→7 (A) |
| Батарея RS485 → Easun/Anenji 6.2 kVA | «pin to pin», 1–2 / 7–8 |
| Батарея CAN 4/5 → Luxpower | 4→4 (H), 5→3 (L) |
| Motoma → Deye | 485A 2→2, 485B 1→1, CAN-L 5→5, CAN-H 4→4 |

## Что это значит для bms-bridge (выход «как Pylontech»)

- Один RJ45 «как у Deye» (1 B, 2 A, 3 GND, 4 H, 5 L, 6 GND, 7 A, 8 B) покрывает
  Deye/Sunsynk (CAN), Easun/Anenji 6.2 kVA (RS485 на 1/2 и 7/8), SRNE (7/8),
  Growatt (CAN 4/5). Для Voltronic и Victron нужны переходные кабели.
- Easun и Anenji — **только RS485** (протокол Pylon RS485, меню 5 = «Li2»/PYL).
  Deye, Victron, Growatt, Luxpower, SMA — CAN.

## Картинки

| Файл | Что |
|---|---|
| [deye-sg03lp1-p53.png](deye-sg03lp1-p53.png) | Deye SG03LP1: BMS 485/CAN, таблица + фото платы J1 |
| [deye-sg03lp1-p52.png](deye-sg03lp1-p52.png) | Deye SG03LP1: RS485/METER (не путать с BMS) |
| [deye-volton-guide-p02.png](deye-volton-guide-p02.png) | Deye + батареи в параллели: схема кабелей, DIP |
| [deye-motoma-guide-p10.png](deye-motoma-guide-p10.png) | Motoma → Deye: кабель |
| [easun-smg2-6.2kp-p35.png](easun-smg2-6.2kp-p35.png) | Easun SMG II 6.2KP: BMS RS485 |
| [anenji-6200-48PL-p38.png](anenji-6200-48PL-p38.png) | Anenji 5.5/6.2 kVA: BMS RS485 (оба варианта) |
| [anenji-6200W-48V-p22.png](anenji-6200W-48V-p22.png) | Anenji ANJ-6200W-48V: BMS RS485 |
| [axpert-vm4-p53.png](axpert-vm4-p53.png) | Voltronic Axpert VM IV: BMS (RS232/RS485/CAN) |
| [axpert-king-p03.png](axpert-king-p03.png) | Pylontech US2000 ↔ Axpert VMIII/KING: обе стороны |
| [sunsynk-compat-p04.png](sunsynk-compat-p04.png) | Sunsynk: сторона батареи и сторона инвертора |
| [volta-multi-p03.png](volta-multi-p03.png) | VOLTA: порты батареи RS485 (A) и CAN (B) |
| [volta-multi-p08.png](volta-multi-p08.png) | → Deye/Sunsynk (CAN) |
| [volta-multi-p09.png](volta-multi-p09.png) | → Luxpower (CAN) |
| [volta-multi-p10.png](volta-multi-p10.png) | → Luxpower SNA (CAN) |
| [volta-multi-p11.png](volta-multi-p11.png) | → Voltronic / Axpert (RS485) |
| [volta-multi-p12.png](volta-multi-p12.png) | → SMA (CAN) |
| [volta-multi-p13.png](volta-multi-p13.png) | → Victron (CAN) |
| [volta-multi-p14.png](volta-multi-p14.png) | → SRNE (RS485) |
| [volta-multi-p15.png](volta-multi-p15.png) | → Growatt (CAN) |
| [volta-multi-p16.png](volta-multi-p16.png) | → TBB (CAN) |
| [volta-multi-p17.png](volta-multi-p17.png) | → SAKO (CAN) |
| [bmz-ess-p42.png](bmz-ess-p42.png) | BMZ ESS: RS485-шина между батареями, Y-адаптер RJ45 |
| [../bmz/photos/03-bms-board-216-003-02.jpg](../bmz/photos/03-bms-board-216-003-02.jpg) | BMZ 618031: плата с X1/X2 (распиновка неизвестна) |

## Источники
- Мануалы и гайды — в [../inverters/](../inverters/) (список: [../inverters/SOURCES.md](../inverters/SOURCES.md))
- Victron Type A/B: https://www.victronenergy.com/live/battery_compatibility:can-bus_bms-cable
- Victron VE.Can pinout (форум): https://community.victronenergy.com/t/ve-can-pinout-and-termiators/49517
- SMA STP SE ↔ BMZ Hyperion: https://manuals.sma.de/STPxx3SE40-Batteries/de-DE/13292500107.html
- BMZ Hyperion DB9 сервис: https://www.photovoltaikforum.com/thread/257596-bmz-hyperion-axistorage-li-sh-bms-auslesen/
