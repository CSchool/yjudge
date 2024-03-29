# Архитектура системы

Несколько компонентов, желательно контейнеризованных:

 - Веб-интерфейс и публичный HTTP API
 - Очередь сообщений
 - Инвокер
 - Файловая БД
 - Реляционная БД
 - Менеджер
 
## Файловая БД

Хранит файлы по их SHA-256 хешу. Принимает CRD запросы.
Можно масштабировать, разбивая обязанности по остатку от деления хеша на количество инстансов.

## Инвокер

Принимает запросы вида: вот файл с программой, язык, параметры компиляции и т.д.,
запусти на этом тесте. Файлы задаются их SHA-256 хешами.
Принимает команды, управляет кэшем, скачивает нужные файлы для выполнения запроса,
если их нет в кэше. Передаёт "тупому" инвокеру, который на той же машине,
он просто запускает что-то с кастомными лимитами, workdir, stdin, stdout и т.п.

## Реляционная БД

Хранит всю информацию в системе.

## Менеджер

Управляет контестами, запускает/останавливает в нужное время.
Принимает запросы на новые посылки, отсылает их инвокеру, запускает грейдеры, чекеры и т.п.
