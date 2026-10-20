# Источники — инверторы 48V и протоколы связи с BMS

Дата сбора: 2026-10-21. Колонка «Локально» — файл в этой папке.

## Мануалы инверторов (`manuals/`)

| Инвертор | URL | Локально | Заметки |
|---|---|---|---|
| Deye SUN-3/3.6/5/6K-SG03LP1-EU | https://www.deyeinverter.com/deyeinverter/2024/08/13/instructions_sun-3-6k-sg03lp1-eu_240813_en.pdf | `Deye-SUN-3-6K-SG03LP1-EU-manual-240813-en.pdf` | Официальный, 08/2024. Распиновка BMS — с. 53 (Appendix) |
| Deye SUN-3.6/5/6K-SG03LP1-EU | https://www.deyeinverter.com/deyeinverter/2024/02/03/instructions_sun-3.6-6k-sg03lp1-eu_240203_en.pdf | `Deye-SUN-3.6-6K-SG03LP1-EU-manual-240203-en.pdf` | Официальный, 02/2024 |
| Deye SG03LP1 даташит (укр.) | https://www.sig.energy/wp-content/uploads/2025/09/deye-sun-6k-sg03lp1-datasheet-ua.pdf | `Deye-SUN-3.6-6K-SG03LP1-datasheet-ua.pdf` | Связь с батареей: RS485, CAN |
| Easun ISolar SMG II 6.2KP 48V WIFI | https://cdn-files.myshopline.com/file/store/1649660287869/ISolar-SMG-II-6-2KP-48V-WIFI-Manual.pdf | `Easun-ISolar-SMG-II-6.2KP-48V-WIFI-manual.pdf` | С сайта easun-energy. BMS только RS485, «Li2» в программе 5 |
| Easun ISolar SMG II 6.2KP WIFI | https://storage.ua.prom.st/2790936_isolar_smg_ii_6.2kp_wifi.pdf | `Easun-ISolar-SMG-II-6.2KP-WIFI-manual-prom.pdf` | Копия с prom.ua |
| Easun SMG II Series (220 стр.) | https://www.manualslib.com/manual/4027802/Easun-Power-Isolar-Smg-Ii-Series.html | — | Только онлайн |
| Anenji ANJ-6200-48PL-WIFI | https://cdn-files.myshopline.com/file/store/1675916196960/ANJ-6200-48PL-WIFI-user-manual.pdf | `Anenji-ANJ-6200-48PL-WIFI-manual.pdf` | Распиновка 5.5 и 6.2 kVA, с. 38 |
| Anenji ANJ-6200W-48V-WIFI | https://electro100.ua/files/attach_files/anj_6200w_48v_wifi_user_manual.pdf | `Anenji-ANJ-6200W-48V-WIFI-manual-electro100.pdf` | Распиновка BMS — с. 22 |
| Anenji 6.2 kW 48V | https://solar-assistant.io/help/inverters/anenji/ANJ-6200W-48V | — | SolarAssistant: RS232/RS485 для мониторинга |
| Anenji (форум) | https://diysolarforum.com/threads/anenji-anj-6200w-48v.85702/ | — | «Только RS485 (Modbus) к батарее» |
| Victron MultiPlus-II 48V (3k–15k, Australia) | https://www.victronenergy.com/upload/documents/MultiPlus-II_3k_5k_8k_10k_15k_Australia/157822-MultiPlus-II_230V_Australia-pdf-en.pdf | `Victron-MultiPlus-II-48V-Australia-157822-en.pdf` | Официальный. У MultiPlus нет BMS-порта: BMS → GX (Cerbo) по VE.Can |
| Victron MultiPlus-II 48/5000 230V (исп.) | https://cuencasolar.es/wp-content/uploads/2024/06/Manual-MultiPlus-II-48-5000-70-50-230V-Victron.pdf | `Victron-MultiPlus-II-230V-es.pdf` | Rev 10, 03/2024 |
| Voltronic Axpert VM IV 4K/6K TWIN | https://voltronicpower.com/content/download/Manual/Axpert%20VM%20IV-4K6K-TWIN-Off-grid-manual-20220629.pdf | `Voltronic-Axpert-VM-IV-4K6K-TWIN-manual-20220629.pdf` | Официальный. Распиновка BMS (RS232/485/CAN) — с. 53. Easun/Anenji — клоны этой платформы |
| Voltronic Axpert MAX | https://cdn.autosolar.co/pdf/manual-voltronic-axpert-max.pdf | `Voltronic-Axpert-MAX-manual.pdf` | DIP-настройки Pylontech, с. 62 |
| Voltronic Axpert VMIII / KING + Pylontech | https://portal.segensolar.co.za/reseller/docs/Voltronic%20Axpert%20VMIII-KING.pdf | `Voltronic-Axpert-VMIII-KING-48V-manual.pdf` | Распиновка обеих сторон, с. 3 |

