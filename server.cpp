#include "crow_all.h"
#include <sw/redis++/redis++.h>
#include <sqlite3.h>
#include <bcrypt/bcrypt.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <string>
#include <thread>
#include <random>
#include <fstream>
#include <algorithm>
#include <memory>

using namespace std;
using namespace sw::redis;

struct ChatUser {
    string username;
    crow::websocket::connection* conn;
};

vector<ChatUser> active_users;
mutex users_mutex;
sqlite3* db;
mutex db_mutex;

unique_ptr<Redis> redis;

string generate_token() {
    string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
    string token;
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> distribution(0, chars.size() - 1);
    for (int i = 0; i < 32; ++i) token += chars[distribution(generator)];
    return token;
}

void init_database() {
    char* errMsg = nullptr;
    sqlite3_open("chat_enterprise.db", &db);
    string sql_users = "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password_hash TEXT NOT NULL, token TEXT);";
    string sql_messages = "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT, message TEXT NOT NULL, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    sqlite3_exec(db, sql_users.c_str(), nullptr, nullptr, &errMsg);
    sqlite3_exec(db, sql_messages.c_str(), nullptr, nullptr, &errMsg);
}

bool register_user(const string& user, const string& pass) {
    lock_guard<mutex> lock(db_mutex);
    string hash = bcrypt::generateHash(pass);
    string sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
    return false;
}

string login_user(const string& user, const string& pass) {
    lock_guard<mutex> lock(db_mutex);
    string sql = "SELECT password_hash FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;
    string token = "";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            string hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (bcrypt::validatePassword(pass, hash)) {
                token = generate_token();
                string update_sql = "UPDATE users SET token = ? WHERE username = ?;";
                sqlite3_stmt* u_stmt;
                if (sqlite3_prepare_v2(db, update_sql.c_str(), -1, &u_stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(u_stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(u_stmt, 2, user.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(u_stmt);
                    sqlite3_finalize(u_stmt);
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    return token;
}

string verify_token(const string& token) {
    lock_guard<mutex> lock(db_mutex);
    string sql = "SELECT username FROM users WHERE token = ?;";
    sqlite3_stmt* stmt;
    string username = "";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
    }
    return username;
}

void save_message_to_db(const string& user, const string& msg) {
    lock_guard<mutex> lock(db_mutex);
    string sql = "INSERT INTO messages (username, message) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, msg.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
}

void send_history_to_user(crow::websocket::connection& conn) {
    lock_guard<mutex> lock(db_mutex);
    string sql = "SELECT username, message FROM (SELECT id, username, message FROM messages ORDER BY id DESC LIMIT 50) ORDER BY id ASC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            string msg = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            conn.send_text(user + ": " + msg);
        }
    }
    sqlite3_finalize(stmt);
}

void local_broadcast(const string& message) {
    lock_guard<mutex> lock(users_mutex);
    for (auto& u : active_users) u.conn->send_text(message);
}

void redis_listener() {
    while (true) {
        try {
            auto sub = redis->subscriber();
            sub.on_message([](string channel, string msg) { local_broadcast(msg); });
            sub.subscribe("chat_channel");
            while (true) { sub.consume(); }
        } catch (...) {
            this_thread::sleep_for(chrono::seconds(2));
        }
    }
}

int main() {
    // Fest verdrahtet auf deine Ziel-IP für Redis
    string redis_host = "172.16.130.85"; 

    try {
        ConnectionOptions opts;
        opts.host = redis_host;
        opts.port = 6379;
        opts.password = "MeinSicheresNetzwerkPasswort"; 
        opts.socket_timeout = std::chrono::milliseconds(3000);
        
        redis = make_unique<Redis>(opts);
        redis->ping();
        cout << "[Redis] Erfolgreich verbunden mit: " << redis_host << " auf Port 6379" << endl;
    } catch (const std::exception& e) {
        cerr << "[CRITICAL] Redis auf " << redis_host << " nicht erreichbar: " << e.what() << endl;
        return 1;
    }

    init_database();
    mkdir("./uploads", 0777);
    crow::SimpleApp app;

    thread listener_thread(redis_listener);
    listener_thread.detach();

    CROW_ROUTE(app, "/")([]() { return crow::mustache::load_text("index.html"); });

    CROW_ROUTE(app, "/uploads/<string>")([](const crow::request&, crow::response& res, string filename) {
        res.set_static_file_info("./uploads/" + filename);
        res.end();
    });

    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x || !x.has("username") || !x.has("password")) return crow::response(400, "Fehler");
        if (register_user(x["username"].s(), x["password"].s())) return crow::response(200, "OK");
        return crow::response(400, "Existiert bereits");
    });

    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        string token = login_user(x["username"].s(), x["password"].s());
        if (!token.empty()) {
            crow::json::wvalue res; res["token"] = token; return crow::response(res);
        }
        return crow::response(401, "Ungültig");
    });

    CROW_ROUTE(app, "/chat").websocket()
        .onopen([&](crow::websocket::connection& conn) {
            string token = conn.get_query_filter().get("token") ? conn.get_query_filter().get("token") : "";
            string username = verify_token(token);
            if (username.empty()) { conn.close("Unauthorized"); return; }
            send_history_to_user(conn);
            { lock_guard<mutex> lock(users_mutex); active_users.push_back({username, &conn}); }
            redis->publish("chat_channel", "System: " + username + " ist online.");
        })
        .onclose([&](crow::websocket::connection& conn, const string&) {
            string username = "";
            {
                lock_guard<mutex> lock(users_mutex);
                auto it = find_if(active_users.begin(), active_users.end(), [&](const ChatUser& u) { return u.conn == &conn; });
                if (it != active_users.end()) { username = it->username; active_users.erase(it); }
            }
            if (!username.empty()) redis->publish("chat_channel", "System: " + username + " ist offline.");
        })
        .onmessage([&](crow::websocket::connection& conn, const string& data, bool is_binary) {
            string username = "";
            {
                lock_guard<mutex> lock(users_mutex);
                auto it = find_if(active_users.begin(), active_users.end(), [&](const ChatUser& u) { return u.conn == &conn; });
                if (it != active_users.end()) username = it->username;
            }
            if (username.empty()) return;

            if (is_binary) {
                string filename = generate_token() + ".jpg";
                ofstream outfile("./uploads/" + filename, ios::out | ios::binary);
                outfile.write(data.data(), data.size()); outfile.close();
                string image_html = "<img src='/uploads/" + filename + "' style='max-width:300px; border-radius:8px;' />";
                save_message_to_db(username, image_html);
                redis->publish("chat_channel", username + ": " + image_html);
            } else {
                save_message_to_db(username, data);
                redis->publish("chat_channel", username + ": " + data);
            }
        });

    app.ssl_file("cert.pem", "key.pem");
    app.bindaddr("::"); // Dual-Stack hört auf alle lokalen IPs inklusive 172.16.130.85

    const char* port_env = getenv("PORT");
    int port = port_env ? stoi(port_env) : 443;
    app.port(port).multithreaded().run();
    sqlite3_close(db);
    return 0;
}
