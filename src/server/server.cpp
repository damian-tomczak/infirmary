#include "server.h"

#include "drogon/drogon.h"
#include "fmt/core.h"
#include "fmt/color.h"
#include "SQLiteCpp/SQLiteCpp.h"

using namespace std::chrono_literals;

// TODO: Handle this at the level of cmake
#define IP "127.0.0.1"
#ifdef NDEBUG
    #define PORT 443
#else
    #define PORT 80
#endif

namespace tsrpp
{
Server::Server()
{
    fmt::print("SQlite3 version {} ({})\n", ::SQLite::VERSION, ::SQLite::getLibVersion());
    fmt::print("SQliteC++ version {}\n", SQLITECPP_VERSION);
}

void Server::run()
{
#ifdef NDEBUG
    constexpr std::string_view startUrl{"https://" IP ":" STR(PORT)};
#else
    constexpr std::string_view startUrl{"http://" IP ":" STR(PORT)};
#endif
    // TODO: Compilation date and time should be printed below
    fmt::print("\nServer::{} built at {} started on {} !\n\n",
        PROJECT_NAME,
        "[TODO]",
        fmt::styled(startUrl, fmt::fg(fmt::color::green))
    );

    // TODO: it should be placed in json instead of hard-coded
    drogon::app()
        .setDocumentRoot(DOCUMENT_ROOT_PATH)
        .enableSession(24h)
#ifdef NDEBUG
        .setSSLFiles("", "") // TODO: fix ssl
#endif
        .addListener(IP, PORT)
        .run();
}
}
