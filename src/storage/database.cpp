#include "hangout/storage/database.hpp"

#include <filesystem>
#include <random>
#include <string>

#include <sqlite3.h>

#include "hangout/domain/errors.hpp"

namespace hangout {
namespace {

void throw_if_sqlite_error(int code, sqlite3* db, const std::string& context) {
    if (code == SQLITE_OK || code == SQLITE_DONE || code == SQLITE_ROW) {
        return;
    }
    throw StorageError(context + ": " + sqlite3_errmsg(db));
}

bool column_exists(sqlite3* db, const std::string& table, const std::string& column) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql = "PRAGMA table_info(" + table + ");";
    throw_if_sqlite_error(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr),
                          db,
                          "Failed to inspect table");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* raw_name = sqlite3_column_text(stmt, 1);
        const std::string name = raw_name == nullptr ? "" : reinterpret_cast<const char*>(raw_name);
        if (name == column) {
            sqlite3_finalize(stmt);
            return true;
        }
    }

    sqlite3_finalize(stmt);
    return false;
}

std::string generate_invite_code() {
    static thread_local std::mt19937_64 engine(std::random_device {}());
    static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::uniform_int_distribution<std::size_t> dist(0, sizeof(alphabet) - 2);

    std::string code;
    code.reserve(8);
    for (int i = 0; i < 8; ++i) {
        code.push_back(alphabet[dist(engine)]);
    }
    return code;
}

}  // namespace

Database::Database(std::string path) : path_(std::move(path)) {}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

void Database::initialize() {
    open();
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");
    migrate();
    seed_demo_data();
}

void Database::open() {
    const std::filesystem::path db_path(path_);
    if (db_path.has_parent_path()) {
        std::filesystem::create_directories(db_path.parent_path());
    }

    const int rc = sqlite3_open(path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string message = "Failed to open database";
        if (db_ != nullptr) {
            message += ": ";
            message += sqlite3_errmsg(db_);
        }
        throw StorageError(message);
    }
}

void Database::execute(const std::string& sql) {
    char* error_message = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_message);
    if (rc != SQLITE_OK) {
        std::string message = "SQLite exec failed";
        if (error_message != nullptr) {
            message += ": ";
            message += error_message;
            sqlite3_free(error_message);
        }
        throw StorageError(message);
    }
}

