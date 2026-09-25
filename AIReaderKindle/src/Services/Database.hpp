#pragma once

#include <sqlite3.h>

#include <string>

/// A thin wrapper around one SQLite connection.
class Database {
public:
    explicit Database(const std::string& path, bool readOnly = false);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool isOpen() const { return db_ != nullptr; }
    sqlite3* handle() const { return db_; }
    bool exec(const std::string& sql);
    std::string lastError() const;
    long long lastInsertId() const;
    int userVersion();
    void setUserVersion(int version);

private:
    sqlite3* db_ = nullptr;
};

/// A prepared statement that binds by position and reads columns by index.
class Statement {
public:
    Statement(Database& database, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool isValid() const { return statement_ != nullptr; }
    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, long long value);
    Statement& bind(int index, int value) { return bind(index, static_cast<long long>(value)); }
    Statement& bind(int index, double value);
    Statement& bindNull(int index);
    Statement& bindBlob(int index, const std::string& bytes);

    /// True while a row is available.
    bool step();
    /// Runs a statement that returns no rows.
    bool run();
    void reset();

    std::string text(int column) const;
    long long integer(int column) const;
    double real(int column) const;
    std::string blob(int column) const;
    bool isNull(int column) const;

private:
    sqlite3_stmt* statement_ = nullptr;
};
