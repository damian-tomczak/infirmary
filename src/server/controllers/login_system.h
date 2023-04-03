#include "tools.hpp"
#include "database.h"

#include <drogon/HttpSimpleController.h>
#include <drogon/HttpResponse.h>

class LoginSystemController
{
public:
    template <bool isOnlyLogged = false>
    static bool isUserShouldSeeThis(
        const drogon::HttpRequestPtr& pReq,
        drogon::HttpResponsePtr& pResp,
        const tsrpp::Database::User::Role& role = tsrpp::Database::User::Role::INVALID)
    {
        auto user{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        if (!user)
        {
            pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/login"));
            return false;
        }
        if constexpr (!isOnlyLogged)
        {
            if (role == tsrpp::Database::User::Role::INVALID)
            {
                throw std::runtime_error{"isUserShouldSeeThis was invoked with INVALID role"};
            }
            if (user->role != role)
            {
                static constexpr std::string_view response{
                    "<p>You are not allowed to see that</p>"
                    "<p><a href=\"" MAIN_URL "\">Return to the Welcome Page</a></p>"
                };
                pResp = drogon::HttpResponse::newHttpResponse();
                pResp->setBody(std::string{response});
                return false;
            }
        }

        return true;
    }

protected:
    bool isAlreadyLogged(const drogon::HttpRequestPtr& pReq, drogon::HttpResponsePtr& pResp)
    {
        auto user{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        if (user)
        {
            pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel"));
            return true;
        }

        return false;
    }
};

class LoginController final : public drogon::HttpSimpleController<LoginController>, public LoginSystemController
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
                auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
                if (!pesel)
                {
                    throw std::runtime_error{"login was successful, after which the pesel couldn't be found"};
                }
                auto user{database.getUser(*pesel)};
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
        ERROR_PAGE;
    }


    LoginStatus postLogin(const drogon::HttpRequestPtr& pReq);
};

class LogoutController final : public drogon::HttpSimpleController<LogoutController>, public LoginSystemController
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

class RegisterController : public drogon::HttpSimpleController<RegisterController>, public LoginSystemController
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
        ERROR_PAGE;
    }


    RegistrationStatus postRegister(const drogon::HttpRequestPtr& pReq);
};