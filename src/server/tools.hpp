#pragma once

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <string_view>
#include <cstring>
#include <array>
#include <string>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <chrono>

#define NOT_COPYABLE(TypeName)           \
TypeName(const TypeName&)=delete;        \
TypeName& operator=(TypeName&)=delete;

#define NOT_MOVEABLE(TypeName)            \
TypeName(const TypeName&&)=delete;        \
TypeName& operator=(TypeName&&)=delete;

#define NOT_COPYABLE_AND_MOVEABLE(TypeName) \
NOT_COPYABLE(TypeName)                      \
NOT_MOVEABLE(TypeName)

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define ERROR_PAGE(exception)                                                                        \
    drogon::HttpResponsePtr pResp;                                                                   \
    fmt::print(std::cerr, fmt::format(fmt::fg(fmt::color::red), "tsrpp::exception {}\n", e.what())); \
    pResp = drogon::HttpResponse::newHttpResponse();                                                 \
    auto body{"<p>Something went wrong... " + std::string{exception.what()} + "</p>"                 \
        R"(<p><a href="mailto:contact@damian-tomczak.pl">Technical Support</a></p>)"                 \
        R"(<p><a href="/">Return to the Welcome Page</a></p>)"};                                     \
    pResp->setBody(body);                                                                            \
    callback(pResp);

namespace tsrpp
{
inline constexpr std::string_view salt{"tsrpp"};

inline std::string hashPassword(const std::string& password)
{
    std::string passwordWithSalt(password);
    passwordWithSalt += salt;

    const EVP_MD* pMd{};
    EVP_MD_CTX* pContext{};
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int hashLen{};
    auto errorCode{1};

    pMd = EVP_sha256();
    pContext = EVP_MD_CTX_new();
    if (pMd == nullptr || pContext == nullptr)
    {
        throw std::runtime_error{"OpenSSL couldn't create context"};
    }

    errorCode = EVP_DigestInit_ex(pContext, pMd, nullptr);
    errorCode = EVP_DigestUpdate(pContext, password.c_str(), password.size());
    errorCode = EVP_DigestFinal_ex(pContext, hash.data(), &hashLen);
    EVP_MD_CTX_free(pContext);

    if (errorCode != 1)
    {
        throw std::runtime_error{"OpenSSL couldn't hashed the password"};
    }

    std::string result(hashLen * 2, '\0');
    for (unsigned int i{}; i < hashLen; ++i)
    {
        sprintf(&result[i*2], "%02x", hash[i]);
    }

    return result;
}

inline bool verifyPassword(const std::string& password, const std::string& hash)
{
    return (hashPassword(password) == hash);
}

inline std::string createUrl(const std::string& path)
{
#ifdef NDEBUG
    constexpr std::string_view prefix{"https://" DOMAIN_NAME};
#else
    constexpr std::string_view prefix{"http://" DOMAIN_NAME};
#endif
    return std::string{prefix} + path;
}

// TODO: there is no available C++20's sys_days feature thus
// https://www.codespeedy.com/check-if-a-given-date-is-weekend-or-not-in-cpp/
inline bool isWeekend(const int32_t day, const int32_t month, int32_t year)
{
    static std::array<int32_t, 12> magic{ 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    year -= (month < 3);
    auto weekday{( year + year / 4 - year / 100 + year / 400 + magic[month - 1] + day) % 7};

    if((weekday == 0) || (weekday == 6))
    {
        return true;
    }

    return false;
}
}