void Database::migrate() {
    execute(R"sql(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            display_name TEXT,
            status_text TEXT,
            user_status TEXT NOT NULL DEFAULT 'online',
            avatar_color TEXT NOT NULL DEFAULT '#c315d2',
            last_seen_at_ms INTEGER NOT NULL DEFAULT 0,
            created_at_ms INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            token TEXT NOT NULL UNIQUE,
            created_at_ms INTEGER NOT NULL,
            expires_at_ms INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            invite_code TEXT UNIQUE,
            created_at_ms INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS server_memberships (
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            server_id INTEGER NOT NULL REFERENCES servers(id) ON DELETE CASCADE,
            role TEXT NOT NULL DEFAULT 'member',
            joined_at_ms INTEGER NOT NULL,
            PRIMARY KEY(user_id, server_id)
        );

        CREATE TABLE IF NOT EXISTS channels (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            server_id INTEGER NOT NULL REFERENCES servers(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            position INTEGER NOT NULL DEFAULT 0,
            created_at_ms INTEGER NOT NULL,
            UNIQUE(server_id, name)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            channel_id INTEGER NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
            author_id INTEGER NOT NULL REFERENCES users(id),
            content TEXT NOT NULL,
            edited_at_ms INTEGER,
            created_at_ms INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS message_reactions (
            message_id INTEGER NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            emoji TEXT NOT NULL,
            created_at_ms INTEGER NOT NULL,
            PRIMARY KEY(message_id, user_id, emoji)
        );

        CREATE TABLE IF NOT EXISTS friend_requests (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            requester_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            addressee_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            status TEXT NOT NULL CHECK(status IN ('pending', 'accepted')),
            created_at_ms INTEGER NOT NULL,
            responded_at_ms INTEGER,
            CHECK(requester_id <> addressee_id),
            UNIQUE(requester_id, addressee_id)
        );

        CREATE TABLE IF NOT EXISTS direct_conversations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            created_at_ms INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS direct_participants (
            conversation_id INTEGER NOT NULL REFERENCES direct_conversations(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            PRIMARY KEY(conversation_id, user_id)
        );

        CREATE TABLE IF NOT EXISTS direct_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL REFERENCES direct_conversations(id) ON DELETE CASCADE,
            author_id INTEGER NOT NULL REFERENCES users(id),
            content TEXT NOT NULL,
            edited_at_ms INTEGER,
            created_at_ms INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS typing_statuses (
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            target_type TEXT NOT NULL CHECK(target_type IN ('channel', 'dm')),
            target_id INTEGER NOT NULL,
            expires_at_ms INTEGER NOT NULL,
            PRIMARY KEY(user_id, target_type, target_id)
        );

        CREATE TABLE IF NOT EXISTS direct_message_reactions (
            message_id INTEGER NOT NULL REFERENCES direct_messages(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            emoji TEXT NOT NULL,
            created_at_ms INTEGER NOT NULL,
            PRIMARY KEY(message_id, user_id, emoji)
        );

        CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
        CREATE INDEX IF NOT EXISTS idx_channels_server_id ON channels(server_id);
        CREATE INDEX IF NOT EXISTS idx_messages_channel_created_at
            ON messages(channel_id, created_at_ms DESC, id DESC);
        CREATE INDEX IF NOT EXISTS idx_message_reactions_message ON message_reactions(message_id);
        CREATE INDEX IF NOT EXISTS idx_friend_requests_requester ON friend_requests(requester_id, status);
        CREATE INDEX IF NOT EXISTS idx_friend_requests_addressee ON friend_requests(addressee_id, status);
        CREATE INDEX IF NOT EXISTS idx_direct_participants_user ON direct_participants(user_id);
        CREATE INDEX IF NOT EXISTS idx_direct_messages_conversation_created_at
            ON direct_messages(conversation_id, created_at_ms DESC, id DESC);
        CREATE INDEX IF NOT EXISTS idx_direct_message_reactions_message ON direct_message_reactions(message_id);
        CREATE INDEX IF NOT EXISTS idx_typing_statuses_target
            ON typing_statuses(target_type, target_id, expires_at_ms);
    )sql");

    if (!column_exists(db_, "users", "display_name")) {
        execute("ALTER TABLE users ADD COLUMN display_name TEXT;");
    }
    if (!column_exists(db_, "users", "status_text")) {
        execute("ALTER TABLE users ADD COLUMN status_text TEXT;");
    }
    if (!column_exists(db_, "users", "user_status")) {
        execute("ALTER TABLE users ADD COLUMN user_status TEXT NOT NULL DEFAULT 'online';");
    }
    if (!column_exists(db_, "users", "avatar_color")) {
        execute("ALTER TABLE users ADD COLUMN avatar_color TEXT NOT NULL DEFAULT '#c315d2';");
    }
    if (!column_exists(db_, "users", "last_seen_at_ms")) {
        execute("ALTER TABLE users ADD COLUMN last_seen_at_ms INTEGER NOT NULL DEFAULT 0;");
    }
    execute("UPDATE users SET display_name = username WHERE display_name IS NULL OR display_name = '';");

    if (!column_exists(db_, "server_memberships", "role")) {
        execute("ALTER TABLE server_memberships ADD COLUMN role TEXT NOT NULL DEFAULT 'member';");
    }
    if (!column_exists(db_, "channels", "position")) {
        execute("ALTER TABLE channels ADD COLUMN position INTEGER NOT NULL DEFAULT 0;");
    }
    if (!column_exists(db_, "messages", "edited_at_ms")) {
        execute("ALTER TABLE messages ADD COLUMN edited_at_ms INTEGER;");
    }
    if (!column_exists(db_, "direct_messages", "edited_at_ms")) {
        execute("ALTER TABLE direct_messages ADD COLUMN edited_at_ms INTEGER;");
    }

    if (!column_exists(db_, "servers", "invite_code")) {
        execute("ALTER TABLE servers ADD COLUMN invite_code TEXT;");
    }

    sqlite3_stmt* stmt = nullptr;
    throw_if_sqlite_error(sqlite3_prepare_v2(db_,
                                             "SELECT id FROM servers WHERE invite_code IS NULL OR invite_code = '';",
                                             -1,
                                             &stmt,
                                             nullptr),
                          db_,
                          "Failed to prepare invite backfill");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto server_id = sqlite3_column_int64(stmt, 0);
        bool updated = false;
        while (!updated) {
            sqlite3_stmt* update_stmt = nullptr;
            throw_if_sqlite_error(sqlite3_prepare_v2(db_,
                                                     "UPDATE servers SET invite_code = ? WHERE id = ?;",
                                                     -1,
                                                     &update_stmt,
                                                     nullptr),
                                  db_,
                                  "Failed to prepare invite update");
            const auto code = generate_invite_code();
            sqlite3_bind_text(update_stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(update_stmt, 2, server_id);
            const int rc = sqlite3_step(update_stmt);
            sqlite3_finalize(update_stmt);
            if (rc == SQLITE_DONE) {
                updated = true;
            } else if (rc != SQLITE_CONSTRAINT) {
                throw_if_sqlite_error(rc, db_, "Failed to update invite code");
            }
        }
    }

    sqlite3_finalize(stmt);

    execute("CREATE UNIQUE INDEX IF NOT EXISTS idx_servers_invite_code ON servers(invite_code);");
}

void Database::seed_demo_data() {
    sqlite3_stmt* count_stmt = nullptr;
    throw_if_sqlite_error(sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM servers;", -1, &count_stmt, nullptr),
                          db_,
                          "Failed to prepare seed count");

    const int step_code = sqlite3_step(count_stmt);
    throw_if_sqlite_error(step_code, db_, "Failed to read seed count");
    const auto server_count = sqlite3_column_int64(count_stmt, 0);
    sqlite3_finalize(count_stmt);

    if (server_count > 0) {
        return;
    }

    execute(R"sql(
        INSERT INTO users (username, display_name, status_text, user_status, avatar_color, last_seen_at_ms, created_at_ms)
        VALUES ('system', 'system', 'Prototype helper', 'online', '#c315d2', strftime('%s','now') * 1000, strftime('%s','now') * 1000);

        INSERT INTO servers (name, invite_code, created_at_ms) VALUES ('Hangout', 'HANGOUT1', strftime('%s','now') * 1000);

        INSERT INTO server_memberships (user_id, server_id, role, joined_at_ms)
        SELECT users.id, servers.id, 'owner', strftime('%s','now') * 1000
        FROM users, servers
        WHERE users.username = 'system' AND servers.name = 'Hangout';

        INSERT INTO channels (server_id, name, position, created_at_ms)
        SELECT id, 'general', 0, strftime('%s','now') * 1000 FROM servers WHERE name = 'Hangout';

        INSERT INTO channels (server_id, name, position, created_at_ms)
        SELECT id, 'backend-lab', 1, strftime('%s','now') * 1000 FROM servers WHERE name = 'Hangout';

        INSERT INTO messages (channel_id, author_id, content, created_at_ms)
        SELECT channels.id, users.id, 'Welcome to Hangout. This backend is ready for an Electron client.',
               strftime('%s','now') * 1000
        FROM channels, users
        WHERE channels.name = 'general' AND users.username = 'system';
    )sql");
}

}  // namespace hangout
