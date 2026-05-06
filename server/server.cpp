#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <csignal>

Server* globalServer = nullptr;

void signalHandler(int signum) {
    if (globalServer) {
        std::cout << "\nПолучен сигнал " << signum << std::endl;
        globalServer->stop();
    }
}

std::string Server::getWelcomeMessage() {
    return std::string(
        "=============================================\n"
        "  ДОБРО ПОЖАЛОВАТЬ В БРОКЕР СООБЩЕНИЙ\n"
        "=============================================\n"
        "\n"
        "Доступные команды:\n"
        "\n"
        "  REGISTER <логин> <пароль> - Регистрация нового пользователя\n"
        "  LOGIN <логин> <пароль>    - Авторизация пользователя\n"
        "  HELP                      - Показать эту справку\n"
        "  PING                      - Проверка соединения\n"
        "  DISCONNECT                - Отключиться от сервера\n"
        "\n"
        "=============================================\n"
    );
}

std::string Server::getHelpMessage(bool authenticated) {
    if (!authenticated) {
        return getWelcomeMessage();
    }

    return std::string(
        "=============================================\n"
        "  СПИСОК КОМАНД\n"
        "=============================================\n"
        "\n"
        "  SUB <топик> <потребитель>  - Подписка на топик\n"
        "  UNSUB <топик> <потребитель>- Отписка от топика\n"
        "  PUB <топик> <тело> <приор> - Публикация сообщения\n"
        "  ACK <id> <потребитель>     - Подтверждение обработки\n"
        "  LIST_TOPICS                - Список всех топиков\n"
        "  LIST_ACTIVE_TOPICS         - Список активных топиков\n"
        "  LIST_SUBS <потребитель>    - Список подписок пользователя\n"
        "  USERS                      - Список пользователей\n"
        "  HELP                       - Показать эту справку\n"
        "  DISCONNECT                 - Отключиться от сервера\n"
        "  PING                       - Проверка соединения\n"
        "\n"
        "=============================================\n"
    );
}

void Server::start(int port) {
    globalServer = this;
    running = true;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    offsetManager.load();
    subscriptionManager.load();
    userManager.load();

    std::thread(&Server::consumerThread, this).detach();
    std::thread(&Server::retryThread, this).detach();
    std::thread(&Server::compactThread, this).detach();
    if (interactive) {
        std::thread(&Server::consoleThread, this).detach();
    }

    logger.init("broker.log");
    logger.info("Сервер запущен на порту " + std::to_string(port));
    std::cout << "Введите 'stop' для остановки сервера" << std::endl;

    acceptClients();
}

void Server::consoleThread() {
    std::string input;
    while (running) {
        std::getline(std::cin, input);

        if (input == "stop") {
            logger.info("Получена команда остановки из консоли");

            stop();
            break;
        }
    }
}

void Server::disconnectClient(ClientState& state) {
    std::string name = state.consumerName;

    if (!name.empty()) {
        {
            std::lock_guard<std::mutex> lock(conn_mtx);
            activeConsumers.erase(name);
        }

        offsetManager.save();
        logger.info("Потребитель отключился: " + name);
    }

    shutdown(state.fd, SHUT_RDWR);
    close(state.fd);
}

void Server::stop() {
    logger.info("Завершение работы сервера...");
    running = false;

    {
        std::lock_guard<std::mutex> lock(conn_mtx);

        for (auto& pair : activeConsumers) {
            std::string msg = "SYS server shutting down\n";
            write(pair.second, msg.c_str(), msg.size());
            shutdown(pair.second, SHUT_RDWR);
            close(pair.second);
        }

        activeConsumers.clear();
    }

    storage.compact();
    logger.info("Финальное компактирование завершено");

    close(server_fd);
    exit(0);
}

void Server::acceptClients() {
    while (running) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        std::thread(&Server::handleClient, this, client).detach();
    }
}

void Server::sendResponse(int client_fd, const std::string& response) {
    std::string out = response + "\n";
    write(client_fd, out.c_str(), out.size());
}

