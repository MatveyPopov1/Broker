#ifndef USER_H
#define USER_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <openssl/sha.h>

class UserManager {
private:
    struct User {
        std::string username;
        std::string salt;
        std::string passwordHash;
    };

    std::map<std::string, User> users;
    std::mutex mtx;

    std::string generateSalt() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        const char* hex = "0123456789abcdef";
        std::string salt;
        for (int i = 0; i < 16; i++) {
            salt += hex[dis(gen)];
        }
        return salt;
    }

    std::string hashPassword(const std::string& password, const std::string& salt) {
        std::string combined = salt + password;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char*)combined.c_str(), combined.size(), hash);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    void save() {
        std::ofstream file("users.dat");
        for (auto& p : users) {
            file << p.second.username << " " << p.second.salt << " " << p.second.passwordHash << "\n";
        }
    }

public:
    void load() {
        std::lock_guard<std::mutex> lock(mtx);
        std::ifstream file("users.dat");

        std::string username, salt, hash;
        while (file >> username >> salt >> hash) {
            User u;
            u.username = username;
            u.salt = salt;
            u.passwordHash = hash;
            users[username] = u;
        }
    }

    bool registerUser(const std::string& username, const std::string& password) {
        std::lock_guard<std::mutex> lock(mtx);

        if (users.find(username) != users.end()) {
            return false;
        }

        User u;
        u.username = username;
        u.salt = generateSalt();
        u.passwordHash = hashPassword(password, u.salt);
        users[username] = u;

        save();
        return true;
    }

    bool authenticate(const std::string& username, const std::string& password) {
        std::lock_guard<std::mutex> lock(mtx);

        auto it = users.find(username);
        if (it == users.end()) {
            return false;
        }

        std::string hash = hashPassword(password, it->second.salt);
        return hash == it->second.passwordHash;
    }

    bool userExists(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        return users.find(username) != users.end();
    }

    std::vector<std::string> getAllUsers() {
        std::lock_guard<std::mutex> lock(mtx);

        std::vector<std::string> result;
        for (auto& p : users) {
            result.push_back(p.first);
        }
        return result;
    }
};

#endif
