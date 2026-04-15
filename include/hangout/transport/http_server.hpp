#pragma once

#include <string>

#include <httplib.h>

namespace hangout {

class AuthService;
class ChatService;
class SocialService;

class HttpServer {
public:
    HttpServer(AuthService& auth_service,
               ChatService& chat_service,
               SocialService& social_service,
               std::string host,
               int port);
    void run();

private:
    void configure_routes();

    AuthService& auth_service_;
    ChatService& chat_service_;
    SocialService& social_service_;
    std::string host_;
    int port_;
    httplib::Server server_;
};

}  // namespace hangout
