#include "hangout/transport/http_server.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <httplib.h>
#include <json.hpp>

#include "hangout/application/auth_service.hpp"
#include "hangout/application/chat_service.hpp"
#include "hangout/application/social_service.hpp"
#include "hangout/domain/errors.hpp"

namespace hangout {

namespace httplib_detail {

class ServerHolder {
public:
    httplib::Server server;
};

}  // namespace httplib_detail

namespace {

using json = nlohmann::json;

std::string extract_bearer_token(const httplib::Request& req) {
    const auto auth_header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth_header.rfind(prefix, 0) != 0) {
        return {};
    }
    return auth_header.substr(prefix.size());
}

std::int64_t parse_id(const std::string& raw) {
    try {
        return std::stoll(raw);
    } catch (...) {
        throw ValidationError("Path id is invalid.");
    }
}

int parse_limit(const httplib::Request& req) {
    if (!req.has_param("limit")) {
        return 50;
    }

    try {
        return std::stoi(req.get_param_value("limit"));
    } catch (...) {
        throw ValidationError("Query parameter 'limit' must be an integer.");
    }
}

json user_json(const User& user) {
    return json {
        { "id", user.id },
        { "username", user.username },
        { "displayName", user.display_name.empty() ? user.username : user.display_name },
        { "statusText", user.status_text },
        { "userStatus", user.user_status.empty() ? "online" : user.user_status },
        { "avatarColor", user.avatar_color.empty() ? "#c315d2" : user.avatar_color },
        { "avatarUrl", user.avatar_url },
        { "lastSeenAtMs", user.last_seen_at_ms },
        { "createdAtMs", user.created_at_ms },
    };
}

json session_json(const Session& session) {
    return json {
        { "id", session.id },
        { "token", session.token },
        { "createdAtMs", session.created_at_ms },
        { "expiresAtMs", session.expires_at_ms },
    };
}

json server_json(const ServerSummary& server) {
    return json {
        { "id", server.id },
        { "name", server.name },
    };
}

json invite_json(const ServerInvite& invite) {
    return json {
        { "serverId", invite.server_id },
        { "serverName", invite.server_name },
        { "inviteCode", invite.invite_code },
    };
}

json channel_json(const ChannelSummary& channel) {
    return json {
        { "id", channel.id },
        { "serverId", channel.server_id },
        { "name", channel.name },
        { "position", channel.position },
    };
}

json member_json(const ServerMember& member) {
    return json {
        { "user", user_json(member.user) },
        { "role", member.role },
        { "joinedAtMs", member.joined_at_ms },
    };
}

json message_json(const MessageRecord& message) {
    json reactions = json::array();
    for (const auto& reaction : message.reactions) {
        reactions.push_back(json {
            { "emoji", reaction.emoji },
            { "count", reaction.count },
            { "reactedByMe", reaction.reacted_by_me },
        });
    }

    return json {
        { "id", message.id },
        { "channelId", message.channel_id },
        { "authorId", message.author_id },
        { "authorUsername", message.author_username },
        { "content", message.content },
        { "createdAtMs", message.created_at_ms },
        { "editedAtMs", message.edited_at_ms },
        { "reactions", reactions },
    };
}

json direct_conversation_json(const DirectConversationSummary& conversation) {
    return json {
        { "id", conversation.id },
        { "otherUser", user_json(conversation.other_user) },
    };
}

json direct_message_json(const DirectMessageRecord& message) {
    json reactions = json::array();
    for (const auto& reaction : message.reactions) {
        reactions.push_back(json {
            { "emoji", reaction.emoji },
            { "count", reaction.count },
            { "reactedByMe", reaction.reacted_by_me },
        });
    }

    return json {
        { "id", message.id },
        { "conversationId", message.conversation_id },
        { "authorId", message.author_id },
        { "authorUsername", message.author_username },
        { "content", message.content },
        { "createdAtMs", message.created_at_ms },
        { "editedAtMs", message.edited_at_ms },
        { "reactions", reactions },
    };
}

json reactions_json(const std::vector<ReactionSummary>& reactions) {
    json items = json::array();
    for (const auto& reaction : reactions) {
        items.push_back(json {
            { "emoji", reaction.emoji },
            { "count", reaction.count },
            { "reactedByMe", reaction.reacted_by_me },
        });
    }
    return items;
}

json friend_request_json(const FriendRequestRecord& request) {
    return json {
        { "id", request.id },
        { "user", user_json(request.user) },
        { "createdAtMs", request.created_at_ms },
    };
}

json friends_snapshot_json(const FriendsSnapshot& snapshot) {
    json friends = json::array();
    for (const auto& user : snapshot.friends) {
        friends.push_back(user_json(user));
    }

    json inbound = json::array();
    for (const auto& request : snapshot.inbound) {
        inbound.push_back(friend_request_json(request));
    }

    json outbound = json::array();
    for (const auto& request : snapshot.outbound) {
        outbound.push_back(friend_request_json(request));
    }

    return json {
        { "friends", friends },
        { "inbound", inbound },
        { "outbound", outbound },
    };
}

template <typename Fn>
void handle_errors(httplib::Response& res, Fn&& fn) {
    try {
        fn();
    } catch (const AppError& error) {
        res.status = error.http_status();
        res.set_content(json {
                            { "error", { { "code", error.code() }, { "message", error.what() } } },
                        }.dump(2),
                        "application/json");
    } catch (const std::exception& error) {
        res.status = 500;
        res.set_content(json {
                            { "error",
                              { { "code", "internal_error" }, { "message", error.what() } } },
                        }.dump(2),
                        "application/json");
    }
}

json parse_body(const httplib::Request& req) {
    if (req.body.empty()) {
        return json::object();
    }

    try {
        return json::parse(req.body);
    } catch (const json::parse_error&) {
        throw ValidationError("Request body is not valid JSON.");
    }
}

}  // namespace

