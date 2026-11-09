# LIVE VOICE — справочник (RU)

Программа для Windows: живая обработка голоса ведущего/диджея на мероприятиях.
Делает голос «дорогим» (как на радио), позволяет крутить высоту голоса вживую и добавляет эффекты.
Автор: Андрей Ульянов (Thematic). Сделано с помощью Claude (Anthropic).

## Что внутри

| Блок | Что делает |
|---|---|
| **VOICE** | Цепочка обработки: срез низа → гейт → **авто-уровень** (тихие USB-микрофоны подтягиваются сами) → чистка «грязи» → компрессор → тело/присутствие/воздух → де-эссер → теплота. 4 режима: NATURAL, RADIO, CLUB, DEEP. STRENGTH — сила |
| **PITCH** | Большая ручка ±12: меняет «размер» голоса (форманты) — влево ниже и мощнее, вправо выше. Плавно, можно играть вживую. Движок: [Signalsmith Stretch](https://signalsmith-audio.co.uk/code/stretch/) (MIT). С 0.11 старый сдвиг ноты убран (на речи почти не слышен и давал шелест) |
| **SFX** | WIDE (стерео-дублер, не пропадает в моно), SHIMMER (ореол на октаву выше), ECHO THROW (пинг-понг эхо, приглушается пока говоришь), BRUTAL (слой на октаву ниже с перегрузом). AMOUNT — сила |
| **Выход** | CLIP (кнопка: мягкий клиппер, плотнее без прибавки громкости, 0 мс), LEVEL, MUTE, лимитер −1 dBFS |
| **Пресеты** | `Документы/LIVE VOICE/Presets`, папки, SAVE/REPLACE, 6 заводских в папке Factory |
| **UNDO** | Кнопка + Ctrl+Z / Ctrl+Shift+Z (Ctrl+Y) |

**Задержка:** при PITCH = 0 и выключенном BRUTAL голос идёт мимо питч-шифтера — 0 мс обработки
(программа сама плавно переключается). При сдвиге высоты — ~30 мс.

**Горячие клавиши:** ПРОБЕЛ — голос вкл/выкл, M — mute, ↑/↓ — pitch ±1, 0 — сброс pitch, Ctrl+Z — undo, Ctrl+S — сохранить пресет.

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
