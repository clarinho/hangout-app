#include "hangout/application/auth_service.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <random>
#include <regex>

#include "hangout/domain/errors.hpp"
#include "hangout/storage/repositories.hpp"

namespace hangout {
namespace {

constexpr std::int64_t kSessionTtlMs = 30LL * 24 * 60 * 60 * 1000;

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

std::string generate_token() {
    static thread_local std::mt19937_64 engine(std::random_device {}());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    constexpr char kHex[] = "0123456789abcdef";

    std::string token;
    token.reserve(64);
    for (int i = 0; i < 32; ++i) {
        const auto value = static_cast<unsigned int>(dist(engine));
        token.push_back(kHex[(value >> 4U) & 0x0FU]);
        token.push_back(kHex[value & 0x0FU]);
    }
    return token;
}

void validate_username(const std::string& username) {
    static const std::regex pattern(R"(^[A-Za-z0-9_-]{3,32}$)");
    if (!std::regex_match(username, pattern)) {
        throw ValidationError("Username must be 3-32 chars and use letters, numbers, '_' or '-'.");
    }
}

}  // namespace

AuthService::AuthService(UserRepository& users, SessionRepository& sessions, ServerRepository& servers)
    : users_(users), sessions_(sessions), servers_(servers) {}

LoginResult AuthService::login(const std::string& username) {
    const std::string normalized = trim_copy(username);
    validate_username(normalized);

    auto user = users_.find_by_username(normalized);
    const auto timestamp = now_ms();
    if (!user.has_value()) {
        user = users_.create(normalized, timestamp);
        servers_.add_user_to_all_servers(user->id, timestamp);
    }

    const auto session =
        sessions_.create(user->id, generate_token(), timestamp, timestamp + kSessionTtlMs);

    return LoginResult { .user = *user, .session = session };
}

SessionContext AuthService::require_session(const std::string& token) {
    if (token.empty()) {
        throw UnauthorizedError("Missing bearer token.");
    }

    const auto session = sessions_.find_valid_by_token(token, now_ms());
    if (!session.has_value()) {
        throw UnauthorizedError("Session is invalid or expired.");
    }

    const auto user = users_.find_by_id(session->user_id);
    if (!user.has_value()) {
        throw UnauthorizedError("Session user no longer exists.");
    }

    return SessionContext { .user = *user, .session = *session };
}

User AuthService::update_profile(const SessionContext& session,
                                 const std::string& display_name,
                                 const std::string& status_text,
                                 const std::string& user_status,
                                 const std::string& avatar_color) {
    const auto name = trim_copy(display_name);
    const auto status = trim_copy(status_text);
    const auto availability = trim_copy(user_status).empty() ? "online" : trim_copy(user_status);
    const auto color = trim_copy(avatar_color).empty() ? "#c315d2" : trim_copy(avatar_color);
    if (name.size() < 2U || name.size() > 40U) {
        throw ValidationError("Display name must be 2-40 characters.");
    }
    if (status.size() > 80U) {
        throw ValidationError("Status text cannot exceed 80 characters.");
    }
    if (availability != "online" && availability != "idle" && availability != "dnd" && availability != "offline") {
        throw ValidationError("User status must be online, idle, dnd, or offline.");
    }
    return users_.update_profile(session.user.id, name, status, availability, color, now_ms());
}

void AuthService::heartbeat(const SessionContext& session) {
    users_.heartbeat(session.user.id, now_ms());
}

}  // namespace hangout
