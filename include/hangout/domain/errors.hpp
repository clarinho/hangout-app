#pragma once

#include <stdexcept>
#include <string>

namespace hangout {

class AppError : public std::runtime_error {
public:
    AppError(int http_status, std::string code, std::string message)
        : std::runtime_error(std::move(message)),
          http_status_(http_status),
          code_(std::move(code)) {}

    [[nodiscard]] int http_status() const noexcept { return http_status_; }
    [[nodiscard]] const std::string& code() const noexcept { return code_; }

private:
    int http_status_;
    std::string code_;
};

class ValidationError : public AppError {
public:
    explicit ValidationError(const std::string& message)
        : AppError(400, "validation_error", message) {}
};

class UnauthorizedError : public AppError {
public:
    explicit UnauthorizedError(const std::string& message)
        : AppError(401, "unauthorized", message) {}
};

class NotFoundError : public AppError {
public:
    explicit NotFoundError(const std::string& message)
        : AppError(404, "not_found", message) {}
};

class StorageError : public AppError {
public:
    explicit StorageError(const std::string& message)
        : AppError(500, "storage_error", message) {}
};

}  // namespace hangout
