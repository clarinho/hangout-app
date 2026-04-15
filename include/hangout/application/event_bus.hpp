#pragma once

#include "hangout/domain/models.hpp"

namespace hangout {

class MessageEventBus {
public:
    virtual ~MessageEventBus() = default;
    virtual void publish_message(const MessageRecord& message) = 0;
};

class NullMessageEventBus final : public MessageEventBus {
public:
    void publish_message(const MessageRecord&) override {}
};

}  // namespace hangout
