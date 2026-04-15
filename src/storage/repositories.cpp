#include "hangout/storage/repositories.hpp"

#include <algorithm>

#include <sqlite3.h>

#include "hangout/domain/errors.hpp"

namespace hangout {
namespace {

class Statement final {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        const int rc = sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr);
        if (rc != SQLITE_OK) {
            throw StorageError(std::string("Failed to prepare statement: ") + sqlite3_errmsg(db_));
        }
    }

    ~Statement() {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    sqlite3_stmt* get() const noexcept { return statement_; }

private:
    sqlite3* db_;
    sqlite3_stmt* statement_ {};
};

void check_sqlite_step(int code, sqlite3* db, const std::string& context) {
    if (code == SQLITE_DONE || code == SQLITE_ROW) {
        return;
    }
    if (code == SQLITE_CONSTRAINT) {
        throw ValidationError(context + ": name already exists.");
    }
    throw StorageError(context + ": " + sqlite3_errmsg(db));
}

void bind_int64(sqlite3* db, sqlite3_stmt* stmt, int index, std::int64_t value) {
    const int rc = sqlite3_bind_int64(stmt, index, value);
    if (rc != SQLITE_OK) {
        throw StorageError(std::string("Failed to bind integer: ") + sqlite3_errmsg(db));
    }
}

void bind_text(sqlite3* db, sqlite3_stmt* stmt, int index, const std::string& value) {
    const int rc = sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw StorageError(std::string("Failed to bind text: ") + sqlite3_errmsg(db));
    }
}

std::string column_text(sqlite3_stmt* stmt, int column) {
    const auto* raw = sqlite3_column_text(stmt, column);
    return raw == nullptr ? std::string {} : reinterpret_cast<const char*>(raw);
}

std::optional<User> read_user(sqlite3_stmt* stmt) {
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        return std::nullopt;
    }

    return User {
        .id = sqlite3_column_int64(stmt, 0),
        .username = column_text(stmt, 1),
        .display_name = column_text(stmt, 2),
        .status_text = column_text(stmt, 3),
        .user_status = column_text(stmt, 4),
        .avatar_color = column_text(stmt, 5),
        .last_seen_at_ms = sqlite3_column_int64(stmt, 6),
        .created_at_ms = sqlite3_column_int64(stmt, 7),
    };
}

std::vector<ReactionSummary> list_reactions_for_table(sqlite3* db,
                                                      const char* table_name,
                                                      std::int64_t message_id,
                                                      std::int64_t user_id) {
    const std::string sql =
        std::string("SELECT emoji, COUNT(*), SUM(CASE WHEN user_id = ? THEN 1 ELSE 0 END) ") +
        "FROM " + table_name + " WHERE message_id = ? GROUP BY emoji ORDER BY COUNT(*) DESC, emoji ASC;";
    Statement stmt(db, sql.c_str());
    bind_int64(db, stmt.get(), 1, user_id);
    bind_int64(db, stmt.get(), 2, message_id);

    std::vector<ReactionSummary> reactions;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db, "Failed to read reactions");
        reactions.push_back(ReactionSummary {
            .emoji = column_text(stmt.get(), 0),
            .count = sqlite3_column_int(stmt.get(), 1),
            .reacted_by_me = sqlite3_column_int(stmt.get(), 2) > 0,
        });
    }
    return reactions;
}

std::vector<ReactionSummary> toggle_reaction_for_table(sqlite3* db,
                                                       const char* table_name,
                                                       std::int64_t message_id,
                                                       std::int64_t user_id,
                                                       const std::string& emoji) {
    {
        const std::string delete_sql =
            std::string("DELETE FROM ") + table_name + " WHERE message_id = ? AND user_id = ? AND emoji = ?;";
        Statement stmt(db, delete_sql.c_str());
        bind_int64(db, stmt.get(), 1, message_id);
        bind_int64(db, stmt.get(), 2, user_id);
        bind_text(db, stmt.get(), 3, emoji);
        check_sqlite_step(sqlite3_step(stmt.get()), db, "Failed to remove reaction");
    }

    if (sqlite3_changes(db) == 0) {
        const std::string insert_sql =
            std::string("INSERT INTO ") + table_name +
            " (message_id, user_id, emoji, created_at_ms) VALUES (?, ?, ?, strftime('%s','now') * 1000);";
        Statement stmt(db, insert_sql.c_str());
        bind_int64(db, stmt.get(), 1, message_id);
        bind_int64(db, stmt.get(), 2, user_id);
        bind_text(db, stmt.get(), 3, emoji);
        check_sqlite_step(sqlite3_step(stmt.get()), db, "Failed to add reaction");
    }

    return list_reactions_for_table(db, table_name, message_id, user_id);
}

}  // namespace

std::optional<User> UserRepository::find_by_id(std::int64_t user_id) const {
    Statement stmt(db_,
                   "SELECT id, username, COALESCE(display_name, username), COALESCE(status_text, ''), "
                   "COALESCE(user_status, 'online'), COALESCE(avatar_color, '#c315d2'), "
                   "COALESCE(last_seen_at_ms, 0), created_at_ms FROM users WHERE id = ?;");
    bind_int64(db_, stmt.get(), 1, user_id);
    return read_user(stmt.get());
}

