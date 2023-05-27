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

LoginController::LoginStatus LoginController::postLogin(const drogon::HttpRequestPtr& pReq)
{
    tsrpp::Database database{SQLite::OPEN_READWRITE};

    auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
    if ((!pesel) || (pesel->length() == 0) || (!validPesel(*pesel)))
    {
        return LoginStatus::INCORRECT_PESEL;
    }

    auto password{pReq->getOptionalParameter<std::string>("password")};
    if ((!password) || (password->length() == 0) || (!validPassword(*password)))
    {
        return LoginStatus::INCORRECT_PASSWORD;
    }

    auto user{database.getUserByPesel(*pesel)};
    if (user)
    {
        if (tsrpp::verifyPassword(*password, user->password))
        {
            return LoginStatus::SUCCESS;
        }
    }

    return LoginStatus::FAILURE;
}

RegisterController::RegistrationStatus RegisterController::postRegister(const drogon::HttpRequestPtr& pReq)
{
    tsrpp::Database database{SQLite::OPEN_READWRITE};

    auto firstName{pReq->getOptionalParameter<std::string>("firstName")};
    if ((!firstName) || (firstName->length() == 0) || (!validFirstName(*firstName)))
    {
        return RegistrationStatus::INCORRECT_FIRST_NAME;
    }

    auto lastName{pReq->getOptionalParameter<std::string>("lastName")};
    if ((!lastName) || (lastName->length() == 0) || (!validLastName(*lastName)))
    {
        return RegistrationStatus::INCORRECT_LAST_NAME;
    }

    auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
    if ((!pesel) || (pesel->length() == 0) || (!validPesel(*pesel)))
    {
        return RegistrationStatus::INCORRECT_PESEL;
    }

    auto email{pReq->getOptionalParameter<std::string>("email")};
    if ((!email) || (email->length() == 0) || (!validEmail(*email)))
    {
        return RegistrationStatus::INCORRECT_EMAIL;
    }

    auto phone{pReq->getOptionalParameter<std::string>("phone")};
    if ((!phone) || (phone->length() == 0) || (!validPhone(*phone)))
    {
        return RegistrationStatus::INCORRECT_PHONE;
    }

    auto password{pReq->getOptionalParameter<std::string>("password")};
    if ((!password) || (password->length() == 0) || (!validPassword(*password)))
    {
        return RegistrationStatus::INCORRECT_PASSWORD;
    }

    auto repeatedPassword{pReq->getOptionalParameter<std::string>("repeatedPassword")};
    if (*password != *repeatedPassword)
    {
        return RegistrationStatus::DIFFERENT_PASSWORDS;
    }

    auto hasUser{database.getUserByPesel(pesel.value())};
    if (hasUser)
    {
        return RegistrationStatus::ALREADY_EXISTS;
    }

    auto hashedPassword = tsrpp::hashPassword(*password);

    auto note{pReq->getOptionalParameter<std::string>("note").value_or("")};

    if (!database.addUser({
        .pesel{*pesel},
        .password{hashedPassword},
        .first_name{*firstName},
        .last_name{*lastName},
        .email{*email},
        .note{note},
        .phone{*phone}}))
    {
        throw std::runtime_error("couldn't add user");
    }

    return RegistrationStatus::SUCCESS;
}