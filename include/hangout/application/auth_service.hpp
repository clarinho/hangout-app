#pragma once

#include <string>

#include "hangout/domain/models.hpp"

namespace hangout {

class UserRepository;
class SessionRepository;
class ServerRepository;

class AuthService {
public:
    AuthService(UserRepository& users, SessionRepository& sessions, ServerRepository& servers);

    LoginResult login(const std::string& username);
    SessionContext require_session(const std::string& token);
    User update_profile(const SessionContext& session,
                        const std::string& display_name,
                        const std::string& status_text,
                        const std::string& user_status,
                        const std::string& avatar_color,
                        const std::string& avatar_url);
    void heartbeat(const SessionContext& session);

private:
    UserRepository& users_;
    SessionRepository& sessions_;
    ServerRepository& servers_;
};

}  // namespace hangout
