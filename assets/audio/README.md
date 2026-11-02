# Jester Jack audio

Все файлы из папки «JESTER JACK» команды. Имена приведены к ASCII; игра ищет файлы именно по этим путям (`src/audio/Mixer.cpp`).

| Файл | Исходное имя | Когда играет |
| --- | --- | --- |
| `sfx/card_deal.mp3` | Dealer's distribution of cards | Каждая карта, розданная игроку и дилеру при раздаче |
| `sfx/card_roll.mp3` | The player's card roll | Игрок берёт или выбирает карту (HIT), а также дилер добирает карту |
| `sfx/chips_start.mp3` | giving chips to player and bot | Начало нового забега |
| `sfx/chips_bet.mp3` | the bet of the chips | Ставка (раздача), оплата улучшения |
| `sfx/chips_win.mp3` | winning chips | Игрок получает монеты; дилер забирает проигранную раздачу; продажа улучшения |
| `sfx/tada_minor.mp3` | tada minor sound | Выиграна раздача |
| `sfx/tada_major.mp3` | tada major sound | Выиграна раздача, выполнившая цель раунда |
| `sfx/you_lose.mp3` | you lose sound | Конец забега |
| `ui/ui_open.mp3` | UI Open | Нажатие кнопок интерфейса |
| `ui/ui_close.mp3` | UI Close | Escape, BACK, DONE, GOT IT, MENU |
| `ui/ui_selection.mp3` | UI Selection | Курсор входит на кнопку, только в меню (не во время забега) |
| `ambient/car_pass.mp3` | car driving by (нормализован +6 дБ с лимитером) | Машина проезжает по улице в главном меню; запись привязана к положению машины |
| `ui/ui_error.mp3` | UI Error | Недопустимая команда или недоступная покупка |
| `music/main_theme.mp3` | JESTER JACK - Main theme | Меню, по кругу |
| `music/soundtrack_1.mp3` ... `soundtrack_5.mp3` | JJ - soundtrack 1 ... 5 | За столом, случайный порядок по кругу |

`music/soundtrack_N.sync`: сетка долей и кривые громкости для ансамбля на сцене, создаются командой `python scripts/analyze_soundtrack.py assets/audio/music/soundtrack_*.mp3` (нужны ffmpeg, numpy, scipy). У нового саундтрека без `.sync` музыканты играют в темпе по умолчанию.

Дополнительные саундтреки: добавьте `soundtrack_N.mp3` в `music/`, они подхватываются автоматически.

Авторство и лицензии записей не указаны; владелец проекта должен внести их сюда (см. `THIRD_PARTY_NOTICES.md`).