## Протоколы и спецификации (`protocols/`)

### Pylontech (главный целевой протокол bms-bridge)
| URL | Локально | Что |
|---|---|---|
| https://github.com/Frankkkkk/python-pylontech/blob/master/RS485-protocol-pylon-low-voltage-V3.3-20180821.pdf | `Pylon-RS485-LV-V3.3-20180821.pdf` | **Pylon LV RS485 V3.3**, 25 стр.: ASCII-кадры `~…\r`, команды 0x42/0x44/0x47/0x92 и др. |
| https://tt-sun.com/wp-content/uploads/2025/03/PYLON-RS485-protocol-pylon-low-voltage-%E6%96%B0%E5%A2%9E%E5%8D%8F%E8%AE%AE%E8%AE%BE%E8%AE%A1-dy12%E5%8F%B79600.pdf | `Pylon-RS485-LV-V3.5-ttsun.pdf` | **Pylon LV RS485 V3.5**, 21 стр. (история версий до 3.5) |
| https://akkudoktor.net/uploads/short-url/7PRjIO1R9OBfix2OZ6qCcU0FjlV.pdf | `akkudoktor-7PRjIO1R9OBfix2OZ6qCcU0FjlV.pdf` | PDF протокола с Akkudoktor, 6 стр. — вероятно Pylon CAN, не проверял |
| https://akkudoktor.net/uploads/short-url/dwFYBPwjnfia9tSuhUopj9uGX2u.pdf | `akkudoktor-dwFYBPwjnfia9tSuhUopj9uGX2u.pdf` | PDF протокола с Akkudoktor, 5 стр. — не проверял |
| https://portal.segen.co.uk/reseller/docs/Sunsynk_BatteryCompatibility_v14_English.pdf | `Sunsynk-BatteryCompatibility-v14.pdf` | Распиновка, раскладка Pylon CAN (0x359 и др.), список батарей |
| https://powerforum.co.za/topic/12901-pylontech-protocol/ | — | Форум: Pylon CAN PDF 2016, Teensy-подмена фреймов |
| https://diysolarforum.com/threads/rs485-pylontech-protocol-info-required.108031/ | — | Форум: Pylon CAN V1.2 (2018) с доп. ID |
| https://github.com/meteosat007/solar-pylontech | — | Ещё одна копия Pylon RS485 V3.3 + Sofar |

### Victron (CAN-bus BMS)
| URL | Локально | Что |
|---|---|---|
| https://www.victronenergy.com/live/battery_compatibility:can-bus_bms-cable | — | Кабели Type A/B, распиновка, BMZ = Type B |
| https://communityarchive.victronenergy.com/questions/48924/can-bms-protocol-specification.html | — | Обязательные ID 0x351/355/356/35A; 0x35E = имя производителя; спека под NDA |
| https://communityarchive.victronenergy.com/storage/attachments/usermanual-rec-victron-bms.pdf | `Victron-forum-REC-victron-bms-manual.pdf` | REC BMS для Victron: кадры CAN |
| https://communityarchive.victronenergy.com/storage/attachments/simp-bms-setup-manual-v025.pdf | `Victron-forum-SimpBMS-setup-manual-v025.pdf` | SimpBMS: CAN-кадры Victron, подключения |
| https://github.com/Tom-evnut/VEcan/blob/master/VEcan.ino | `VEcan.ino` | Минимальный Arduino-скетч Victron CAN-BMS |
| https://community.victronenergy.com/t/vecan-bms-canbus-dbc-file-datasheet/51173 | — | DBC для Victron CAN-BMS |
| https://community.victronenergy.com/t/ve-can-pinout-and-termiators/49517 | — | Распиновка VE.Can, терминаторы, GND |
| https://github.com/dfch/BydCanProtocol | — | Эмуляция BYD для Venus OS: фреймы и тайминги |

### Deye / Sunsynk
| URL | Локально | Что |
|---|---|---|
| https://www.deyeinverter.com/deyeinverter/2024/01/04/deyeapprovedbatterylistdy-lv48-0076.pdf | `Deye-approved-battery-list-LV48-0076.pdf` | Approved Battery List LV: протокол 00 = Pylon CAN |
| https://www.deyeinverter.com/deyeinverter/2025/08/11/deye+approved+battery+listdy-hv160-800-028.pdf | — | То же для HV |
| https://volton.bg/userfiles/product_files_shared/Product%20Files/CERNNSS/Communication%20Deye%20Ho0104%20V2408.pdf | `Deye-Ho01-04-BMS-communication-guide-Volton.pdf` | Deye + параллельные батареи: Lithium Mode 00, кабели |
| https://motoma.cn/wp-content/uploads/2023/11/Connect-MOTOMA-Lithium-Battery-with-Deye-Inverter.pdf | `Motoma-to-Deye-connection.pdf` | Пошагово: батарея → Deye, распиновка |
| https://diysolarforum.com/threads/deye-and-jk-bms-wont-communicate-via-can.92293/ | — | JK BMS + Deye: работает протокол 004 (Victron), после обновления прошивки — Pylon |
| https://savolture.com/blog/deye-approved-battery-list-2026/ | — | Разбор кодов Lithium Mode |

