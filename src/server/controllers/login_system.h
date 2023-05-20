#include "tools.hpp"
#include "database.h"

#include <drogon/HttpSimpleController.h>
#include <drogon/HttpResponse.h>

#include <regex>

class LoginSystem
{
public:
    template <bool isTrowException = true>
    static bool isUserShouldSeeThis(
        const drogon::HttpRequestPtr& pReq,
        drogon::HttpResponsePtr& pResp,
        const tsrpp::Database::User::Role& role)
    {
        auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        if (pUser == std::nullopt)
        {
            pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/login"));
            return false;
        }
        else if (pUser->role == role)
        {
            return true;
        }

        if constexpr (isTrowException)
        {
            throw std::runtime_error{"you are not allowed to see this"};
        }

        return false;
    }

    static bool validPesel(const std::string& pesel)
    {
        std::regex regex(R"(^\d{11}$)");
        if (!std::regex_match(pesel, regex))
        {
            return false;
        }
        std::array<int, 10> weights{1, 3, 7, 9, 1, 3, 7, 9, 1, 3};
        int sum{};
        for (size_t i{0}; i < weights.size(); ++i)
        {
            sum += weights.at(i) * (pesel.at(i) - '0');
        }
        auto peselLastDigit{pesel.back() - '0'};
        auto sumLastDigit{sum % 10};
        if ((peselLastDigit != 0) && ((10 - (sumLastDigit) != peselLastDigit)))
        {
            return false;
        }
        else if ((peselLastDigit == 0) && (sumLastDigit != peselLastDigit))
        {
            return false;
        }

        return true;
    }

    static bool validPassword(const std::string& password)
    {
        std::regex regex(R"(^(?=.*\d).{9,}$)");
        return std::regex_match(password, regex);
    }

    static bool validFirstName(const std::string& firstName)
    {
        std::regex regex(R"(^[a-zA-ZąćęłńóśźżĄĆĘŁŃÓŚŹŻ]{4,}$)");
        return std::regex_match(firstName, regex);
    }

    static bool validLastName(const std::string& lastName)
    {
        std::regex regex(R"(^[a-zA-ZąćęłńóśźżĄĆĘŁŃÓŚŹŻ]{4,}$)");
        return std::regex_match(lastName, regex);
    }

    static bool validEmail(const std::string& email)
    {
        std::regex regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(email, regex);
    }

protected:
    bool isAlreadyLogged(const drogon::HttpRequestPtr& pReq, drogon::HttpResponsePtr& pResp);
};

class LoginController final : public drogon::HttpSimpleController<LoginController>, public LoginSystem
{
public:
    PATH_LIST_BEGIN
    PATH_ADD("/login");
    PATH_LIST_END

    enum class LoginStatus
    {
        DEFAULT,
        REGISTRATION_SUCCESS,
        INCORRECT_PESEL,
        INCORRECT_PASSWORD,
        FAILURE,
        SUCCESS
    };

    void asyncHandleHttpRequest(
        const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback) override try
    {
        drogon::HttpResponsePtr pResp;
        if (isAlreadyLogged(pReq, pResp))
        {
            callback(pResp);
            return;
        }

        LoginStatus loginStatus{};
        if (pReq->method() == drogon::HttpMethod::Post)
        {
            loginStatus = postLogin(pReq);
            if (loginStatus == LoginStatus::SUCCESS)
            {
                tsrpp::Database database{SQLite::OPEN_READWRITE};
                pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel"));
#ifndef NDEBUG
    auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
#else
    std::optional<std::string> pesel{"00302800690"};
#endif
                if (!pesel)
                {
                    throw std::runtime_error{"login was successful, after which the pesel couldn't be found"};
                }
                auto user{database.getUserByPesel(*pesel)};
                if (!user)
                {
                    throw std::runtime_error{"login was successful, after which the user couldn't be found"};
                }
                pReq->getSession()->insert("user", *user);
                callback(pResp);
                return;
            }
        }

        drogon::HttpViewData data;

        auto isRegistrationSuccess{pReq->getOptionalParameter<bool>("registrationSuccess").value_or(false)};
        if (isRegistrationSuccess)
        {
            loginStatus = LoginStatus::REGISTRATION_SUCCESS;
        }
        data.insert("loginStatus", static_cast<int>(loginStatus));

        pResp = drogon::HttpResponse::newHttpViewResponse("login", data);
        callback(pResp);
    }
    catch(const std::exception& e)
    {
        ERROR_PAGE(e);
    }


    LoginStatus postLogin(const drogon::HttpRequestPtr& pReq);
};

class LogoutController final : public drogon::HttpSimpleController<LogoutController>, public LoginSystem
{
public:
    PATH_LIST_BEGIN
    PATH_ADD("/logout");
    PATH_LIST_END

    void asyncHandleHttpRequest(
        const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback) override
    {
        const auto redirectionUrl = tsrpp::createUrl("/");
        drogon::HttpResponsePtr pResp = drogon::HttpResponse::newRedirectionResponse(redirectionUrl);
        pReq->getSession()->erase("user");
        callback(pResp);
    }
};

class RegisterController : public drogon::HttpSimpleController<RegisterController>, public LoginSystem
{
public:
    PATH_LIST_BEGIN
    PATH_ADD("/register");
    PATH_LIST_END

    enum class RegistrationStatus
    {
        DEFAULT,
        INCORRECT_PESEL,
        INCORRECT_FIRST_NAME,
        INCORRECT_LAST_NAME,
        INCORRECT_PASSWORD,
        INCORRECT_EMAIL,
        DIFFERENT_PASSWORDS,
        ALREADY_EXISTS,
        SUCCESS
    };

    void asyncHandleHttpRequest(
        const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback) override try
    {
        drogon::HttpResponsePtr pResp;
        if (isAlreadyLogged(pReq, pResp))
        {
            callback(pResp);
            return;
        }

        RegistrationStatus registrationStatus{};
        if (pReq->method() == drogon::HttpMethod::Post)
        {
            registrationStatus = postRegister(pReq);
        }

        if (registrationStatus == RegistrationStatus::SUCCESS)
        {
            pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/login?registrationSuccess=true"));
            callback(pResp);
            return;
        }

        drogon::HttpViewData data;
        data.insert("registrationStatus", static_cast<int>(registrationStatus));

        pResp = drogon::HttpResponse::newHttpViewResponse("registration", data);
        callback(pResp);
    }
    catch(const std::exception& e)
    {
        ERROR_PAGE(e);
    }


    RegistrationStatus postRegister(const drogon::HttpRequestPtr& pReq);
};