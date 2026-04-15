#pragma once

#include <cstdint>
#include <string>

#include "hangout/domain/models.hpp"

namespace hangout {

class FriendRepository;
class DirectMessageRepository;
class UserRepository;

class SocialService {
public:
    SocialService(FriendRepository& friends, UserRepository& users, DirectMessageRepository& direct_messages);

    FriendsSnapshot list_friends(const SessionContext& session) const;
    FriendRequestRecord send_request(const SessionContext& session, const std::string& username);
    void accept_request(const SessionContext& session, std::int64_t request_id);
    void deny_request(const SessionContext& session, std::int64_t request_id);
    void remove_friend(const SessionContext& session, std::int64_t friend_id);
    DirectConversationSummary open_conversation(const SessionContext& session, const std::string& username);
    std::vector<DirectConversationSummary> list_conversations(const SessionContext& session) const;
    std::vector<DirectMessageRecord> list_direct_messages(const SessionContext& session,
                                                          std::int64_t conversation_id,
                                                          int limit) const;
    DirectMessageRecord send_direct_message(const SessionContext& session,
                                            std::int64_t conversation_id,
                                            const std::string& content);
    DirectMessageRecord edit_direct_message(const SessionContext& session,
                                            std::int64_t message_id,
                                            const std::string& content);
    void delete_direct_message(const SessionContext& session, std::int64_t message_id);
    std::vector<ReactionSummary> toggle_direct_message_reaction(const SessionContext& session,
                                                                std::int64_t message_id,
                                                                const std::string& emoji);

private:
    FriendRepository& friends_;
    UserRepository& users_;
    DirectMessageRepository& direct_messages_;
};

}  // namespace hangout
