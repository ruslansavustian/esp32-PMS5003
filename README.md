# Контроллер монитора воздуха

ESP32, C/C++, ESP-IDF 6.1. Этот репозиторий содержит только прошивку и инструменты. NestJS backend разворачивается отдельно; связь — HTTPS JSON.

## Запуск

Установи ESP-IDF 6.1 через EIM. Из корня этого репозитория:

```sh
cp firmware/main/wifi_config.example.h firmware/main/wifi_config.local.h
cp firmware/main/api_config.example.h firmware/main/api_config.local.h
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
