#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hangout/application/event_bus.hpp"
#include "hangout/domain/models.hpp"

namespace hangout {

class ServerRepository;
class ChannelRepository;
class MessageRepository;

class ChatService {
public:
    ChatService(ServerRepository& servers,
                ChannelRepository& channels,
                MessageRepository& messages,
                MessageEventBus& event_bus);

    std::vector<ServerSummary> list_servers(const SessionContext& session) const;
    ServerSummary create_server(const SessionContext& session, const std::string& name);
    ServerSummary join_server(const SessionContext& session, const std::string& invite_code);
    ServerInvite get_server_invite(const SessionContext& session, std::int64_t server_id) const;
    ServerInvite regenerate_server_invite(const SessionContext& session, std::int64_t server_id);
    std::vector<ServerMember> list_members(const SessionContext& session, std::int64_t server_id) const;
    std::vector<ChannelSummary> list_channels(const SessionContext& session, std::int64_t server_id) const;
    ChannelSummary create_channel(const SessionContext& session,
                                  std::int64_t server_id,
                                  const std::string& name);
    std::vector<MessageRecord> list_messages(const SessionContext& session,
                                             std::int64_t channel_id,
                                             int limit) const;
    std::vector<MessageRecord> search_messages(const SessionContext& session,
                                               std::int64_t channel_id,
                                               const std::string& query,
                                               int limit) const;
    MessageRecord send_message(const SessionContext& session,
                               std::int64_t channel_id,
                               const std::string& content);
    MessageRecord edit_message(const SessionContext& session,
                               std::int64_t message_id,
                               const std::string& content);
    void delete_message(const SessionContext& session, std::int64_t message_id);
    std::vector<ReactionSummary> toggle_message_reaction(const SessionContext& session,
                                                         std::int64_t message_id,
                                                         const std::string& emoji);
    void set_channel_position(const SessionContext& session, std::int64_t channel_id, int position);

private:
    ServerRepository& servers_;
    ChannelRepository& channels_;
    MessageRepository& messages_;
    MessageEventBus& event_bus_;
};

}  // namespace hangout
