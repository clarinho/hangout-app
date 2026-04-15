#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hangout/domain/models.hpp"

struct sqlite3;

namespace hangout {

class UserRepository {
public:
    explicit UserRepository(sqlite3* db) : db_(db) {}

    std::optional<User> find_by_id(std::int64_t user_id) const;
    std::optional<User> find_by_username(const std::string& username) const;
    User create(const std::string& username, std::int64_t created_at_ms) const;
    User update_profile(std::int64_t user_id,
                        const std::string& display_name,
                        const std::string& status_text,
                        const std::string& user_status,
                        const std::string& avatar_color,
                        std::int64_t now_ms) const;
    void heartbeat(std::int64_t user_id, std::int64_t now_ms) const;

private:
    sqlite3* db_;
};

class SessionRepository {
public:
    explicit SessionRepository(sqlite3* db) : db_(db) {}

    Session create(std::int64_t user_id,
                   const std::string& token,
                   std::int64_t created_at_ms,
                   std::int64_t expires_at_ms) const;
    std::optional<Session> find_valid_by_token(const std::string& token, std::int64_t now_ms) const;

private:
    sqlite3* db_;
};

class ServerRepository {
public:
    explicit ServerRepository(sqlite3* db) : db_(db) {}

    std::vector<ServerSummary> list_for_user(std::int64_t user_id) const;
    std::vector<ServerMember> list_members(std::int64_t user_id, std::int64_t server_id) const;
    std::string role_for_user(std::int64_t user_id, std::int64_t server_id) const;
    bool is_member(std::int64_t user_id, std::int64_t server_id) const;
    void add_user_to_all_servers(std::int64_t user_id, std::int64_t joined_at_ms) const;
    ServerSummary create_for_user(std::int64_t user_id,
                                  const std::string& name,
                                  const std::string& invite_code,
                                  std::int64_t created_at_ms) const;
    std::optional<ServerInvite> find_invite_for_member(std::int64_t user_id,
                                                       std::int64_t server_id) const;
    ServerInvite regenerate_invite(std::int64_t user_id,
                                   std::int64_t server_id,
                                   const std::string& invite_code) const;
    std::optional<ServerSummary> find_by_invite_code(const std::string& invite_code) const;
    void add_member(std::int64_t user_id,
                    std::int64_t server_id,
                    std::int64_t joined_at_ms) const;

private:
    sqlite3* db_;
};

class ChannelRepository {
public:
    explicit ChannelRepository(sqlite3* db) : db_(db) {}

    std::vector<ChannelSummary> list_for_server(std::int64_t server_id) const;
    ChannelSummary create(std::int64_t server_id,
                          const std::string& name,
                          std::int64_t created_at_ms) const;
    std::optional<ChannelSummary> find_accessible_by_id(std::int64_t user_id, std::int64_t channel_id) const;
    bool set_position(std::int64_t user_id, std::int64_t channel_id, int position) const;

private:
    sqlite3* db_;
};

class MessageRepository {
public:
    explicit MessageRepository(sqlite3* db) : db_(db) {}

    std::vector<MessageRecord> list_for_channel(std::int64_t channel_id,
                                                int limit,
                                                std::int64_t viewer_user_id) const;
    std::vector<MessageRecord> search_for_channel(std::int64_t channel_id,
                                                  const std::string& query,
                                                  int limit,
                                                  std::int64_t viewer_user_id) const;
    MessageRecord create(std::int64_t channel_id,
                         std::int64_t author_id,
                         const std::string& author_username,
                         const std::string& content,
                         std::int64_t created_at_ms) const;
    bool delete_own(std::int64_t message_id, std::int64_t author_id) const;
    std::optional<MessageRecord> edit_own(std::int64_t message_id,
                                          std::int64_t author_id,
                                          const std::string& content,
                                          std::int64_t edited_at_ms) const;
    bool is_accessible(std::int64_t user_id, std::int64_t message_id) const;
    std::vector<ReactionSummary> toggle_reaction(std::int64_t message_id,
                                                 std::int64_t user_id,
                                                 const std::string& emoji) const;
    std::vector<ReactionSummary> list_reactions(std::int64_t message_id,
                                                std::int64_t user_id) const;

private:
    sqlite3* db_;
};

class FriendRepository {
public:
    explicit FriendRepository(sqlite3* db) : db_(db) {}

    FriendsSnapshot snapshot_for_user(std::int64_t user_id) const;
    FriendRequestRecord create_request(std::int64_t requester_id,
                                       const User& addressee,
                                       std::int64_t created_at_ms) const;
    bool has_any_relationship(std::int64_t user_a, std::int64_t user_b) const;
    bool are_friends(std::int64_t user_a, std::int64_t user_b) const;
    bool accept_request(std::int64_t request_id,
                        std::int64_t addressee_id,
                        std::int64_t responded_at_ms) const;
    bool deny_request(std::int64_t request_id, std::int64_t addressee_id) const;
    bool remove_friend(std::int64_t user_id, std::int64_t friend_id) const;

private:
    sqlite3* db_;
};

class DirectMessageRepository {
public:
    explicit DirectMessageRepository(sqlite3* db) : db_(db) {}

    DirectConversationSummary find_or_create(std::int64_t user_id,
                                             const User& other_user,
                                             std::int64_t created_at_ms) const;
    std::vector<DirectConversationSummary> list_for_user(std::int64_t user_id) const;
    bool is_participant(std::int64_t user_id, std::int64_t conversation_id) const;
    std::vector<DirectMessageRecord> list_messages(std::int64_t user_id,
                                                   std::int64_t conversation_id,
                                                   int limit) const;
    std::vector<DirectMessageRecord> search_messages(std::int64_t user_id,
                                                     std::int64_t conversation_id,
                                                     const std::string& query,
                                                     int limit) const;
    DirectMessageRecord create_message(std::int64_t conversation_id,
                                       std::int64_t author_id,
                                       const std::string& author_username,
                                       const std::string& content,
                                       std::int64_t created_at_ms) const;
    bool delete_own_message(std::int64_t message_id, std::int64_t author_id) const;
    std::optional<DirectMessageRecord> edit_own_message(std::int64_t message_id,
                                                        std::int64_t author_id,
                                                        const std::string& content,
                                                        std::int64_t edited_at_ms) const;
    bool is_message_accessible(std::int64_t user_id, std::int64_t message_id) const;
    std::vector<ReactionSummary> toggle_reaction(std::int64_t message_id,
                                                 std::int64_t user_id,
                                                 const std::string& emoji) const;
    std::vector<ReactionSummary> list_reactions(std::int64_t message_id,
                                                std::int64_t user_id) const;

private:
    sqlite3* db_;
};

class TypingRepository {
public:
    explicit TypingRepository(sqlite3* db) : db_(db) {}

    void set_typing(std::int64_t user_id,
                    const std::string& target_type,
                    std::int64_t target_id,
                    std::int64_t expires_at_ms) const;
    std::vector<TypingStatus> list_typing(std::int64_t viewer_user_id,
                                          const std::string& target_type,
                                          std::int64_t target_id,
                                          std::int64_t now_ms) const;

private:
    sqlite3* db_;
};

}  // namespace hangout
