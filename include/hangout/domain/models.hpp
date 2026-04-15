#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hangout {

struct User {
    std::int64_t id {};
    std::string username;
    std::string display_name;
    std::string status_text;
    std::string user_status;
    std::string avatar_color;
    std::string avatar_url;
    std::int64_t last_seen_at_ms {};
    std::int64_t created_at_ms {};
};

struct Session {
    std::int64_t id {};
    std::int64_t user_id {};
    std::string token;
    std::int64_t created_at_ms {};
    std::int64_t expires_at_ms {};
};

struct SessionContext {
    User user;
    Session session;
};

struct ServerSummary {
    std::int64_t id {};
    std::string name;
};

struct ServerInvite {
    std::int64_t server_id {};
    std::string server_name;
    std::string invite_code;
};

struct ServerMember {
    User user;
    std::string role;
    std::int64_t joined_at_ms {};
};

struct ChannelSummary {
    std::int64_t id {};
    std::int64_t server_id {};
    std::string name;
    int position {};
};

struct ReactionSummary {
    std::string emoji;
    int count {};
    bool reacted_by_me {};
};

struct MessageRecord {
    std::int64_t id {};
    std::int64_t channel_id {};
    std::int64_t author_id {};
    std::string author_username;
    std::string content;
    std::int64_t created_at_ms {};
    std::int64_t edited_at_ms {};
    std::vector<ReactionSummary> reactions;
};

struct LoginResult {
    User user;
    Session session;
};

struct FriendRequestRecord {
    std::int64_t id {};
    User user;
    std::int64_t created_at_ms {};
};

struct FriendsSnapshot {
    std::vector<User> friends;
    std::vector<FriendRequestRecord> inbound;
    std::vector<FriendRequestRecord> outbound;
};

struct DirectConversationSummary {
    std::int64_t id {};
    User other_user;
};

struct DirectMessageRecord {
    std::int64_t id {};
    std::int64_t conversation_id {};
    std::int64_t author_id {};
    std::string author_username;
    std::string content;
    std::int64_t created_at_ms {};
    std::int64_t edited_at_ms {};
    std::vector<ReactionSummary> reactions;
};

struct TypingStatus {
    User user;
    std::string target_type;
    std::int64_t target_id {};
    std::int64_t expires_at_ms {};
};

}  // namespace hangout
