#include "hangout/application/social_service.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>

#include "hangout/domain/errors.hpp"
#include "hangout/storage/repositories.hpp"

namespace hangout {
namespace {

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string trim_copy(const std::string& input) {
    const auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

}  // namespace

SocialService::SocialService(FriendRepository& friends,
                             UserRepository& users,
                             DirectMessageRepository& direct_messages)
    : friends_(friends), users_(users), direct_messages_(direct_messages) {}

FriendsSnapshot SocialService::list_friends(const SessionContext& session) const {
    return friends_.snapshot_for_user(session.user.id);
}

FriendRequestRecord SocialService::send_request(const SessionContext& session, const std::string& username) {
    const auto normalized = trim_copy(username);
    if (normalized.empty()) {
        throw ValidationError("Enter a username.");
    }

    const auto target = users_.find_by_username(normalized);
    if (!target.has_value()) {
        throw NotFoundError("That user does not exist yet.");
    }
    if (target->id == session.user.id) {
        throw ValidationError("You cannot send a friend request to yourself.");
    }
    if (friends_.has_any_relationship(session.user.id, target->id)) {
        throw ValidationError("A friend request or friendship already exists.");
    }

    return friends_.create_request(session.user.id, *target, now_ms());
}

void SocialService::accept_request(const SessionContext& session, std::int64_t request_id) {
    if (!friends_.accept_request(request_id, session.user.id, now_ms())) {
        throw NotFoundError("Friend request not found.");
    }
}

void SocialService::deny_request(const SessionContext& session, std::int64_t request_id) {
    if (!friends_.deny_request(request_id, session.user.id)) {
        throw NotFoundError("Friend request not found.");
    }
}

void SocialService::remove_friend(const SessionContext& session, std::int64_t friend_id) {
    if (!friends_.remove_friend(session.user.id, friend_id)) {
        throw NotFoundError("Friendship not found.");
    }
}

DirectConversationSummary SocialService::open_conversation(const SessionContext& session,
                                                           const std::string& username) {
    const auto normalized = trim_copy(username);
    const auto other_user = users_.find_by_username(normalized);
    if (!other_user.has_value()) {
        throw NotFoundError("That user does not exist yet.");
    }
    if (other_user->id == session.user.id) {
        throw ValidationError("You cannot open a direct message with yourself.");
    }
    if (!friends_.are_friends(session.user.id, other_user->id)) {
        throw ValidationError("You can only direct message accepted friends.");
    }
    return direct_messages_.find_or_create(session.user.id, *other_user, now_ms());
}

std::vector<DirectConversationSummary> SocialService::list_conversations(const SessionContext& session) const {
    return direct_messages_.list_for_user(session.user.id);
}

std::vector<DirectMessageRecord> SocialService::list_direct_messages(const SessionContext& session,
                                                                     std::int64_t conversation_id,
                                                                     int limit) const {
    const int clamped_limit = std::clamp(limit, 1, 100);
    auto messages = direct_messages_.list_messages(session.user.id, conversation_id, clamped_limit);
    if (messages.empty() && !direct_messages_.is_participant(session.user.id, conversation_id)) {
        throw NotFoundError("Direct conversation not found.");
    }
    return messages;
}

DirectMessageRecord SocialService::send_direct_message(const SessionContext& session,
                                                       std::int64_t conversation_id,
                                                       const std::string& content) {
    if (!direct_messages_.is_participant(session.user.id, conversation_id)) {
        throw NotFoundError("Direct conversation not found.");
    }
    const auto trimmed = trim_copy(content);
    if (trimmed.empty()) {
        throw ValidationError("Message content cannot be empty.");
    }
    if (trimmed.size() > 2000U) {
        throw ValidationError("Message content cannot exceed 2000 characters.");
    }
    return direct_messages_.create_message(
        conversation_id, session.user.id, session.user.username, trimmed, now_ms());
}

DirectMessageRecord SocialService::edit_direct_message(const SessionContext& session,
                                                       std::int64_t message_id,
                                                       const std::string& content) {
    const auto trimmed = trim_copy(content);
    if (trimmed.empty()) {
        throw ValidationError("Message content cannot be empty.");
    }
    if (trimmed.size() > 2000U) {
        throw ValidationError("Message content cannot exceed 2000 characters.");
    }

    auto message = direct_messages_.edit_own_message(message_id, session.user.id, trimmed, now_ms());
    if (!message.has_value()) {
        throw NotFoundError("Direct message not found or you are not the author.");
    }
    return *message;
}

void SocialService::delete_direct_message(const SessionContext& session, std::int64_t message_id) {
    if (!direct_messages_.delete_own_message(message_id, session.user.id)) {
        throw NotFoundError("Direct message not found or you are not the author.");
    }
}

std::vector<ReactionSummary> SocialService::toggle_direct_message_reaction(const SessionContext& session,
                                                                           std::int64_t message_id,
                                                                           const std::string& emoji) {
    const auto trimmed = trim_copy(emoji);
    if (trimmed.empty() || trimmed.size() > 16U) {
        throw ValidationError("Reaction must be 1-16 characters.");
    }
    if (!direct_messages_.is_message_accessible(session.user.id, message_id)) {
        throw NotFoundError("Direct message not found.");
    }
    return direct_messages_.toggle_reaction(message_id, session.user.id, trimmed);
}

}  // namespace hangout
