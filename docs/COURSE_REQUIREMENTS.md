# Требования курса CGF

Задание предусматривает интерактивную графику на OpenGL с современным шейдерным конвейером. В Jester Jack используются C++17, OpenGL 3.3 Core и CMake 3.24+.

## Графические техники

| Требование | Реализация и демонстрация |
| --- | --- |
| Real-time interactive graphics | Цикл GLFW/OpenGL, действия мышью и клавиатурой |
| Modern shader-based pipeline | VAO/VBO/EBO, собственные GLSL vertex/fragment shaders |
| Scene setup | 3D-стол, карты, фишки, казино, камера и свет |
| Transformation pipeline | Model/view/projection и матрица нормалей через GLM |
| Lighting/shading | Blinn-Phong: ambient, diffuse, specular |
| Texturing or equivalent effect | Процедурные изображения, GL_TEXTURE_2D, UV и выборка в шейдере |

ImGui дополняет собственную 3D-сцену. На защите следует показать преобразования объектов, освещение, текстуры и реакцию на ввод, а также объяснить связь C++-данных с GLSL.

## Материалы сдачи

- Репозиторий, собираемый на машине преподавателя: исходники, зависимости, лицензии и инструкция сборки.
- Демонстрация вживую или видео длительностью 3-6 минут.
- Письменный отчёт по шаблону курса и слайды: задача, архитектура, графические техники, результаты проверок, источники и вклад участников.

## Контрольные точки

| Период курса | Материалы |
| --- | --- |
| Week 5 / Midterm | Proposal; прототип с циклом рендеринга и одной функцией; Introduction, Background, Project Description, Methodology |
| Week 7 | Feature-complete beta: освещение, текстуры и взаимодействие |
| Finals week | Демонстрация, отчёт и слайды |

В доступном фрагменте рубрики пункт Technical depth & correctness составляет 35%. Технические источники и страницы лекций перечислены в [LECTURE_ALIGNMENT.md](LECTURE_ALIGNMENT.md); устройство проекта — в [ARCHITECTURE.md](ARCHITECTURE.md).
