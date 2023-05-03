#pragma once

#include <iostream>

#include "tools.hpp"

#include <memory>

namespace tsrpp
{
class Server final
{
    NOT_COPYABLE_AND_MOVEABLE(Server);
public:
    Server();

    void run();
};
}