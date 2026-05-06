# Брокер сообщений

Курсовая работа. Брокер сообщений на C++ с персистентным хранением, авторизацией и потокобезопасной доставкой.

## Документация

[Смотреть документацию](https://MatveyPopov1.github.io/Broker/html/index.html)

## Сборка

mkdir build && cd build
cmake ..
make

## Запуск

./broker

## Docker

docker build -t broker .
docker run -p 8080:8080 broker