void Server::handleClient(int client_fd) {
    ClientState state;
    state.fd = client_fd;
    state.buffer = "";
    state.consumerName = "";

    char temp[1024];

    sendResponse(client_fd, getWelcomeMessage());

    while (running) {
        int bytes = read(client_fd, temp, sizeof(temp) - 1);
        if (bytes <= 0) {
            disconnectClient(state);
            break;
        }

        temp[bytes] = '\0';
        state.buffer += std::string(temp);

        size_t pos;
        while ((pos = state.buffer.find('\n')) != std::string::npos) {
            std::string command = state.buffer.substr(0, pos);
            state.buffer.erase(0, pos + 1);

            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }

            if (!command.empty()) {
                std::string response;

                if (state.consumerName.empty()) {
                    response = processAuthCommand(state, command);
                } else {
                    response = processCommand(state, command);
                }

                if (response == "__DISCONNECT__") {
                    sendResponse(client_fd, "OK до свидания");
                    disconnectClient(state);
                    return;
                }

                if (!response.empty()) {
                    sendResponse(client_fd, response);
                }
            }
        }
    }

    close(client_fd);
}

std::string Server::processAuthCommand(ClientState& state, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "PING") {
        return "PONG";
    }

    if (cmd == "HELP") {
        return getHelpMessage(false);
    }

    if (cmd == "DISCONNECT") {
        return "__DISCONNECT__";
    }

    if (cmd == "REGISTER") {
        std::string username, password;
        iss >> username >> password;

        if (username.empty() || password.empty()) {
            return "ОШИБКА использование: REGISTER <логин> <пароль>";
        }

        if (userManager.userExists(username)) {
            return "ОШИБКА пользователь уже существует";
        }

        if (userManager.registerUser(username, password)) {
            logger.info("Пользователь зарегистрирован: " + username);
            return "OK регистрация успешна, выполните LOGIN";
        } else {
            return "ОШИБКА не удалось зарегистрировать";
        }
    }

    if (cmd == "LOGIN") {
        std::string username, password;
        iss >> username >> password;

        if (username.empty() || password.empty()) {
            return "ОШИБКА использование: LOGIN <логин> <пароль>";
        }

        if (!userManager.userExists(username)) {
            return "ОШИБКА пользователь не найден";
        }

        if (userManager.authenticate(username, password)) {
            state.consumerName = username;

            std::lock_guard<std::mutex> lock(conn_mtx);
            activeConsumers[username] = state.fd;

            logger.info("Пользователь вошел: " + username);

            std::string response = "OK вход выполнен\n" + getHelpMessage(true);
            sendResponse(state.fd, response);

            replayHistory(username, state.fd);

            return "";
        } else {
            return "ОШИБКА неверный пароль";
        }
    }

    return "ОШИБКА неизвестная команда";
}

