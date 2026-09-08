# Контроллер монитора воздуха

ESP32, C/C++, ESP-IDF 6.1. Этот репозиторий содержит только прошивку и инструменты. NestJS backend разворачивается отдельно; связь — HTTPS JSON.

## Запуск

Установи ESP-IDF 6.1 через EIM. Из корня этого репозитория:

```sh
cp firmware/main/config/wifi_config.example.h firmware/main/config/wifi_config.local.h
cp firmware/main/config/api_config.example.h firmware/main/config/api_config.local.h
```

Заполни локально Wi-Fi и API endpoint (`https://<домен>/api/v1/measurements`), deviceId и токен backend. Не коммить локальные конфиги. Изменения требуют build и flash.

```sh
make build
make flash
make monitor
```

scripts/esp.sh автоматически активирует SDK и ищет CH340. При другой установке задай ESP_IDF_ACTIVATE, при нескольких платах — ESP_PORT. Выход из monitor: Ctrl+]. Можно запускать make и из firmware/.

## Модули

- firmware/main/main.cpp — запуск Wi-Fi и телеметрии.
- wifi_manager — подключение и до 5 повторов.
- measurement_source.h / mock_measurement_source.cpp — случайные показания, source=mock.
- telemetry — SNTP, проверяемый HTTPS, снимок с паузой 30 секунд. Успех API: HTTP 202.

Датчик пока не подключён. Очереди и повторной отправки измерений нет. До заполнения API-конфига телеметрия отключена. Конфигурация сборки и артефакты могут содержать секреты: не публиковать build/ или .local/.

Общая учебная документация при работе в исходной рабочей папке лежит в ../docs/. Для самостоятельной сборки она не нужна.

## Компоненты ESP-IDF

```text
firmware/
├── main/
│   ├── main.cpp
│   ├── CMakeLists.txt
│   └── config/
└── components/
    ├── wifi_manager/
    │   ├── CMakeLists.txt
    │   ├── include/wifi_manager.h
    │   └── wifi_manager.cpp
    ├── telemetry/
    │   ├── CMakeLists.txt
    │   ├── include/telemetry.h
    │   └── telemetry.cpp
    └── measurements/
        ├── CMakeLists.txt
        ├── include/measurement_source.h
        └── mock_measurement_source.cpp
```

main.cpp читает сгенерированные настройки из main/config и вызывает start(config). Компоненты не подключают конфиги приложения. Публичные типы Config и функции объявлены в include/, реализация — в .cpp. Каждый компонент объявляет свои зависимости через PRIV_REQUIRES, а INCLUDE_DIRS экспортирует только публичные заголовки.

Зависимости: main → wifi_manager + telemetry; telemetry → wifi_manager + measurements + сетевые компоненты ESP-IDF. Обратной зависимости от main нет. Это компоненты сборки ESP-IDF, без NestJS-декораторов или DI-контейнера.

wifi_manager копирует credentials в драйвер при start(). telemetry копирует структуру Config, но хранит указатели на строки: строки должны жить всё время работы программы и не меняться. В main используются inline constexpr массивы со статическим временем жизни. start вызывается из app_main однократно. Компоненты проверяют параметры во время запуска; неправильный конфиг не включает соответствующую функциональность.
