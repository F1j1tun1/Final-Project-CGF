# Jester Jack

Одиночный блэкджек с бесконечными раундами и улучшениями. C++17, OpenGL 3.3 Core.

## Запуск

В готовой сборке откройте `JesterJack.exe`. Рядом должна оставаться папка `assets/`. Для запуска нужен Windows с драйвером OpenGL 3.3 Core.

## Правила и управление

За пять раздач наберите цель раунда: сначала 30 очков, затем +10 за раунд, максимум 100. Победная рука приносит очки и 3 монеты. После выполнения цели открывается магазин с двумя предложениями. Можно держать до четырёх улучшений и продавать их за половину цены. Если раздачи закончились без выполнения цели, забег завершается.

| Управление | Действие |
| --- | --- |
| H | Взять карту |
| S | Остановиться |
| Space | Основное действие: старт, раздача, остановиться, продолжить или повторить забег |
| Enter | Нажать выделенную кнопку |
| F1 | Настройки |
| Escape | Закрыть правила/настройки или вернуться в меню |

Полные правила — HOW TO PLAY. Кнопки также работают мышью. Выход — EXIT или QUIT GAME.

## Сборка на Windows

Нужны CMake 3.24+ и Visual Studio 2022/2026 либо Build Tools с MSVC, Windows SDK и компонентом Desktop development with C++. Зависимости включены в `third_party/`.

В терминале из папки с `CMakeLists.txt`:

```powershell
cmake --preset desktop
cmake --build --preset desktop --parallel
ctest --preset desktop
.\build\desktop\Debug\blackjack_forge.exe
```

Если CMake не видит MSVC, используйте Developer PowerShell. Имя собираемого файла — `blackjack_forge.exe`; игра называется Jester Jack.

Только правила и тесты, без окна и GPU:

```powershell
cmake --preset core-only
cmake --build --preset core-only --parallel
ctest --preset core-only
```

Лицензии библиотек и шрифтов сохранены в `third_party/`, `assets/fonts/` и папке `licenses/` готовой сборки. Источники сторонних компонентов — `THIRD_PARTY_NOTICES.md`. При копировании игры сохраняйте ресурсы и лицензии.
