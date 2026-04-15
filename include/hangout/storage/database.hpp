#pragma once

#include <string>

struct sqlite3;

namespace hangout {

class Database {
public:
    explicit Database(std::string path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void initialize();
    sqlite3* connection() const noexcept { return db_; }

private:
    void open();
    void migrate();
    void seed_demo_data();
    void execute(const std::string& sql);

    std::string path_;
    sqlite3* db_ {};
};

}  // namespace hangout
