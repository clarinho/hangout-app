#include <exception>
#include <iostream>
#include <string>

#include "hangout/application/auth_service.hpp"
#include "hangout/application/chat_service.hpp"
#include "hangout/application/event_bus.hpp"
#include "hangout/application/social_service.hpp"
#include "hangout/storage/database.hpp"
#include "hangout/storage/repositories.hpp"
#include "hangout/transport/http_server.hpp"

int main(int argc, char** argv) {
    try {
        std::string host = "127.0.0.1";
        int port = 8080;
        std::string db_path = "data/hangout.db";

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--host" && i + 1 < argc) {
                host = argv[++i];
            } else if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
            } else if (arg == "--db" && i + 1 < argc) {
                db_path = argv[++i];
            } else {
                std::cerr << "Unknown argument: " << arg << '\n';
                return 1;
            }
        }

        hangout::Database database(db_path);
        database.initialize();

        hangout::UserRepository user_repository(database.connection());
        hangout::SessionRepository session_repository(database.connection());
        hangout::ServerRepository server_repository(database.connection());
        hangout::ChannelRepository channel_repository(database.connection());
        hangout::MessageRepository message_repository(database.connection());
        hangout::FriendRepository friend_repository(database.connection());
        hangout::DirectMessageRepository direct_message_repository(database.connection());

        hangout::NullMessageEventBus event_bus;
        hangout::AuthService auth_service(user_repository, session_repository, server_repository);
        hangout::ChatService chat_service(
            server_repository, channel_repository, message_repository, event_bus);
        hangout::SocialService social_service(friend_repository, user_repository, direct_message_repository);
        hangout::HttpServer server(auth_service, chat_service, social_service, host, port);
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}
