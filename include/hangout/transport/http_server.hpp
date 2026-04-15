#pragma once

#include <memory>
#include <string>

namespace hangout {

namespace httplib_detail {
class ServerHolder;
}

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
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void run();

private:
    void configure_routes();

    AuthService& auth_service_;
    ChatService& chat_service_;
    SocialService& social_service_;
    std::string host_;
    int port_;
    std::unique_ptr<httplib_detail::ServerHolder> server_;
};

}  // namespace hangout
