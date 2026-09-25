#include "Database.hpp"

Database::Database(const std::string& path, bool readOnly) {
    int flags = readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    sqlite3_busy_timeout(db_, 2000);
    if (!readOnly) exec("PRAGMA foreign_keys = ON");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

bool Database::exec(const std::string& sql) {
    return db_ && sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::string Database::lastError() const {
    return db_ ? sqlite3_errmsg(db_) : "database is not open";
}

long long Database::lastInsertId() const {
    return db_ ? sqlite3_last_insert_rowid(db_) : 0;
}

int Database::userVersion() {
    Statement query(*this, "PRAGMA user_version");
    return query.step() ? static_cast<int>(query.integer(0)) : 0;
}

void Database::setUserVersion(int version) {
    exec("PRAGMA user_version = " + std::to_string(version));
}

Statement::Statement(Database& database, const std::string& sql) {
    if (!database.isOpen()) return;
    if (sqlite3_prepare_v2(database.handle(), sql.c_str(), -1, &statement_, nullptr) != SQLITE_OK) {
        statement_ = nullptr;
    }
}

Statement::~Statement() {
    if (statement_) sqlite3_finalize(statement_);
}

Statement& Statement::bind(int index, const std::string& value) {
    if (statement_) sqlite3_bind_text(statement_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    return *this;
}

Statement& Statement::bind(int index, long long value) {
    if (statement_) sqlite3_bind_int64(statement_, index, value);
    return *this;
}

Statement& Statement::bind(int index, double value) {
    if (statement_) sqlite3_bind_double(statement_, index, value);
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (statement_) sqlite3_bind_null(statement_, index);
    return *this;
}

Statement& Statement::bindBlob(int index, const std::string& bytes) {
    if (statement_) sqlite3_bind_blob(statement_, index, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
    return *this;
}

bool Statement::step() {
    return statement_ && sqlite3_step(statement_) == SQLITE_ROW;
}

bool Statement::run() {
    if (!statement_) return false;
    int status = sqlite3_step(statement_);
    return status == SQLITE_DONE || status == SQLITE_ROW;
}

void Statement::reset() {
    if (statement_) {
        sqlite3_reset(statement_);
        sqlite3_clear_bindings(statement_);
    }
}

std::string Statement::text(int column) const {
    const unsigned char* value = statement_ ? sqlite3_column_text(statement_, column) : nullptr;
    return value ? reinterpret_cast<const char*>(value) : "";
}

long long Statement::integer(int column) const {
    return statement_ ? sqlite3_column_int64(statement_, column) : 0;
}

double Statement::real(int column) const {
    return statement_ ? sqlite3_column_double(statement_, column) : 0;
}

std::string Statement::blob(int column) const {
    if (!statement_) return "";
    const void* bytes = sqlite3_column_blob(statement_, column);
    int size = sqlite3_column_bytes(statement_, column);
    return bytes ? std::string(static_cast<const char*>(bytes), size) : "";
}

bool Statement::isNull(int column) const {
    return !statement_ || sqlite3_column_type(statement_, column) == SQLITE_NULL;
}
