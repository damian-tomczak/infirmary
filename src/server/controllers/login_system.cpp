#include "login_system.h"

bool LoginSystem::isAlreadyLogged(const drogon::HttpRequestPtr& pReq, drogon::HttpResponsePtr& pResp)
{
    auto user{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    if (user)
    {
        pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel"));
        return true;
    }

    return false;
}

LoginController::LoginErrorCode LoginController::postLogin(const drogon::HttpRequestPtr& pReq)
{
    tsrpp::Database database{SQLite::OPEN_READWRITE};

    auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
    if ((!pesel) || (pesel->length() == 0) || (!validPesel(*pesel)))
    {
        return LoginErrorCode::INCORRECT_PESEL;
    }

    auto password{pReq->getOptionalParameter<std::string>("password")};
    if ((!password) || (password->length() == 0) || (!validPassword(*password)))
    {
        return LoginErrorCode::INCORRECT_PASSWORD;
    }

    auto user{database.getUserByPesel(*pesel)};
    if (user)
    {
        if (tsrpp::verifyPassword(*password, user->password))
        {
            return LoginErrorCode::SUCCESS;
        }
    }

    return LoginErrorCode::FAILURE;
}