std::string Server::processCommand(ClientState& state, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "PING") {
        return "PONG";
    }

    if (cmd == "HELP") {
        return getHelpMessage(true);
    }

    if (cmd == "DISCONNECT") {
        return "__DISCONNECT__";
    }

    if (cmd == "SUB") {
        std::string topic, consumer;
        iss >> topic >> consumer;

        if (topic.empty() || consumer.empty()) {
            return "ОШИБКА использование: SUB <топик> <потребитель>";
        }

        bool added = subscriptionManager.add(topic, consumer);

        if (added) {
            logger.info(consumer + " подписался на "+ topic);
            return "OK подписан на " + topic;
        } else {
            return "ОШИБКА уже подписан на " + topic;
        }
    }

    if (cmd == "UNSUB") {
        std::string topic, consumer;
        iss >> topic >> consumer;

        if (topic.empty() || consumer.empty()) {
            return "ОШИБКА использование: UNSUB <топик> <потребитель>";
        }

        bool removed = subscriptionManager.remove(topic, consumer);

        if (removed) {
            logger.info(consumer + " отписался от " + topic);
            return "OK отписан от " + topic;
        } else {
            return "ОШИБКА не подписан на " + topic;
        }
    }

    if (cmd == "LIST_TOPICS") {
        auto topics = subscriptionManager.getTopics();

        if (topics.empty()) {
            return "OK топиков нет";
        }

        std::string response = "OK топики:";
        for (auto& t : topics) {
            response += " " + t;
        }
        return response;
    }

    if (cmd == "LIST_ACTIVE_TOPICS") {
        auto topics = subscriptionManager.getActiveTopics();

        if (topics.empty()) {
            return "OK нет активных топиков";
        }

        std::string response = "OK активные топики:";
        for (auto& t : topics) {
            response += " " + t;
        }
        return response;
    }

    if (cmd == "LIST_SUBS") {
        std::string consumer;
        iss >> consumer;

        if (consumer.empty()) {
            return "ОШИБКА использование: LIST_SUBS <потребитель>";
        }

        auto subs = subscriptionManager.getSubscriptions(consumer);

        if (subs.empty()) {
            return "OK нет подписок у " + consumer;
        }

        std::string response = "OK подписки " + consumer + ":";
        for (auto& s : subs) {
            response += " " + s;
        }
        return response;
    }

    if (cmd == "USERS") {
        auto allUsers = userManager.getAllUsers();
        std::string response = "OK пользователи:";

        for (auto& u : allUsers) {
            response += " " + u;
            {
                std::lock_guard<std::mutex> lock(conn_mtx);
                if (activeConsumers.find(u) != activeConsumers.end()) {
                    response += "(онлайн)";
                } else {
                    response += "(офлайн)";
                }
            }
        }
        return response;
    }

    if (cmd == "PUB") {
        std::string topic;
        iss >> topic;

        if (topic.empty()) {
            return "ОШИБКА использование: PUB <топик> <сообщение> <приоритет>";
        }

        std::string rest;
        std::getline(iss, rest);

        while (!rest.empty() && rest.front() == ' ') {
            rest.erase(0, 1);
        }

        size_t lastSpace = rest.find_last_of(' ');
        if (lastSpace == std::string::npos) {
            return "ОШИБКА использование: PUB <топик> <сообщение> <приоритет>";
        }

        std::string body = rest.substr(0, lastSpace);
        std::string priorityStr = rest.substr(lastSpace + 1);

        int priority;
        try {
            priority = std::stoi(priorityStr);
        } catch (...) {
            return "ОШИБКА приоритет должен быть числом";
        }

        subscriptionManager.ensureTopic(topic);

        Message msg;
        msg.topic = topic;
        msg.body = body;
        msg.priority = priority;

        queue.push(msg);
        metrics.produced++;
        logger.info("Сообщение опубликовано в " + topic + " приоритет " + std::to_string(priority) );
        return "OK сообщение опубликовано в " + topic;
    }

    if (cmd == "ACK") {
        std::string consumer;
        uint64_t id;

        iss >> id >> consumer;

        if (consumer.empty()) {
            return "ОШИБКА использование: ACK <id> <потребитель>";
        }

        bool allAcked = false;

        {
            std::lock_guard<std::mutex> lock(ack_mtx);

            auto it = pendingAck.find(id);
            if (it != pendingAck.end()) {
                it->second.waitingConsumers.erase(consumer);
                logger.info("ACK " + std::to_string(id) + " от " + consumer
                          + " (осталось: " + std::to_string(it->second.waitingConsumers.size()) + ")");
                if (it->second.waitingConsumers.empty()) {
                    allAcked = true;
                }
            }
        }

        if (allAcked) {
            storage.remove(id);

            std::lock_guard<std::mutex> lock(ack_mtx);
            pendingAck.erase(id);
            logger.info("Сообщение " + std::to_string(id) + " полностью подтверждено, помечено удаленным");
        }

        metrics.consumed++;
        offsetManager.set(consumer, id);

        return "OK подтверждено " + std::to_string(id);
    }

    return "ОШИБКА неизвестная команда";
}