HttpServer::HttpServer(AuthService& auth_service,
                       ChatService& chat_service,
                       SocialService& social_service,
                       std::string host,
                       int port)
    : auth_service_(auth_service),
      chat_service_(chat_service),
      social_service_(social_service),
      host_(std::move(host)),
      port_(port),
      server_(std::make_unique<httplib_detail::ServerHolder>()) {
    configure_routes();
}

HttpServer::~HttpServer() = default;

void HttpServer::configure_routes() {
    auto& server = server_->server;

    server.set_default_headers({
        { "Access-Control-Allow-Origin", "*" },
        { "Access-Control-Allow-Headers", "Content-Type, Authorization" },
        { "Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS" },
    });

    server.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    server.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    server.Post("/api/v1/auth/login", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto body = parse_body(req);
            const auto username = body.value("username", "");
            const auto result = auth_service_.login(username);

            res.status = 200;
            res.set_content(json {
                                { "user", user_json(result.user) },
                                { "session", session_json(result.session) },
                            }.dump(2),
                            "application/json");
        });
    });

    server.Get("/api/v1/me", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            res.set_content(json { { "user", user_json(session.user) } }.dump(2), "application/json");
        });
    });

    server.Post("/api/v1/me/profile", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto body = parse_body(req);
            const auto user = auth_service_.update_profile(session,
                                                           body.value("displayName", session.user.username),
                                                           body.value("statusText", ""),
                                                           body.value("userStatus", "online"),
                                                           body.value("avatarColor", "#c315d2"),
                                                           body.value("avatarUrl", ""));
            res.set_content(json { { "user", user_json(user) } }.dump(2), "application/json");
        });
    });

    server.Post("/api/v1/me/heartbeat", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            auth_service_.heartbeat(session);
            res.set_content("{\"ok\":true}", "application/json");
        });
    });

    server.Get("/api/v1/friends", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto snapshot = social_service_.list_friends(session);
            res.set_content(friends_snapshot_json(snapshot).dump(2), "application/json");
        });
    });

    server.Post("/api/v1/friends/requests", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto body = parse_body(req);
            const auto username = body.value("username", "");
            const auto request = social_service_.send_request(session, username);

            res.status = 201;
            res.set_content(json { { "request", friend_request_json(request) } }.dump(2),
                            "application/json");
        });
    });

    server.Post(R"(/api/v1/friends/requests/(\d+)/accept)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         social_service_.accept_request(session, parse_id(req.matches[1]));
                         res.set_content("{\"ok\":true}", "application/json");
                     });
                 });

    server.Post(R"(/api/v1/friends/requests/(\d+)/deny)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         social_service_.deny_request(session, parse_id(req.matches[1]));
                         res.set_content("{\"ok\":true}", "application/json");
                     });
                 });

    server.Delete(R"(/api/v1/friends/(\d+))",
                   [this](const httplib::Request& req, httplib::Response& res) {
                       handle_errors(res, [&]() {
                           const auto session =
                               auth_service_.require_session(extract_bearer_token(req));
                           social_service_.remove_friend(session, parse_id(req.matches[1]));
                           res.set_content("{\"ok\":true}", "application/json");
                       });
                   });

    server.Get("/api/v1/dms", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto conversations = social_service_.list_conversations(session);
            json items = json::array();
            for (const auto& conversation : conversations) {
                items.push_back(direct_conversation_json(conversation));
            }
            res.set_content(json { { "conversations", items } }.dump(2), "application/json");
        });
    });

    server.Post("/api/v1/dms", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto body = parse_body(req);
            const auto username = body.value("username", "");
            const auto conversation = social_service_.open_conversation(session, username);
            res.status = 201;
            res.set_content(json { { "conversation", direct_conversation_json(conversation) } }.dump(2),
                            "application/json");
        });
    });

    server.Get(R"(/api/v1/dms/(\d+)/messages)",
                [this](const httplib::Request& req, httplib::Response& res) {
                    handle_errors(res, [&]() {
                        const auto session =
                            auth_service_.require_session(extract_bearer_token(req));
                        const auto conversation_id = parse_id(req.matches[1]);
                        const auto messages = social_service_.list_direct_messages(
                            session, conversation_id, parse_limit(req));
                        json items = json::array();
                        for (const auto& message : messages) {
                            items.push_back(direct_message_json(message));
                        }
                        res.set_content(json { { "messages", items } }.dump(2), "application/json");
                    });
                });

    server.Post(R"(/api/v1/dms/(\d+)/messages)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto conversation_id = parse_id(req.matches[1]);
                         const auto body = parse_body(req);
                         const auto message = social_service_.send_direct_message(
                             session, conversation_id, body.value("content", ""));
                         res.status = 201;
                         res.set_content(json { { "message", direct_message_json(message) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Post(R"(/api/v1/dm-messages/(\d+)/reactions)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto body = parse_body(req);
                         const auto reactions = social_service_.toggle_direct_message_reaction(
                             session, parse_id(req.matches[1]), body.value("emoji", ""));
                         res.set_content(json { { "reactions", reactions_json(reactions) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Post(R"(/api/v1/dm-messages/(\d+)/edit)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto body = parse_body(req);
                         const auto message = social_service_.edit_direct_message(
                             session, parse_id(req.matches[1]), body.value("content", ""));
                         res.set_content(json { { "message", direct_message_json(message) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Delete(R"(/api/v1/dm-messages/(\d+))",
                   [this](const httplib::Request& req, httplib::Response& res) {
                       handle_errors(res, [&]() {
                           const auto session =
                               auth_service_.require_session(extract_bearer_token(req));
                           social_service_.delete_direct_message(session, parse_id(req.matches[1]));
                           res.set_content("{\"ok\":true}", "application/json");
                       });
                   });

    server.Get("/api/v1/servers", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto servers = chat_service_.list_servers(session);

            json items = json::array();
            for (const auto& server : servers) {
                items.push_back(server_json(server));
            }

            res.set_content(json { { "servers", items } }.dump(2), "application/json");
        });
    });

    server.Post("/api/v1/servers", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto body = parse_body(req);
            const auto name = body.value("name", "");
            const auto server = chat_service_.create_server(session, name);

            res.status = 201;
            res.set_content(json { { "server", server_json(server) } }.dump(2), "application/json");
        });
    });

    server.Post("/api/v1/servers/join", [this](const httplib::Request& req, httplib::Response& res) {
        handle_errors(res, [&]() {
            const auto session = auth_service_.require_session(extract_bearer_token(req));
            const auto body = parse_body(req);
            const auto code = body.value("inviteCode", "");
            const auto server = chat_service_.join_server(session, code);

            res.status = 200;
            res.set_content(json { { "server", server_json(server) } }.dump(2), "application/json");
        });
    });

    server.Get(R"(/api/v1/servers/(\d+)/invite)",
                [this](const httplib::Request& req, httplib::Response& res) {
                    handle_errors(res, [&]() {
                        const auto session =
                            auth_service_.require_session(extract_bearer_token(req));
                        const auto server_id = parse_id(req.matches[1]);
                        const auto invite = chat_service_.get_server_invite(session, server_id);

                        res.set_content(json { { "invite", invite_json(invite) } }.dump(2),
                                        "application/json");
                    });
                });

    server.Post(R"(/api/v1/servers/(\d+)/invite/regenerate)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto invite =
                             chat_service_.regenerate_server_invite(session, parse_id(req.matches[1]));
                         res.set_content(json { { "invite", invite_json(invite) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Get(R"(/api/v1/servers/(\d+)/members)",
                [this](const httplib::Request& req, httplib::Response& res) {
                    handle_errors(res, [&]() {
                        const auto session =
                            auth_service_.require_session(extract_bearer_token(req));
                        const auto members =
                            chat_service_.list_members(session, parse_id(req.matches[1]));
                        json items = json::array();
                        for (const auto& member : members) {
                            items.push_back(member_json(member));
                        }
                        res.set_content(json { { "members", items } }.dump(2), "application/json");
                    });
                });

    server.Get(R"(/api/v1/servers/(\d+)/channels)",
                [this](const httplib::Request& req, httplib::Response& res) {
                    handle_errors(res, [&]() {
                        const auto session =
                            auth_service_.require_session(extract_bearer_token(req));
                        const auto server_id = parse_id(req.matches[1]);
                        const auto channels = chat_service_.list_channels(session, server_id);

                        json items = json::array();
                        for (const auto& channel : channels) {
                            items.push_back(channel_json(channel));
                        }

                        res.set_content(json { { "channels", items } }.dump(2),
                                        "application/json");
                    });
                });

    server.Post(R"(/api/v1/servers/(\d+)/channels)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto server_id = parse_id(req.matches[1]);
                         const auto body = parse_body(req);
                         const auto name = body.value("name", "");
                         const auto channel =
                             chat_service_.create_channel(session, server_id, name);

                         res.status = 201;
                         res.set_content(json { { "channel", channel_json(channel) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Get(R"(/api/v1/channels/(\d+)/messages)",
                [this](const httplib::Request& req, httplib::Response& res) {
                    handle_errors(res, [&]() {
                        const auto session =
                            auth_service_.require_session(extract_bearer_token(req));
                        const auto channel_id = parse_id(req.matches[1]);
                        const auto messages = req.has_param("q")
                            ? chat_service_.search_messages(
                                  session, channel_id, req.get_param_value("q"), parse_limit(req))
                            : chat_service_.list_messages(session, channel_id, parse_limit(req));

                        json items = json::array();
                        for (const auto& message : messages) {
                            items.push_back(message_json(message));
                        }

                        res.set_content(json { { "messages", items } }.dump(2),
                                        "application/json");
                    });
                });

    server.Post(R"(/api/v1/channels/(\d+)/messages)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto channel_id = parse_id(req.matches[1]);
                         const auto body = parse_body(req);
                         const auto content = body.value("content", "");
                         const auto message =
                             chat_service_.send_message(session, channel_id, content);

                         res.status = 201;
                         res.set_content(json { { "message", message_json(message) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Post(R"(/api/v1/channels/(\d+)/position)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto body = parse_body(req);
                         chat_service_.set_channel_position(
                             session, parse_id(req.matches[1]), body.value("position", 0));
                         res.set_content("{\"ok\":true}", "application/json");
                     });
                 });

    server.Post(R"(/api/v1/messages/(\d+)/reactions)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto body = parse_body(req);
                         const auto reactions = chat_service_.toggle_message_reaction(
                             session, parse_id(req.matches[1]), body.value("emoji", ""));
                         res.set_content(json { { "reactions", reactions_json(reactions) } }.dump(2),
                                         "application/json");
                     });
                 });

    server.Delete(R"(/api/v1/messages/(\d+))",
                   [this](const httplib::Request& req, httplib::Response& res) {
                       handle_errors(res, [&]() {
                           const auto session =
                               auth_service_.require_session(extract_bearer_token(req));
                           chat_service_.delete_message(session, parse_id(req.matches[1]));
                           res.set_content("{\"ok\":true}", "application/json");
                       });
                   });

    server.Post(R"(/api/v1/messages/(\d+)/edit)",
                 [this](const httplib::Request& req, httplib::Response& res) {
                     handle_errors(res, [&]() {
                         const auto session =
                             auth_service_.require_session(extract_bearer_token(req));
                         const auto body = parse_body(req);
                         const auto message = chat_service_.edit_message(
                             session, parse_id(req.matches[1]), body.value("content", ""));
                         res.set_content(json { { "message", message_json(message) } }.dump(2),
                                         "application/json");
                     });
                 });
}

void HttpServer::run() {
    std::cout << "Hangout backend listening on http://" << host_ << ':' << port_ << '\n';
    server_->server.listen(host_, port_);
}

}  // namespace hangout
