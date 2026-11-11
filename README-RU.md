# LIVE VOICE — справочник (RU)

Программа для Windows: живая обработка голоса ведущего/диджея на мероприятиях.
Делает голос «дорогим» (как на радио), позволяет крутить высоту голоса вживую и добавляет эффекты.
Автор: Андрей Ульянов (Thematic). Сделано с помощью Claude (Anthropic).

## Что внутри (0.16)

| Блок | Что делает |
|---|---|
| **VOICE PRESETS** | 10 готовых «коммерческих» звуков: CLEAN, RADIO, CLUB MC, HYPE MC (без понижения, звук 0.13), WARM −1, DEEP −1.5, BIG HOST −2, TRAILER −2.5, GIANT −3, TITAN −4 (мягкие режимы, органичное понижение) (на плитках только названия). Внутри: режим обработки, сила, понижение, клиппер, иногда эффект |
| **PITCH** | Большая ручка ±6: меняет «размер» голоса (форманты), верха выше 4.5 кГц остаются от оригинала. Движок [Signalsmith Stretch](https://signalsmith-audio.co.uk/code/stretch/) (MIT) |
| **EFFECTS** | WIDE (моно-безопасный, немного во всех пресетах кроме CLEAN), SHIMMER, ECHO, BRUTAL («тяжёлый ведущий»): включаются вместе, у каждого своя сила. PITCH + EFFECTS — один модуль, кнопка HIDE; при запуске видны только пресеты |
| **Шапка** | IN/OUT, CLIP (клиппер), ON, AUDIO, MUTE |
| **Шум** | Умный гейт (шум комнаты +13 дБ) + повторное подавление после компрессора: паузы тише, чем на входе |

**Задержка:** пресеты без понижения — 0 мс обработки, с понижением — ~30 мс.

**Горячие клавиши:** 1…9, 0 — пресеты, ПРОБЕЛ — вкл/выкл, C — clip, M — mute, ↑/↓ — pitch, H — показать/спрятать PITCH и эффекты.

## Если нет интернета
Полная инструкция «на всё»: [`docs/OFFLINE-RU.txt`](docs/OFFLINE-RU.txt) — запуск, писк, задержка, сборка без интернета, продолжение с другим ИИ.

## Установка и первый запуск
См. `docs/README-RU.txt` (он же лежит в архиве сборки рядом с .exe): выбор микрофона и выхода, как слышать себя без писка, советы по задержке.

## Сборки
- Готовые архивы: ветка [`builds`](https://github.com/doublesensespb-rgb/live-voice/tree/builds) (`LIVE-VOICE-<версия>-Windows.zip`, скриншоты UI в `shots/`).
- Логи сборок: [Actions](https://github.com/doublesensespb-rgb/live-voice/actions).
- История изменений: [коммиты](https://github.com/doublesensespb-rgb/live-voice/commits/main).

## Как собрать самому (без Claude)
1. Установить [Visual Studio 2022 Community](https://visualstudio.microsoft.com/) (C++) и [CMake](https://cmake.org/download/).
2. Скачать [JUCE 8.0.10](https://github.com/juce-framework/JUCE/tree/8.0.10) в папку `JUCE` внутри проекта (в офлайн-архиве уже лежит).
3. Запустить `tools/build-windows.bat` — готовый `LIVE VOICE.exe` появится в `build/LiveVoice_artefacts/Release/Standalone/`.

Или: залить изменения в ветку `main` на GitHub — сборка запустится сама (GitHub Actions) и положит архив в ветку `builds`.
