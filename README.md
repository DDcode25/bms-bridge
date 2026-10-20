# bms-bridge

Прошивка ESP32 (ESP-IDF, C), которая читает BMS модуля **BMZ 618031** и отвечает
инвертору как батарея **Pylontech RS485**. Цель — Easun ISolar SMH III 6.2kW, но
протокол подходит почти любому низковольтному гибридному инвертору.

- ТЗ: [docs/TZ.md](docs/TZ.md) (копия; оригинал — в Claude Docs, ссылка в файле)
- Исследование модуля: [bmz/](bmz/); инверторы и спецификации протоколов —
  [inverters/](inverters/README.md), распиновки — [pinouts/](pinouts/README.md)
- Сниффер шины BMZ для этапа 1: [tools/bmz_can_sniffer/](tools/bmz_can_sniffer/)

## Состояние

| Этап ТЗ | Что есть |
|---|---|
| 1. Разведка батареи | сниффер `tools/bmz_can_sniffer`; логов пока нет |
| 2. Инвертор отдельно | **готово к стенду**: симулятор + ответчик Pylontech RS485, лог запрошенных CID2 |
| 3. Протокол BMZ | разбор CAN по гипотезе А (0x351…0x379, как Victron / Pylon CAN) — ждёт логов |
| 4–5 | ещё нет |

Не сделано: веб-страница, MQTT / Home Assistant, OTA, настройки в NVS, журнал
событий, светодиоды (ТЗ 6.1, 6.3, F6–F9); выход Pylontech CAN; SOC по
кулон-счётчику (F5); гипотеза «CiA 454 как у Hyperion» из [bmz/README.md](bmz/README.md).

## Структура

```
components/
  pack_model/   сводка по модулям (pack_aggregate) + хранилище с мьютексом
  limits/       CCL/DCL и флаги по таблице 5.2 ТЗ
  pylon_rs485/  кадр Pylontech (pylon_frame), ответы (pylon_cmd), UART-задача
  bmz_driver/   источник данных: симулятор или CAN (гипотеза А, bmz_can)
main/           запуск задач, консоль, сводка в лог раз в 10 с
test/           unit-тесты (Unity)
tools/          сниффер BMZ
```

Чистая логика (кадры, лимиты, сводка, разбор CAN) не зависит от ESP-IDF и
покрыта тестами; железо — только в `pylon_rs485.c`, `bmz_driver.c`, `pack_store.c`.

## Сборка

ESP-IDF v6.0 (проверено на v6.0.2). Драйвер TWAI — legacy `driver/twai.h`,
как в сниффере.

```
. ~/.espressif/tools/activate_idf_v6.0.2.sh
idf.py set-target esp32        # или esp32s3
idf.py menuconfig              # разделы «BMZ», «Pylontech RS485», «Лимиты»
idf.py build flash monitor
```

По умолчанию источник — **симулятор**: шлюз отвечает инвертору без батареи.

### Тесты

```
test/host_test.sh              # gcc + Unity из ESP-IDF, с ASan/UBSan
```

Эталоны — кадры из PYLON RS485 V3.5 (CHKSUM, LENGTH, ответы 0x61 и 0x62 байт в
байт) и пример запроса 0x42 из ТЗ. Проект `test/` собирается и как
`idf.py --preview set-target linux`, но для этого на хосте нужен `libbsd-dev`.

## Консоль (стенд, ТЗ 7.1)

| Команда | Зачем |
|---|---|
| `status` | батарея, CCL/DCL, причины снижения, счётчики опроса инвертора |
| `cid2` | какие команды запрашивал инвертор — список для этапа 2 |
| `sim cell 8 3560` | T6: ячейка 3,56 В → CCL = 0 |
| `sim temp 0` | T7: 0 °C → CCL = 0 |
| `sim pause` / `sim resume` | T4: через 10 с CCL = DCL = 0, через 60 с шлюз молчит |
| `sim module 2 off` | T9: модуль выпал → CCL/DCL вдвое (при `BMZ_MODULES=2`) |
| `sim current -20`, `sim soc 4` | ток, SOC (ниже 5 % — «charge immediately») |

## Pylontech RS485: что и откуда

Форматы — по оригиналам [PYLON RS485 LV V3.3](inverters/protocols/Pylon-RS485-LV-V3.3-20180821.pdf)
(0x42, 0x44, 0x47, 0x4F, 0x51, 0x92, 0x93, 0x96) и [V3.5](inverters/protocols/Pylon-RS485-LV-V3.5-ttsun.pdf)
(«системные» 0x60–0x63, ADR 0x12 или 0x02). Шлюз показывает всю батарею как
**один** модуль Pylontech (ADR 0x02) с суммарными данными. Команды 0x94 («задать
лимиты»), 0x95 и 0x64 («выключить») отклоняются кодом 0x04. Запрет заряда и
разряда передаётся флагами 0x92/0x63 — на 0x44 инверторы часто не смотрят (ТЗ 4.2),
хотя 0x44 тоже заполняется полностью.

Решено по V3.3:

- **Ток в 0x42 — шаг 100 мА** (пример: −4000 мА = 0xFFD8). В ТЗ 4.2 написано
  10 мА — это ошибка ТЗ. Опция `PYLON_CURRENT_UNIT` оставлена на всякий случай.
- **«Charge immediately» в 0x92** — ставятся оба бита 5 и 4, как советует V3.3.

Проверить логом на SMH III (`menuconfig` → «Pylontech RS485», `PYLON_LOG_FRAMES`):

- **Знак DCL в 0x92** — V3.3 его не задаёт; по умолчанию положительный, как у diyBMS.
- **VER в ответе на 0x4F** — по умолчанию 0x20, как в примерах; diyBMS отвечает 0x28.
- **Ошибки в примерах спецификаций.** В примере 0x42 из V3.3 неверен CHKSUM. В примере 0x61 поле «модуль» в расшифровке
  записано как `0x0304`, а в самом кадре — `0x0034`; шлюз шлёт как в кадре.
  В примере 0x63 неверен LENGTH, в примере 0x60 — LENGTH и CHKSUM.

## Железо

- Классический ESP32 и ESP32-S3 имеют **один** контроллер CAN. Если BMZ на CAN,
  второй выход Pylontech CAN потребует MCP2515 (SPI) или ESP32-C6 (два TWAI).
  RS485 к инвертору идёт по UART, тут конфликта нет.
- Waveshare ESP32-S3-RS485-CAN (вариант из ТЗ 3.1) питается **7–36 В** — от шины
  48 В напрямую нельзя, нужен DC-DC. Направление RS485 на ней автоматическое
  (`PYLON_DE_GPIO = -1`). GPIO для RS485 и CAN на wiki нет — взять из схемы платы.
- **Easun RS485 на порту BMS:** по мануалу SMG II 6.2KP — 1 B, 2 A, 7 A, 8 B
  ([pinouts/](pinouts/README.md)); в ТЗ 2.2 для SMH III указано 1 A, 2 B, 8 GND.
  Если связи нет — поменять A и B.
- Пины по умолчанию — из ТЗ 3.2: CAN TX/RX = GPIO5/4, RS485 TX/RX = GPIO17/16.
  DE в ТЗ указан на GPIO4, а он занят CAN RX; по умолчанию DE не используется.