void Server::replayHistory(const std::string& consumerName, int client_fd) {
    auto allSubs = subscriptionManager.getAll();

    uint64_t startOffset = offsetManager.get(consumerName);
    logger.info("Восстановление истории для " + consumerName + " начиная с offset " + std::to_string(startOffset) );

    uint64_t maxId = storage.getMaxId();

    int replayedCount = 0;

    for (uint64_t id = startOffset + 1; id <= maxId; id++) {
        std::string body = storage.readMessage(id);

        if (body.empty()) continue;

        std::string msgTopic = storage.getMessageTopic(id);

        if (msgTopic.empty()) continue;

        bool subscribed = false;
        for (auto& sub : allSubs) {
            if (sub.first == msgTopic) {
                for (auto& c : sub.second) {
                    if (c == consumerName) {
                        subscribed = true;
                        break;
                    }
                }
            }
            if (subscribed) break;
        }

        if (!subscribed) continue;

        std::string out = "MSG " + std::to_string(id) + " " + msgTopic + " " + body + "\n";
        write(client_fd, out.c_str(), out.size());
        replayedCount++;

        {
            std::lock_guard<std::mutex> lock(ack_mtx);
            if (pendingAck.find(id) != pendingAck.end()) {
                pendingAck[id].waitingConsumers.insert(consumerName);
            }
        }
    }

    if (replayedCount > 0) {
        logger.info("Восстановлено " + std::to_string(replayedCount) + " сообщений для " + consumerName);
        sendResponse(client_fd, "SYS восстановлено " + std::to_string(replayedCount) + " сообщений");
    }
}

void Server::deliverMessage(const Message& msg) {
    auto consumers = subscriptionManager.get(msg.topic);

    if (consumers.empty()) return;

    std::set<std::string> waitingSet;
    for (auto& c : consumers) {
        waitingSet.insert(c);
    }

    {
        std::lock_guard<std::mutex> lock(ack_mtx);
        PendingMessage pm;
        pm.waitingConsumers = waitingSet;
        pm.topic = msg.topic;
        pm.lastRetry = std::chrono::steady_clock::now();
        pm.retryCount = 0;
        pendingAck[msg.id] = pm;
    }

    for (auto& consumer : consumers) {
        std::lock_guard<std::mutex> lock(conn_mtx);

        if (activeConsumers.find(consumer) != activeConsumers.end()) {
            int client = activeConsumers[consumer];

            std::string out = "MSG " + std::to_string(msg.id) + " " + msg.topic + " " + msg.body + "\n";
            write(client, out.c_str(), out.size());
            logger.info("Доставлено сообщение " + std::to_string(msg.id) + " пользователю " + consumer);
        } else {
            logger.info("Потребитель " + consumer + " офлайн, сообщение " + std::to_string(msg.id) + " ожидает");
        }
    }
}

void Server::consumerThread() {
    while (running) {
        Message msg = queue.pop();
        if (!running) break;

        uint64_t id = storage.store(msg.topic, msg.body);
        msg.id = id;

        deliverMessage(msg);

        metrics.stored++;
    }
}

void Server::retryThread() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!running) break;

        auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(ack_mtx);

        std::vector<uint64_t> toRetry;

        for (auto& p : pendingAck) {
            if (p.second.waitingConsumers.empty()) continue;

            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - p.second.lastRetry).count();

            if (diff > 5) {
                toRetry.push_back(p.first);
            }
        }

        for (auto& id : toRetry) {
            auto& pm = pendingAck[id];

            std::string msgBody = storage.readMessage(id);

            for (auto& consumer : pm.waitingConsumers) {
                std::lock_guard<std::mutex> connLock(conn_mtx);

                if (activeConsumers.find(consumer) != activeConsumers.end()) {
                    int client = activeConsumers[consumer];

                    std::string out = "MSG " + std::to_string(id) + " " + pm.topic + " " + msgBody + "\n";
                    write(client, out.c_str(), out.size());
                    logger.info("Повтор сообщения " + std::to_string(id) + " для " + consumer
                              + " (попытка " + std::to_string(pm.retryCount + 1) + ")");
                }
            }

            pm.lastRetry = now;
            pm.retryCount++;

            if (pm.retryCount > 10) {
                logger.info("Сообщение " + std::to_string(id) + " превысило лимит попыток, удаляется");
                storage.remove(id);
                pendingAck.erase(id);
            }
        }
    }
}

void Server::compactThread() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::minutes(10));
        if (!running) break;
        logger.info("Запуск периодического компактирования...");
        storage.compact();
        logger.info("Периодическое компактирование завершено");
    }
}