### Voltronic / Axpert (на этой платформе — Easun, Anenji, Must, Mecer…)
| URL | Локально | Что |
|---|---|---|
| https://github.com/ardupic/voltronic-inverter-communication-protocols | — | Репозиторий официальных протоколов Voltronic |
| …/Voltronic Inverter and BMS 485 communication protocol 20201202.docx | `Voltronic-Inverter-and-BMS-485-protocol-20201202.docx` | **Протокол инвертор↔BMS по RS485** (Voltronic native, 9600 8N1) |
| …/BMS Modbus Protocol for RS485 V1.1(2018-11-15)(2).pdf | `Voltronic-BMS-Modbus-RS485-V1.1-20181115.pdf` | BMS Modbus RS485 V1.1 |
| …/Axpert MAXIIVMIV MKSIV Communication Protocol20210804.pdf | `Voltronic-Axpert-MAXII-VMIV-MKSIV-protocol-20210804.pdf` | Протокол мониторинга инвертора (QPIGS и др.) |
| …/Axpert MAX Communication Protocol20210203.pdf | `Voltronic-Axpert-MAX-protocol-20210203.pdf` | То же, Axpert MAX |
| …/Axpert MKS II&MKS III&MKS IV RS232 Protocol 20201109.pdf | `Voltronic-Axpert-MKS-II-III-IV-RS232-20201109.pdf` | RS232 протокол MKS |
| https://www.scribd.com/document/869898926/Voltronic-Inverter-and-BMS-485-communication-protocol | — | Копия на scribd |
| https://ecobatenergy.ro/uploads/eshop/produse_upload/Voltronic-Installation-Manual.pdf | `Voltronic-installation-manual-ecobat.pdf` | Установка батареи с Voltronic |
| https://github.com/alcestide/easun-smg-ii-11kw | — | Modbus-регистры Easun SMG II 8/11 kW |
| https://powerforum.co.za/topic/33314-jk-bms-to-easunisolar-smg-iii-62kw/ | — | JK BMS → Easun SMG-III 6.2 kW через ESP32 |
| https://powerforum.co.za/topic/18051-easun-inverters/ | — | Easun: LI1–LI4, ошибки связи |

### Growatt, Goodwe, SMA, прочие
| URL | Локально | Что |
|---|---|---|
| https://www.scribd.com/document/869897722/Growatt-BMS-CAN-Bus-Protocol-Low-Voltage-V1-04-1-Translate | — | Growatt BMS CAN LV V1.04 (scribd, не скачать) |
| https://powerforum.co.za/topic/13527-growatt-lithium-battery-protocol/ | — | Growatt: протоколы L51…L99, программа 36 |
| https://www.rec-bms.com/wp-content/uploads/2021/04/UserManual_REC_Goodwe_BMS.pdf | `REC-Goodwe-BMS-manual.pdf` | REC BMS для Goodwe LV (CAN) |
| https://rec-bms.com/datasheet/UserManual_REC_SI_BMS.pdf | `REC-SI-BMS-manual.pdf` | REC BMS для SMA Sunny Island: 0x351/355/356/35A/35E/35F |
| https://www.scribd.com/document/804230084/SMA-CAN-Protocol-2 | — | SMA Sunny Island external BMS CAN (scribd) |
| https://www.jkbms.net/solutions/inverter-bms-communication | — | JK BMS: 14 протоколов инверторов, распиновки, битрейты |
| https://batterydistributors.co.za/wp-content/uploads/2023/04/VOLTA-Communication-Setting1203.pdf | `VOLTA-battery-multi-inverter-comm-settings.pdf` | **Распиновки кабелей батарея → Deye, Luxpower, Voltronic, SMA, Victron, SRNE, Growatt, TBB, SAKO** |
| https://www.offgridtec.com/media/product_attachements/Offgridtec_BMS%20Communication_Protocols&Fixed-ID_EN.pdf | `Offgridtec-BMS-Communication-Protocols.pdf` | Таблица протоколов по производителям инверторов |
| https://greenpowertalk.tech/threads/ne-mogu-soedenit-po-can-shine-must-pv18-5248-pro-batareja-must-lp16-48100.1529/ | — | Must PV18: CAN, пункт 41 = 8, протокол Pylon |