std::optional<User> UserRepository::find_by_username(const std::string& username) const {
    Statement stmt(db_,
                   "SELECT id, username, COALESCE(display_name, username), COALESCE(status_text, ''), "
                   "COALESCE(user_status, 'online'), COALESCE(avatar_color, '#c315d2'), "
                   "COALESCE(last_seen_at_ms, 0), created_at_ms FROM users WHERE username = ?;");
    bind_text(db_, stmt.get(), 1, username);
    return read_user(stmt.get());
}

User UserRepository::create(const std::string& username, std::int64_t created_at_ms) const {
    Statement stmt(db_,
                   "INSERT INTO users (username, display_name, last_seen_at_ms, created_at_ms) "
                   "VALUES (?, ?, ?, ?);");
    bind_text(db_, stmt.get(), 1, username);
    bind_text(db_, stmt.get(), 2, username);
    bind_int64(db_, stmt.get(), 3, created_at_ms);
    bind_int64(db_, stmt.get(), 4, created_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to insert user");

    return User {
        .id = sqlite3_last_insert_rowid(db_),
        .username = username,
        .display_name = username,
        .user_status = "online",
        .avatar_color = "#c315d2",
        .last_seen_at_ms = created_at_ms,
        .created_at_ms = created_at_ms,
    };
}

User UserRepository::update_profile(std::int64_t user_id,
                                    const std::string& display_name,
                                    const std::string& status_text,
                                    const std::string& user_status,
                                    const std::string& avatar_color,
                                    std::int64_t now_ms) const {
    Statement stmt(db_,
                   "UPDATE users SET display_name = ?, status_text = ?, user_status = ?, "
                   "avatar_color = ?, last_seen_at_ms = ? WHERE id = ?;");
    bind_text(db_, stmt.get(), 1, display_name);
    bind_text(db_, stmt.get(), 2, status_text);
    bind_text(db_, stmt.get(), 3, user_status);
    bind_text(db_, stmt.get(), 4, avatar_color);
    bind_int64(db_, stmt.get(), 5, now_ms);
    bind_int64(db_, stmt.get(), 6, user_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to update profile");
    auto user = find_by_id(user_id);
    if (!user.has_value()) {
        throw StorageError("Failed to reload profile.");
    }
    return *user;
}

void UserRepository::heartbeat(std::int64_t user_id, std::int64_t now_ms) const {
    Statement stmt(db_, "UPDATE users SET last_seen_at_ms = ? WHERE id = ?;");
    bind_int64(db_, stmt.get(), 1, now_ms);
    bind_int64(db_, stmt.get(), 2, user_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to update presence");
}

Session SessionRepository::create(std::int64_t user_id,
                                  const std::string& token,
                                  std::int64_t created_at_ms,
                                  std::int64_t expires_at_ms) const {
    Statement stmt(
        db_,
        "INSERT INTO sessions (user_id, token, created_at_ms, expires_at_ms) VALUES (?, ?, ?, ?);");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_text(db_, stmt.get(), 2, token);
    bind_int64(db_, stmt.get(), 3, created_at_ms);
    bind_int64(db_, stmt.get(), 4, expires_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to insert session");

    return Session {
        .id = sqlite3_last_insert_rowid(db_),
        .user_id = user_id,
        .token = token,
        .created_at_ms = created_at_ms,
        .expires_at_ms = expires_at_ms,
    };
}

std::optional<Session> SessionRepository::find_valid_by_token(const std::string& token,
                                                              std::int64_t now_ms) const {
    Statement stmt(db_,
                   "SELECT id, user_id, token, created_at_ms, expires_at_ms "
                   "FROM sessions WHERE token = ? AND expires_at_ms > ?;");
    bind_text(db_, stmt.get(), 1, token);
    bind_int64(db_, stmt.get(), 2, now_ms);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    return Session {
        .id = sqlite3_column_int64(stmt.get(), 0),
        .user_id = sqlite3_column_int64(stmt.get(), 1),
        .token = column_text(stmt.get(), 2),
        .created_at_ms = sqlite3_column_int64(stmt.get(), 3),
        .expires_at_ms = sqlite3_column_int64(stmt.get(), 4),
    };
}

std::vector<ServerSummary> ServerRepository::list_for_user(std::int64_t user_id) const {
    Statement stmt(
        db_,
        "SELECT s.id, s.name "
        "FROM servers s "
        "JOIN server_memberships m ON m.server_id = s.id "
        "WHERE m.user_id = ? "
        "ORDER BY s.name ASC;");
    bind_int64(db_, stmt.get(), 1, user_id);

    std::vector<ServerSummary> servers;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to read servers");
        servers.push_back(ServerSummary {
            .id = sqlite3_column_int64(stmt.get(), 0),
            .name = column_text(stmt.get(), 1),
        });
    }
    return servers;
}

bool ServerRepository::is_member(std::int64_t user_id, std::int64_t server_id) const {
    Statement stmt(db_,
                   "SELECT 1 FROM server_memberships WHERE user_id = ? AND server_id = ? LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, server_id);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

void ServerRepository::add_user_to_all_servers(std::int64_t user_id, std::int64_t joined_at_ms) const {
    Statement stmt(db_,
                   "INSERT OR IGNORE INTO server_memberships (user_id, server_id, joined_at_ms) "
                   "SELECT ?, id, ? FROM servers;");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, joined_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to add user to servers");
}

ServerSummary ServerRepository::create_for_user(std::int64_t user_id,
                                                const std::string& name,
                                                const std::string& invite_code,
                                                std::int64_t created_at_ms) const {
    Statement server_stmt(db_, "INSERT INTO servers (name, invite_code, created_at_ms) VALUES (?, ?, ?);");
    bind_text(db_, server_stmt.get(), 1, name);
    bind_text(db_, server_stmt.get(), 2, invite_code);
    bind_int64(db_, server_stmt.get(), 3, created_at_ms);
    check_sqlite_step(sqlite3_step(server_stmt.get()), db_, "Failed to insert server");

    const auto server_id = sqlite3_last_insert_rowid(db_);

    Statement membership_stmt(
        db_,
        "INSERT INTO server_memberships (user_id, server_id, role, joined_at_ms) VALUES (?, ?, 'owner', ?);");
    bind_int64(db_, membership_stmt.get(), 1, user_id);
    bind_int64(db_, membership_stmt.get(), 2, server_id);
    bind_int64(db_, membership_stmt.get(), 3, created_at_ms);
    check_sqlite_step(sqlite3_step(membership_stmt.get()), db_, "Failed to add server membership");

    Statement channel_stmt(
        db_,
        "INSERT INTO channels (server_id, name, created_at_ms) VALUES (?, 'general', ?);");
    bind_int64(db_, channel_stmt.get(), 1, server_id);
    bind_int64(db_, channel_stmt.get(), 2, created_at_ms);
    check_sqlite_step(sqlite3_step(channel_stmt.get()), db_, "Failed to create default channel");

    return ServerSummary {
        .id = server_id,
        .name = name,
    };
}

std::vector<ServerMember> ServerRepository::list_members(std::int64_t user_id, std::int64_t server_id) const {
    if (!is_member(user_id, server_id)) {
        return {};
    }

    Statement stmt(db_,
                   "SELECT u.id, u.username, COALESCE(u.display_name, u.username), "
                   "COALESCE(u.status_text, ''), COALESCE(u.user_status, 'online'), "
                   "COALESCE(u.avatar_color, '#c315d2'), COALESCE(u.last_seen_at_ms, 0), "
                   "u.created_at_ms, m.role, m.joined_at_ms "
                   "FROM server_memberships m "
                   "JOIN users u ON u.id = m.user_id "
                   "WHERE m.server_id = ? ORDER BY m.role DESC, u.username ASC;");
    bind_int64(db_, stmt.get(), 1, server_id);

    std::vector<ServerMember> members;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to read server members");
        members.push_back(ServerMember {
            .user = User {
                .id = sqlite3_column_int64(stmt.get(), 0),
                .username = column_text(stmt.get(), 1),
                .display_name = column_text(stmt.get(), 2),
                .status_text = column_text(stmt.get(), 3),
                .user_status = column_text(stmt.get(), 4),
                .avatar_color = column_text(stmt.get(), 5),
                .last_seen_at_ms = sqlite3_column_int64(stmt.get(), 6),
                .created_at_ms = sqlite3_column_int64(stmt.get(), 7),
            },
            .role = column_text(stmt.get(), 8),
            .joined_at_ms = sqlite3_column_int64(stmt.get(), 9),
        });
    }
    return members;
}

std::string ServerRepository::role_for_user(std::int64_t user_id, std::int64_t server_id) const {
    Statement stmt(db_, "SELECT role FROM server_memberships WHERE user_id = ? AND server_id = ?;");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, server_id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return {};
    }
    return column_text(stmt.get(), 0);
}

ServerInvite ServerRepository::regenerate_invite(std::int64_t user_id,
                                                 std::int64_t server_id,
                                                 const std::string& invite_code) const {
    Statement stmt(db_,
                   "UPDATE servers SET invite_code = ? WHERE id = ? AND id IN ("
                   "SELECT server_id FROM server_memberships WHERE user_id = ? AND role = 'owner');");
    bind_text(db_, stmt.get(), 1, invite_code);
    bind_int64(db_, stmt.get(), 2, server_id);
    bind_int64(db_, stmt.get(), 3, user_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to regenerate invite");
    if (sqlite3_changes(db_) == 0) {
        throw NotFoundError("Server not found or you are not the owner.");
    }
    auto invite = find_invite_for_member(user_id, server_id);
    if (!invite.has_value()) {
        throw StorageError("Failed to reload invite.");
    }
    return *invite;
}

std::optional<ServerInvite> ServerRepository::find_invite_for_member(std::int64_t user_id,
                                                                     std::int64_t server_id) const {
    Statement stmt(db_,
                   "SELECT s.id, s.name, s.invite_code "
                   "FROM servers s "
                   "JOIN server_memberships m ON m.server_id = s.id "
                   "WHERE m.user_id = ? AND s.id = ? "
                   "LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, server_id);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    return ServerInvite {
        .server_id = sqlite3_column_int64(stmt.get(), 0),
        .server_name = column_text(stmt.get(), 1),
        .invite_code = column_text(stmt.get(), 2),
    };
}

std::optional<ServerSummary> ServerRepository::find_by_invite_code(const std::string& invite_code) const {
    Statement stmt(db_, "SELECT id, name FROM servers WHERE invite_code = ? LIMIT 1;");
    bind_text(db_, stmt.get(), 1, invite_code);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    return ServerSummary {
        .id = sqlite3_column_int64(stmt.get(), 0),
        .name = column_text(stmt.get(), 1),
    };
}

void ServerRepository::add_member(std::int64_t user_id,
                                  std::int64_t server_id,
                                  std::int64_t joined_at_ms) const {
    Statement stmt(
        db_,
        "INSERT OR IGNORE INTO server_memberships (user_id, server_id, joined_at_ms) VALUES (?, ?, ?);");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, server_id);
    bind_int64(db_, stmt.get(), 3, joined_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to join server");
}

std::vector<ChannelSummary> ChannelRepository::list_for_server(std::int64_t server_id) const {
    Statement stmt(db_,
                   "SELECT id, server_id, name, position "
                   "FROM channels WHERE server_id = ? ORDER BY position ASC, name ASC;");
    bind_int64(db_, stmt.get(), 1, server_id);

    std::vector<ChannelSummary> channels;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to read channels");
        channels.push_back(ChannelSummary {
            .id = sqlite3_column_int64(stmt.get(), 0),
            .server_id = sqlite3_column_int64(stmt.get(), 1),
            .name = column_text(stmt.get(), 2),
            .position = sqlite3_column_int(stmt.get(), 3),
        });
    }
    return channels;
}

ChannelSummary ChannelRepository::create(std::int64_t server_id,
                                         const std::string& name,
                                         std::int64_t created_at_ms) const {
    Statement max_stmt(db_, "SELECT COALESCE(MAX(position), -1) + 1 FROM channels WHERE server_id = ?;");
    bind_int64(db_, max_stmt.get(), 1, server_id);
    check_sqlite_step(sqlite3_step(max_stmt.get()), db_, "Failed to read channel position");
    const int position = sqlite3_column_int(max_stmt.get(), 0);

    Statement stmt(db_, "INSERT INTO channels (server_id, name, position, created_at_ms) VALUES (?, ?, ?, ?);");
    bind_int64(db_, stmt.get(), 1, server_id);
    bind_text(db_, stmt.get(), 2, name);
    bind_int64(db_, stmt.get(), 3, position);
    bind_int64(db_, stmt.get(), 4, created_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to insert channel");

    return ChannelSummary {
        .id = sqlite3_last_insert_rowid(db_),
        .server_id = server_id,
        .name = name,
        .position = position,
    };
}

std::optional<ChannelSummary> ChannelRepository::find_accessible_by_id(std::int64_t user_id,
                                                                       std::int64_t channel_id) const {
    Statement stmt(db_,
                   "SELECT c.id, c.server_id, c.name, c.position "
                   "FROM channels c "
                   "JOIN server_memberships m ON m.server_id = c.server_id "
                   "WHERE m.user_id = ? AND c.id = ? "
                   "LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, channel_id);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    return ChannelSummary {
        .id = sqlite3_column_int64(stmt.get(), 0),
        .server_id = sqlite3_column_int64(stmt.get(), 1),
        .name = column_text(stmt.get(), 2),
        .position = sqlite3_column_int(stmt.get(), 3),
    };
}

bool ChannelRepository::set_position(std::int64_t user_id, std::int64_t channel_id, int position) const {
    Statement stmt(db_,
                   "UPDATE channels SET position = ? WHERE id = ? AND server_id IN ("
                   "SELECT server_id FROM server_memberships WHERE user_id = ? AND role = 'owner');");
    bind_int64(db_, stmt.get(), 1, position);
    bind_int64(db_, stmt.get(), 2, channel_id);
    bind_int64(db_, stmt.get(), 3, user_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to update channel position");
    return sqlite3_changes(db_) > 0;
}

std::vector<MessageRecord> MessageRepository::list_for_channel(std::int64_t channel_id,
                                                               int limit,
                                                               std::int64_t viewer_user_id) const {
    Statement stmt(db_,
                   "SELECT m.id, m.channel_id, m.author_id, u.username, m.content, m.created_at_ms, "
                   "COALESCE(m.edited_at_ms, 0) "
                   "FROM messages m "
                   "JOIN users u ON u.id = m.author_id "
                   "WHERE m.channel_id = ? "
                   "ORDER BY m.created_at_ms DESC, m.id DESC "
                   "LIMIT ?;");
    bind_int64(db_, stmt.get(), 1, channel_id);
    bind_int64(db_, stmt.get(), 2, limit);

    std::vector<MessageRecord> messages;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to read messages");
        messages.push_back(MessageRecord {
            .id = sqlite3_column_int64(stmt.get(), 0),
            .channel_id = sqlite3_column_int64(stmt.get(), 1),
            .author_id = sqlite3_column_int64(stmt.get(), 2),
            .author_username = column_text(stmt.get(), 3),
            .content = column_text(stmt.get(), 4),
            .created_at_ms = sqlite3_column_int64(stmt.get(), 5),
            .edited_at_ms = sqlite3_column_int64(stmt.get(), 6),
        });
        messages.back().reactions = list_reactions(messages.back().id, viewer_user_id);
    }

    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<MessageRecord> MessageRepository::search_for_channel(std::int64_t channel_id,
                                                                 const std::string& query,
                                                                 int limit,
                                                                 std::int64_t viewer_user_id) const {
    Statement stmt(db_,
                   "SELECT m.id, m.channel_id, m.author_id, u.username, m.content, m.created_at_ms, "
                   "COALESCE(m.edited_at_ms, 0) "
                   "FROM messages m "
                   "JOIN users u ON u.id = m.author_id "
                   "WHERE m.channel_id = ? AND m.content LIKE ? "
                   "ORDER BY m.created_at_ms DESC, m.id DESC LIMIT ?;");
    bind_int64(db_, stmt.get(), 1, channel_id);
    bind_text(db_, stmt.get(), 2, "%" + query + "%");
    bind_int64(db_, stmt.get(), 3, limit);

    std::vector<MessageRecord> messages;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to search messages");
        messages.push_back(MessageRecord {
            .id = sqlite3_column_int64(stmt.get(), 0),
            .channel_id = sqlite3_column_int64(stmt.get(), 1),
            .author_id = sqlite3_column_int64(stmt.get(), 2),
            .author_username = column_text(stmt.get(), 3),
            .content = column_text(stmt.get(), 4),
            .created_at_ms = sqlite3_column_int64(stmt.get(), 5),
            .edited_at_ms = sqlite3_column_int64(stmt.get(), 6),
        });
        messages.back().reactions = list_reactions(messages.back().id, viewer_user_id);
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

bool MessageRepository::delete_own(std::int64_t message_id, std::int64_t author_id) const {
    Statement stmt(db_, "DELETE FROM messages WHERE id = ? AND author_id = ?;");
    bind_int64(db_, stmt.get(), 1, message_id);
    bind_int64(db_, stmt.get(), 2, author_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to delete message");
    return sqlite3_changes(db_) > 0;
}

std::optional<MessageRecord> MessageRepository::edit_own(std::int64_t message_id,
                                                         std::int64_t author_id,
                                                         const std::string& content,
                                                         std::int64_t edited_at_ms) const {
    Statement stmt(db_,
                   "UPDATE messages SET content = ?, edited_at_ms = ? "
                   "WHERE id = ? AND author_id = ?;");
    bind_text(db_, stmt.get(), 1, content);
    bind_int64(db_, stmt.get(), 2, edited_at_ms);
    bind_int64(db_, stmt.get(), 3, message_id);
    bind_int64(db_, stmt.get(), 4, author_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to edit message");
    if (sqlite3_changes(db_) == 0) {
        return std::nullopt;
    }

    Statement read_stmt(db_,
                        "SELECT m.id, m.channel_id, m.author_id, u.username, m.content, m.created_at_ms, "
                        "COALESCE(m.edited_at_ms, 0) FROM messages m "
                        "JOIN users u ON u.id = m.author_id WHERE m.id = ?;");
    bind_int64(db_, read_stmt.get(), 1, message_id);
    if (sqlite3_step(read_stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    MessageRecord message {
        .id = sqlite3_column_int64(read_stmt.get(), 0),
        .channel_id = sqlite3_column_int64(read_stmt.get(), 1),
        .author_id = sqlite3_column_int64(read_stmt.get(), 2),
        .author_username = column_text(read_stmt.get(), 3),
        .content = column_text(read_stmt.get(), 4),
        .created_at_ms = sqlite3_column_int64(read_stmt.get(), 5),
        .edited_at_ms = sqlite3_column_int64(read_stmt.get(), 6),
    };
    message.reactions = list_reactions(message.id, author_id);
    return message;
}

bool MessageRepository::is_accessible(std::int64_t user_id, std::int64_t message_id) const {
    Statement stmt(db_,
                   "SELECT 1 FROM messages m "
                   "JOIN channels c ON c.id = m.channel_id "
                   "JOIN server_memberships sm ON sm.server_id = c.server_id "
                   "WHERE m.id = ? AND sm.user_id = ? LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, message_id);
    bind_int64(db_, stmt.get(), 2, user_id);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

std::vector<ReactionSummary> MessageRepository::toggle_reaction(std::int64_t message_id,
                                                                std::int64_t user_id,
                                                                const std::string& emoji) const {
    return toggle_reaction_for_table(db_, "message_reactions", message_id, user_id, emoji);
}

std::vector<ReactionSummary> MessageRepository::list_reactions(std::int64_t message_id,
                                                               std::int64_t user_id) const {
    return list_reactions_for_table(db_, "message_reactions", message_id, user_id);
}

MessageRecord MessageRepository::create(std::int64_t channel_id,
                                        std::int64_t author_id,
                                        const std::string& author_username,
                                        const std::string& content,
                                        std::int64_t created_at_ms) const {
    Statement stmt(db_,
                   "INSERT INTO messages (channel_id, author_id, content, created_at_ms) "
                   "VALUES (?, ?, ?, ?);");
    bind_int64(db_, stmt.get(), 1, channel_id);
    bind_int64(db_, stmt.get(), 2, author_id);
    bind_text(db_, stmt.get(), 3, content);
    bind_int64(db_, stmt.get(), 4, created_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to insert message");

    return MessageRecord {
        .id = sqlite3_last_insert_rowid(db_),
        .channel_id = channel_id,
        .author_id = author_id,
        .author_username = author_username,
        .content = content,
        .created_at_ms = created_at_ms,
    };
}

FriendsSnapshot FriendRepository::snapshot_for_user(std::int64_t user_id) const {
    FriendsSnapshot snapshot;

    {
        Statement stmt(db_,
                       "SELECT u.id, u.username, u.created_at_ms "
                       "FROM friend_requests f "
                       "JOIN users u ON u.id = CASE "
                       "  WHEN f.requester_id = ? THEN f.addressee_id "
                       "  ELSE f.requester_id END "
                       "WHERE f.status = 'accepted' "
                       "  AND (f.requester_id = ? OR f.addressee_id = ?) "
                       "ORDER BY u.username ASC;");
        bind_int64(db_, stmt.get(), 1, user_id);
        bind_int64(db_, stmt.get(), 2, user_id);
        bind_int64(db_, stmt.get(), 3, user_id);

        while (true) {
            const int rc = sqlite3_step(stmt.get());
            if (rc == SQLITE_DONE) {
                break;
            }
            check_sqlite_step(rc, db_, "Failed to read friends");
            snapshot.friends.push_back(User {
                .id = sqlite3_column_int64(stmt.get(), 0),
                .username = column_text(stmt.get(), 1),
                .created_at_ms = sqlite3_column_int64(stmt.get(), 2),
            });
        }
    }

    {
        Statement stmt(db_,
                       "SELECT f.id, u.id, u.username, u.created_at_ms, f.created_at_ms "
                       "FROM friend_requests f "
                       "JOIN users u ON u.id = f.requester_id "
                       "WHERE f.addressee_id = ? AND f.status = 'pending' "
                       "ORDER BY f.created_at_ms DESC;");
        bind_int64(db_, stmt.get(), 1, user_id);

        while (true) {
            const int rc = sqlite3_step(stmt.get());
            if (rc == SQLITE_DONE) {
                break;
            }
            check_sqlite_step(rc, db_, "Failed to read inbound friend requests");
            snapshot.inbound.push_back(FriendRequestRecord {
                .id = sqlite3_column_int64(stmt.get(), 0),
                .user = User {
                    .id = sqlite3_column_int64(stmt.get(), 1),
                    .username = column_text(stmt.get(), 2),
                    .created_at_ms = sqlite3_column_int64(stmt.get(), 3),
                },
                .created_at_ms = sqlite3_column_int64(stmt.get(), 4),
            });
        }
    }

    {
        Statement stmt(db_,
                       "SELECT f.id, u.id, u.username, u.created_at_ms, f.created_at_ms "
                       "FROM friend_requests f "
                       "JOIN users u ON u.id = f.addressee_id "
                       "WHERE f.requester_id = ? AND f.status = 'pending' "
                       "ORDER BY f.created_at_ms DESC;");
        bind_int64(db_, stmt.get(), 1, user_id);

        while (true) {
            const int rc = sqlite3_step(stmt.get());
            if (rc == SQLITE_DONE) {
                break;
            }
            check_sqlite_step(rc, db_, "Failed to read outbound friend requests");
            snapshot.outbound.push_back(FriendRequestRecord {
                .id = sqlite3_column_int64(stmt.get(), 0),
                .user = User {
                    .id = sqlite3_column_int64(stmt.get(), 1),
                    .username = column_text(stmt.get(), 2),
                    .created_at_ms = sqlite3_column_int64(stmt.get(), 3),
                },
                .created_at_ms = sqlite3_column_int64(stmt.get(), 4),
            });
        }
    }

    return snapshot;
}

FriendRequestRecord FriendRepository::create_request(std::int64_t requester_id,
                                                     const User& addressee,
                                                     std::int64_t created_at_ms) const {
    Statement stmt(db_,
                   "INSERT INTO friend_requests "
                   "(requester_id, addressee_id, status, created_at_ms) "
                   "VALUES (?, ?, 'pending', ?);");
    bind_int64(db_, stmt.get(), 1, requester_id);
    bind_int64(db_, stmt.get(), 2, addressee.id);
    bind_int64(db_, stmt.get(), 3, created_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to create friend request");

    return FriendRequestRecord {
        .id = sqlite3_last_insert_rowid(db_),
        .user = addressee,
        .created_at_ms = created_at_ms,
    };
}

bool FriendRepository::has_any_relationship(std::int64_t user_a, std::int64_t user_b) const {
    Statement stmt(db_,
                   "SELECT 1 FROM friend_requests "
                   "WHERE (requester_id = ? AND addressee_id = ?) "
                   "   OR (requester_id = ? AND addressee_id = ?) "
                   "LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, user_a);
    bind_int64(db_, stmt.get(), 2, user_b);
    bind_int64(db_, stmt.get(), 3, user_b);
    bind_int64(db_, stmt.get(), 4, user_a);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

bool FriendRepository::are_friends(std::int64_t user_a, std::int64_t user_b) const {
    Statement stmt(db_,
                   "SELECT 1 FROM friend_requests "
                   "WHERE status = 'accepted' AND ("
                   "  (requester_id = ? AND addressee_id = ?) "
                   "  OR (requester_id = ? AND addressee_id = ?)) "
                   "LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, user_a);
    bind_int64(db_, stmt.get(), 2, user_b);
    bind_int64(db_, stmt.get(), 3, user_b);
    bind_int64(db_, stmt.get(), 4, user_a);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

bool FriendRepository::accept_request(std::int64_t request_id,
                                      std::int64_t addressee_id,
                                      std::int64_t responded_at_ms) const {
    Statement stmt(db_,
                   "UPDATE friend_requests "
                   "SET status = 'accepted', responded_at_ms = ? "
                   "WHERE id = ? AND addressee_id = ? AND status = 'pending';");
    bind_int64(db_, stmt.get(), 1, responded_at_ms);
    bind_int64(db_, stmt.get(), 2, request_id);
    bind_int64(db_, stmt.get(), 3, addressee_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to accept friend request");
    return sqlite3_changes(db_) > 0;
}

bool FriendRepository::deny_request(std::int64_t request_id, std::int64_t addressee_id) const {
    Statement stmt(db_,
                   "DELETE FROM friend_requests "
                   "WHERE id = ? AND addressee_id = ? AND status = 'pending';");
    bind_int64(db_, stmt.get(), 1, request_id);
    bind_int64(db_, stmt.get(), 2, addressee_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to deny friend request");
    return sqlite3_changes(db_) > 0;
}

bool FriendRepository::remove_friend(std::int64_t user_id, std::int64_t friend_id) const {
    Statement stmt(db_,
                   "DELETE FROM friend_requests WHERE status = 'accepted' AND ("
                   "(requester_id = ? AND addressee_id = ?) OR "
                   "(requester_id = ? AND addressee_id = ?));");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, friend_id);
    bind_int64(db_, stmt.get(), 3, friend_id);
    bind_int64(db_, stmt.get(), 4, user_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to remove friend");
    return sqlite3_changes(db_) > 0;
}

DirectConversationSummary DirectMessageRepository::find_or_create(std::int64_t user_id,
                                                                  const User& other_user,
                                                                  std::int64_t created_at_ms) const {
    {
        Statement stmt(db_,
                       "SELECT c.id "
                       "FROM direct_conversations c "
                       "JOIN direct_participants p1 ON p1.conversation_id = c.id AND p1.user_id = ? "
                       "JOIN direct_participants p2 ON p2.conversation_id = c.id AND p2.user_id = ? "
                       "LIMIT 1;");
        bind_int64(db_, stmt.get(), 1, user_id);
        bind_int64(db_, stmt.get(), 2, other_user.id);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            return DirectConversationSummary {
                .id = sqlite3_column_int64(stmt.get(), 0),
                .other_user = other_user,
            };
        }
    }

    Statement conversation_stmt(db_, "INSERT INTO direct_conversations (created_at_ms) VALUES (?);");
    bind_int64(db_, conversation_stmt.get(), 1, created_at_ms);
    check_sqlite_step(sqlite3_step(conversation_stmt.get()), db_, "Failed to create direct conversation");
    const auto conversation_id = sqlite3_last_insert_rowid(db_);

    Statement participant_stmt(
        db_,
        "INSERT INTO direct_participants (conversation_id, user_id) VALUES (?, ?), (?, ?);");
    bind_int64(db_, participant_stmt.get(), 1, conversation_id);
    bind_int64(db_, participant_stmt.get(), 2, user_id);
    bind_int64(db_, participant_stmt.get(), 3, conversation_id);
    bind_int64(db_, participant_stmt.get(), 4, other_user.id);
    check_sqlite_step(sqlite3_step(participant_stmt.get()), db_, "Failed to create direct participants");

    return DirectConversationSummary {
        .id = conversation_id,
        .other_user = other_user,
    };
}

std::vector<DirectConversationSummary> DirectMessageRepository::list_for_user(std::int64_t user_id) const {
    Statement stmt(db_,
                   "SELECT c.id, u.id, u.username, u.created_at_ms "
                   "FROM direct_conversations c "
                   "JOIN direct_participants mine ON mine.conversation_id = c.id AND mine.user_id = ? "
                   "JOIN direct_participants other ON other.conversation_id = c.id AND other.user_id <> ? "
                   "JOIN users u ON u.id = other.user_id "
                   "ORDER BY c.id DESC;");
    bind_int64(db_, stmt.get(), 1, user_id);
    bind_int64(db_, stmt.get(), 2, user_id);

    std::vector<DirectConversationSummary> conversations;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to read direct conversations");
        conversations.push_back(DirectConversationSummary {
            .id = sqlite3_column_int64(stmt.get(), 0),
            .other_user = User {
                .id = sqlite3_column_int64(stmt.get(), 1),
                .username = column_text(stmt.get(), 2),
                .created_at_ms = sqlite3_column_int64(stmt.get(), 3),
            },
        });
    }
    return conversations;
}

bool DirectMessageRepository::is_participant(std::int64_t user_id, std::int64_t conversation_id) const {
    Statement stmt(db_,
                   "SELECT 1 FROM direct_participants WHERE conversation_id = ? AND user_id = ? LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, conversation_id);
    bind_int64(db_, stmt.get(), 2, user_id);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

std::vector<DirectMessageRecord> DirectMessageRepository::list_messages(std::int64_t user_id,
                                                                        std::int64_t conversation_id,
                                                                        int limit) const {
    if (!is_participant(user_id, conversation_id)) {
        return {};
    }

    Statement stmt(db_,
                   "SELECT m.id, m.conversation_id, m.author_id, u.username, m.content, m.created_at_ms "
                   "FROM direct_messages m "
                   "JOIN users u ON u.id = m.author_id "
                   "WHERE m.conversation_id = ? "
                   "ORDER BY m.created_at_ms DESC, m.id DESC "
                   "LIMIT ?;");
    bind_int64(db_, stmt.get(), 1, conversation_id);
    bind_int64(db_, stmt.get(), 2, limit);

    std::vector<DirectMessageRecord> messages;
    while (true) {
        const int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
            break;
        }
        check_sqlite_step(rc, db_, "Failed to read direct messages");
        messages.push_back(DirectMessageRecord {
            .id = sqlite3_column_int64(stmt.get(), 0),
            .conversation_id = sqlite3_column_int64(stmt.get(), 1),
            .author_id = sqlite3_column_int64(stmt.get(), 2),
            .author_username = column_text(stmt.get(), 3),
            .content = column_text(stmt.get(), 4),
            .created_at_ms = sqlite3_column_int64(stmt.get(), 5),
        });
        messages.back().reactions = list_reactions(messages.back().id, user_id);
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

DirectMessageRecord DirectMessageRepository::create_message(std::int64_t conversation_id,
                                                            std::int64_t author_id,
                                                            const std::string& author_username,
                                                            const std::string& content,
                                                            std::int64_t created_at_ms) const {
    Statement stmt(db_,
                   "INSERT INTO direct_messages (conversation_id, author_id, content, created_at_ms) "
                   "VALUES (?, ?, ?, ?);");
    bind_int64(db_, stmt.get(), 1, conversation_id);
    bind_int64(db_, stmt.get(), 2, author_id);
    bind_text(db_, stmt.get(), 3, content);
    bind_int64(db_, stmt.get(), 4, created_at_ms);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to insert direct message");
    return DirectMessageRecord {
        .id = sqlite3_last_insert_rowid(db_),
        .conversation_id = conversation_id,
        .author_id = author_id,
        .author_username = author_username,
        .content = content,
        .created_at_ms = created_at_ms,
    };
}

bool DirectMessageRepository::delete_own_message(std::int64_t message_id, std::int64_t author_id) const {
    Statement stmt(db_, "DELETE FROM direct_messages WHERE id = ? AND author_id = ?;");
    bind_int64(db_, stmt.get(), 1, message_id);
    bind_int64(db_, stmt.get(), 2, author_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to delete direct message");
    return sqlite3_changes(db_) > 0;
}

std::optional<DirectMessageRecord> DirectMessageRepository::edit_own_message(std::int64_t message_id,
                                                                             std::int64_t author_id,
                                                                             const std::string& content,
                                                                             std::int64_t edited_at_ms) const {
    Statement stmt(db_,
                   "UPDATE direct_messages SET content = ?, edited_at_ms = ? "
                   "WHERE id = ? AND author_id = ?;");
    bind_text(db_, stmt.get(), 1, content);
    bind_int64(db_, stmt.get(), 2, edited_at_ms);
    bind_int64(db_, stmt.get(), 3, message_id);
    bind_int64(db_, stmt.get(), 4, author_id);
    check_sqlite_step(sqlite3_step(stmt.get()), db_, "Failed to edit direct message");
    if (sqlite3_changes(db_) == 0) {
        return std::nullopt;
    }

    Statement read_stmt(db_,
                        "SELECT m.id, m.conversation_id, m.author_id, u.username, m.content, "
                        "m.created_at_ms, COALESCE(m.edited_at_ms, 0) "
                        "FROM direct_messages m "
                        "JOIN users u ON u.id = m.author_id WHERE m.id = ?;");
    bind_int64(db_, read_stmt.get(), 1, message_id);
    if (sqlite3_step(read_stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    DirectMessageRecord message {
        .id = sqlite3_column_int64(read_stmt.get(), 0),
        .conversation_id = sqlite3_column_int64(read_stmt.get(), 1),
        .author_id = sqlite3_column_int64(read_stmt.get(), 2),
        .author_username = column_text(read_stmt.get(), 3),
        .content = column_text(read_stmt.get(), 4),
        .created_at_ms = sqlite3_column_int64(read_stmt.get(), 5),
        .edited_at_ms = sqlite3_column_int64(read_stmt.get(), 6),
    };
    message.reactions = list_reactions(message.id, author_id);
    return message;
}

bool DirectMessageRepository::is_message_accessible(std::int64_t user_id, std::int64_t message_id) const {
    Statement stmt(db_,
                   "SELECT 1 FROM direct_messages m "
                   "JOIN direct_participants p ON p.conversation_id = m.conversation_id "
                   "WHERE m.id = ? AND p.user_id = ? LIMIT 1;");
    bind_int64(db_, stmt.get(), 1, message_id);
    bind_int64(db_, stmt.get(), 2, user_id);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

std::vector<ReactionSummary> DirectMessageRepository::toggle_reaction(std::int64_t message_id,
                                                                      std::int64_t user_id,
                                                                      const std::string& emoji) const {
    return toggle_reaction_for_table(db_, "direct_message_reactions", message_id, user_id, emoji);
}

std::vector<ReactionSummary> DirectMessageRepository::list_reactions(std::int64_t message_id,
                                                                     std::int64_t user_id) const {
    return list_reactions_for_table(db_, "direct_message_reactions", message_id, user_id);
}

}  // namespace hangout
