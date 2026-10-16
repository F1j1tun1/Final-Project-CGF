# Совместная работа

Репозиторий: [F1j1tun1/Final-Project-CGF](https://github.com/F1j1tun1/Final-Project-CGF). Сборка требует C++17 и CMake 3.24+; приложение использует OpenGL 3.3 Core.

Создайте ветку от актуального main, сохраните изменения и откройте Pull Request. Согласуйте общие файлы с участниками; не перезаписывайте чужую работу и историю общих веток.

```powershell
git switch -c feature/change-name
cmake --preset core-only
cmake --build --preset core-only --parallel
ctest --preset core-only
cmake --preset desktop
cmake --build --preset desktop --parallel
ctest --preset desktop
.\build\desktop\Debug\blackjack_forge.exe --smoke
```

`src/game/` содержит чистые правила и случайность по seed; `src/graphics/` — отрисовку; `src/main.cpp` — окно, ввод и UI. Недопустимые команды не меняют состояние, визуальные эффекты не влияют на исходы. Техническое устройство описано в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Для изменения правил добавьте содержательный тест. Для графики проверьте видимый кадр, размер окна и нужный сценарий. В PR укажите выполненные команды и результат. Сохраняйте лицензии и источники ресурсов; не добавляйте build, бинарники, временные файлы и секреты. Документация — на русском, интерфейс и идентификаторы — на английском.
