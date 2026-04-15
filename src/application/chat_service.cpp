#include "hangout/application/chat_service.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <random>
#include <regex>

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

void validate_server_name(const std::string& name) {
    if (name.size() < 2U || name.size() > 48U) {
        throw ValidationError("Server name must be 2-48 characters.");
    }
}

void validate_channel_name(const std::string& name) {
    static const std::regex pattern(R"(^[a-z0-9][a-z0-9_-]{1,31}$)");
    if (!std::regex_match(name, pattern)) {
        throw ValidationError("Channel name must be 2-32 lowercase letters, numbers, '_' or '-'.");
    }
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

std::string normalize_invite_code(const std::string& raw) {
    auto code = trim_copy(raw);
    code.erase(std::remove_if(code.begin(), code.end(), [](unsigned char ch) {
                   return std::isspace(ch) != 0 || ch == '-';
               }),
               code.end());
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return code;
}

}  // namespace

ChatService::ChatService(ServerRepository& servers,
                         ChannelRepository& channels,
                         MessageRepository& messages,
                         MessageEventBus& event_bus)
    : servers_(servers), channels_(channels), messages_(messages), event_bus_(event_bus) {}

std::vector<ServerSummary> ChatService::list_servers(const SessionContext& session) const {
    return servers_.list_for_user(session.user.id);
}

ServerSummary ChatService::create_server(const SessionContext& session, const std::string& name) {
    const auto trimmed = trim_copy(name);
    validate_server_name(trimmed);
    return servers_.create_for_user(session.user.id, trimmed, generate_invite_code(), now_ms());
}

ServerSummary ChatService::join_server(const SessionContext& session, const std::string& invite_code) {
    const auto normalized = normalize_invite_code(invite_code);
    if (normalized.size() != 8U) {
        throw ValidationError("Invite code must be 8 characters.");
    }

    const auto server = servers_.find_by_invite_code(normalized);
    if (!server.has_value()) {
        throw NotFoundError("Invite code was not found.");
    }

    servers_.add_member(session.user.id, server->id, now_ms());
    return *server;
}

ServerInvite ChatService::get_server_invite(const SessionContext& session, std::int64_t server_id) const {
    const auto invite = servers_.find_invite_for_member(session.user.id, server_id);
    if (!invite.has_value()) {
        throw NotFoundError("Server not found.");
    }
    return *invite;
}

ServerInvite ChatService::regenerate_server_invite(const SessionContext& session, std::int64_t server_id) {
    return servers_.regenerate_invite(session.user.id, server_id, generate_invite_code());
}

std::vector<ServerMember> ChatService::list_members(const SessionContext& session, std::int64_t server_id) const {
    auto members = servers_.list_members(session.user.id, server_id);
    if (members.empty() && !servers_.is_member(session.user.id, server_id)) {
        throw NotFoundError("Server not found.");
    }
    return members;
}

std::vector<ChannelSummary> ChatService::list_channels(const SessionContext& session,
                                                       std::int64_t server_id) const {
    if (!servers_.is_member(session.user.id, server_id)) {
        throw NotFoundError("Server not found.");
    }
    return channels_.list_for_server(server_id);
}

ChannelSummary ChatService::create_channel(const SessionContext& session,
                                           std::int64_t server_id,
                                           const std::string& name) {
    if (!servers_.is_member(session.user.id, server_id)) {
        throw NotFoundError("Server not found.");
    }

    auto trimmed = trim_copy(name);
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::replace(trimmed.begin(), trimmed.end(), ' ', '-');
    validate_channel_name(trimmed);

    return channels_.create(server_id, trimmed, now_ms());
}

std::vector<MessageRecord> ChatService::list_messages(const SessionContext& session,
                                                      std::int64_t channel_id,
                                                      int limit) const {
    if (!channels_.find_accessible_by_id(session.user.id, channel_id).has_value()) {
        throw NotFoundError("Channel not found.");
    }

    const int clamped_limit = std::clamp(limit, 1, 100);
    return messages_.list_for_channel(channel_id, clamped_limit, session.user.id);
}

std::vector<MessageRecord> ChatService::search_messages(const SessionContext& session,
                                                        std::int64_t channel_id,
                                                        const std::string& query,
                                                        int limit) const {
    if (!channels_.find_accessible_by_id(session.user.id, channel_id).has_value()) {
        throw NotFoundError("Channel not found.");
    }
    const auto trimmed = trim_copy(query);
    if (trimmed.empty()) {
        return {};
    }
    return messages_.search_for_channel(channel_id, trimmed, std::clamp(limit, 1, 100), session.user.id);
}

MessageRecord ChatService::send_message(const SessionContext& session,
                                        std::int64_t channel_id,
                                        const std::string& content) {
    if (!channels_.find_accessible_by_id(session.user.id, channel_id).has_value()) {
        throw NotFoundError("Channel not found.");
    }

    const auto trimmed = trim_copy(content);
    if (trimmed.empty()) {
        throw ValidationError("Message content cannot be empty.");
    }
    if (trimmed.size() > 2000U) {
        throw ValidationError("Message content cannot exceed 2000 characters.");
    }

    const auto message =
        messages_.create(channel_id, session.user.id, session.user.username, trimmed, now_ms());
    event_bus_.publish_message(message);
    return message;
}

MessageRecord ChatService::edit_message(const SessionContext& session,
                                        std::int64_t message_id,
                                        const std::string& content) {
    const auto trimmed = trim_copy(content);
    if (trimmed.empty()) {
        throw ValidationError("Message content cannot be empty.");
    }
    if (trimmed.size() > 2000U) {
        throw ValidationError("Message content cannot exceed 2000 characters.");
    }
    auto message = messages_.edit_own(message_id, session.user.id, trimmed, now_ms());
    if (!message.has_value()) {
        throw NotFoundError("Message not found or you are not the author.");
    }
    return *message;
}

void ChatService::delete_message(const SessionContext& session, std::int64_t message_id) {
    if (!messages_.delete_own(message_id, session.user.id)) {
        throw NotFoundError("Message not found or you are not the author.");
    }
}

std::vector<ReactionSummary> ChatService::toggle_message_reaction(const SessionContext& session,
                                                                  std::int64_t message_id,
                                                                  const std::string& emoji) {
    const auto trimmed = trim_copy(emoji);
    if (trimmed.empty() || trimmed.size() > 16U) {
        throw ValidationError("Reaction must be 1-16 characters.");
    }
    if (!messages_.is_accessible(session.user.id, message_id)) {
        throw NotFoundError("Message not found.");
    }
    return messages_.toggle_reaction(message_id, session.user.id, trimmed);
}

void ChatService::set_channel_position(const SessionContext& session, std::int64_t channel_id, int position) {
    if (!channels_.set_position(session.user.id, channel_id, position)) {
        throw NotFoundError("Channel not found or you are not the server owner.");
    }
}

}  // namespace hangout
