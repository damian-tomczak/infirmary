#pragma once

#include <iostream>

#include "tools.hpp"
#include "mailer.hpp"

#include <memory>

namespace tsrpp
{
class Server final
{
    NOT_COPYABLE_AND_MOVEABLE(Server);

public:
    Server(const std::string& mailLogin, const std::string& mailPassword);

    void run();
};
}