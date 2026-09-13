# Контроллер монитора воздуха

ESP32, C/C++, ESP-IDF 6.1. Этот репозиторий содержит только прошивку и инструменты. NestJS backend разворачивается отдельно; связь — HTTPS JSON.

## Запуск

Установи ESP-IDF 6.1 через EIM. Из корня этого репозитория:

Создай два локальных файла в firmware/main/config/.

wifi_config.local.h:
```cpp
#pragma once
namespace wifi_config {
inline constexpr char kSsid[] = "";
inline constexpr char kPassword[] = "";
}
```

api_config.local.h:
```cpp
#pragma once
namespace api_config {
inline constexpr char kEndpoint[] = "";
inline constexpr char kDeviceId[] = "home-air-01";
inline constexpr char kDeviceToken[] = "";
inline constexpr unsigned kIntervalSeconds = 30;
inline constexpr bool kAllowInsecureHttp = false;
}
```

Заполни локально Wi-Fi и API endpoint (`https://<домен>/api/v1/measurements`), deviceId и токен backend. Не коммить локальные конфиги. Изменения требуют build и flash.

```sh
make build
make flash
make monitor
```

scripts/esp.sh автоматически активирует SDK и ищет CH340. При другой установке задай ESP_IDF_ACTIVATE, при нескольких платах — ESP_PORT. Выход из monitor: Ctrl+]. Можно запускать make и из firmware/.

## Модули

- firmware/main/main.cpp — запуск PMS5003, Wi-Fi и телеметрии.
- wifi_manager — подключение и до 5 повторов.
- measurements — UART2/D16, потоковый парсер PMS5003, стабилизация и свежий снимок с source=pms5003.
- telemetry — SNTP, проверяемый HTTPS, снимок с паузой 30 секунд. Успех API: HTTP 202.

Датчик подключён, сырой поток ранее проверен. Новая версия полного приёма собрана; результат загрузки/проверки см. ../docs/PROGRESS.md. Очереди и повторной отправки измерений нет. До заполнения API-конфига телеметрия отключена. Конфигурация сборки и артефакты могут содержать секреты: не публиковать build/ или .local/.

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
        ├── measurement_source.cpp
        ├── include/measurement_source.h
        ├── pms5003_parser.cpp / .h
        └── include/sample_store.h
```

Реализация measurement_source, включая тело start и задачу чтения, находится в measurements/measurement_source.cpp; заголовок measurement_source.h описывает интерфейс для main и telemetry. Парсер и SampleStore остаются в measurements. main.cpp читает сгенерированные настройки из main/config и вызывает start(config). Компоненты не подключают конфиги приложения. Публичные типы Config и функции объявлены в include/, реализация — в .cpp. Каждый компонент объявляет свои зависимости через PRIV_REQUIRES, а INCLUDE_DIRS экспортирует только публичные заголовки.

Зависимости: main → measurements + wifi_manager + telemetry; telemetry → wifi_manager + measurements + сетевые компоненты ESP-IDF. Обратной зависимости от main нет. Это компоненты сборки ESP-IDF, без NestJS-декораторов или DI-контейнера.

wifi_manager копирует credentials в драйвер при start(). telemetry копирует структуру Config, но хранит указатели на строки: строки должны жить всё время работы программы и не меняться. В main используются inline constexpr массивы со статическим временем жизни. start вызывается из app_main однократно. Компоненты проверяют параметры во время запуска; неправильный конфиг не включает соответствующую функциональность.

## Приоритет HTTPS-задачи

Телеметрия выполняется с tskIDLE_PRIORITY: синхронные вычисления TLS делят процессор с IDLE благодаря time slicing FreeRTOS. Ранее приоритет 5 вытеснял IDLE1 дольше 5 секунд и срабатывал task watchdog. Watchdog и проверка сертификатов остаются включены. Под нагрузкой отправка может занимать больше времени; это фоновая задача без жёсткого срока выполнения. Если другие задачи постоянно занимают CPU, их поведение нужно проверять отдельно.

Для новой версии проверяем firmwareVersion 0.3.0-pms5003, рост frames во время HTTPS и HTTP 202 без task_wdt. Сборка сама по себе этого не подтверждает.

## Реальный приём PMS5003

main запускает реальный источник и Wi-Fi/HTTPS-телеметрию. Номер входа `kRxGpio = 16` хранится в `firmware/main/config/sensor_config.h` и передаётся из main.

- UART2 9600 8N1, отдельная задача с приоритетом 5 и блокирующим ожиданием событий. UART0 остаётся консолью.
- Парсер хранит максимум 32 байта, принимает только заголовок 42 4D, длину 28 и совпадающую checksum. Восстанавливается после шума/частичного/повреждённого кадра, выбирает atmospheric PM1.0/PM2.5/PM10 (µg/m³).
- Публикация разрешена после 30 секунд с первого корректного кадра. Пауза между корректными кадрами 10 секунд делает снимок устаревшим; при возобновлении повторяется стабилизация. UART overflow/framing/parity/break сбрасывают накопление и доступность снимка.
- Последний снимок защищён короткой критической секцией. Чтение UART продолжается при отсутствии Wi-Fi и во время HTTP. Неактуальный снимок не отправляется, mock/нули не подставляются.
- Каждые 5 секунд Serial показывает PM или состояние и счётчики frames/bad_length/bad_checksum/uart_errors.
- API получает source=pms5003, firmwareVersion=0.3.0-pms5003, sampleCount=1 и windowSeconds=0. measuredAt вычисляется с учётом возраста снимка; uptimeMs относится к приёму кадра.

Это последние снимки, не средние за интервал. Истории/очереди повторов нет: при недоступной сети старые снимки заменяются новыми. Код не управляет SET/RESET/RX датчика и рассчитан на активную выдачу после включения. Точная схема клона платы не установлена; текущая сборка ранее прошла краткий тест при согласованном пользователем пробном питании (подробнее ../docs/HARDWARE.md).
