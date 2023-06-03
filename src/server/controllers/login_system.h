#include "tools.hpp"
#include "database.h"

#include <drogon/HttpSimpleController.h>
#include <drogon/HttpResponse.h>

#include <regex>

// TODO: in the login system I forgot about correct name convention for variables with arrow operator overloading
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

    // TODO: functions below should be renamed with "is" naming convention
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

    static bool validPhone(const std::string& phone)
    {
        std::regex regex(R"(^\d{9}$)");
        auto tmp = std::regex_match(phone, regex);;
        return tmp;
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

    enum class LoginErrorCode
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

        LoginErrorCode LoginErrorCode{};
        if (pReq->method() == drogon::HttpMethod::Post)
        {
            LoginErrorCode = postLogin(pReq);
            if (LoginErrorCode == LoginErrorCode::SUCCESS)
            {
                tsrpp::Database database{SQLite::OPEN_READWRITE};
                pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel"));

                auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
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
            LoginErrorCode = LoginErrorCode::REGISTRATION_SUCCESS;
        }
        data.insert("LoginErrorCode", static_cast<int>(LoginErrorCode));

        pResp = drogon::HttpResponse::newHttpViewResponse("login", data);
        callback(pResp);
    }
    catch(const std::exception& e)
    {
        ERROR_PAGE(e);
    }


    LoginErrorCode postLogin(const drogon::HttpRequestPtr& pReq);
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

class RegistrationController : public drogon::HttpSimpleController<RegistrationController>, public LoginSystem
{
public:
    PATH_LIST_BEGIN
    PATH_ADD("/register");
    PATH_LIST_END

    // TODO: reorganize
    enum class RegistrationErrorCode
    {
        DEFAULT,
        INCORRECT_PESEL,
        INCORRECT_FIRSTNAME,
        INCORRECT_LASTNAME,
        INCORRECT_PASSWORD,
        INCORRECT_EMAIL,
        DIFFERENT_PASSWORDS,
        ALREADY_EXISTS,
        INCORRECT_PHONE,
        INCORRECT_PROFESSION,
        SUCCESS,
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

        RegistrationErrorCode RegistrationErrorCode{};
        if (pReq->method() == drogon::HttpMethod::Post)
        {
            RegistrationErrorCode = postRegister(pReq);
        }

        if (RegistrationErrorCode == RegistrationErrorCode::SUCCESS)
        {
            pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/login?registrationSuccess=true"));
            callback(pResp);
            return;
        }

        drogon::HttpViewData data;
        data.insert("RegistrationErrorCode", static_cast<int>(RegistrationErrorCode));

        pResp = drogon::HttpResponse::newHttpViewResponse("registration", data);
        callback(pResp);
    }
    catch(const std::exception& e)
    {
        ERROR_PAGE(e);
    }

    template<tsrpp::Database::User::Role role = tsrpp::Database::User::Role::PATIENT>
    static RegistrationController::RegistrationErrorCode postRegister(const drogon::HttpRequestPtr& pReq)
    {
        tsrpp::Database database{SQLite::OPEN_READWRITE};

        auto firstName{pReq->getOptionalParameter<std::string>("firstName")};
        if ((!firstName) || (firstName->length() == 0) || (!validFirstName(*firstName)))
        {
            return RegistrationErrorCode::INCORRECT_FIRSTNAME;
        }

        auto lastName{pReq->getOptionalParameter<std::string>("lastName")};
        if ((!lastName) || (lastName->length() == 0) || (!validLastName(*lastName)))
        {
            return RegistrationErrorCode::INCORRECT_LASTNAME;
        }

        auto pProfession{pReq->getOptionalParameter<int32_t>("profession")};
        if constexpr (role == tsrpp::Database::User::Role::DOCTOR)
        {
            if ((!pProfession) || (!tsrpp::Database::User::isValidProfession(tsrpp::Database::User::Profession{*pProfession})))
            {
                throw std::runtime_error{"profession is not valid"};
            }
        }

        auto email{pReq->getOptionalParameter<std::string>("email")};
        if ((!email) || (email->length() == 0) || (!validEmail(*email)))
        {
            return RegistrationErrorCode::INCORRECT_EMAIL;
        }

        auto pesel{pReq->getOptionalParameter<std::string>("pesel")};
        if ((!pesel) || (pesel->length() == 0) || (!validPesel(*pesel)))
        {
            return RegistrationErrorCode::INCORRECT_PESEL;
        }

        auto phone{pReq->getOptionalParameter<std::string>("phone")};
        if ((!phone) || (phone->length() == 0) || (!validPhone(*phone)))
        {
            return RegistrationErrorCode::INCORRECT_PHONE;
        }

        auto password{pReq->getOptionalParameter<std::string>("password")};
        if ((!password) || (password->length() == 0) || (!validPassword(*password)))
        {
            return RegistrationErrorCode::INCORRECT_PASSWORD;
        }

        auto repeatedPassword{pReq->getOptionalParameter<std::string>("repeatedPassword")};
        if (*password != *repeatedPassword)
        {
            return RegistrationErrorCode::DIFFERENT_PASSWORDS;
        }

        auto hasUser{database.getUserByPesel(pesel.value())};
        if (hasUser)
        {
            return RegistrationErrorCode::ALREADY_EXISTS;
        }

        auto hashedPassword = tsrpp::hashPassword(*password);

        auto note{pReq->getOptionalParameter<std::string>("note").value_or("")};

        // TODO: it should be refactored
        if constexpr (role != tsrpp::Database::User::Role::DOCTOR)
        {
            if (!database.addUser({
                .pesel{*pesel},
                .password{hashedPassword},
                .first_name{*firstName},
                .last_name{*lastName},
                .email{*email},
                .note{note},
                .role{tsrpp::Database::User::Role::PATIENT},
                .phone{*phone}
                }))
            {
                throw std::runtime_error("couldn't add user");
            }
        }
        else
        {
            if (!database.addUser({
                .pesel{*pesel},
                .password{hashedPassword},
                .first_name{*firstName},
                .last_name{*lastName},
                .email{*email},
                .note{note},
                .role{tsrpp::Database::User::Role::DOCTOR},
                .type{tsrpp::Database::User::Profession{*pProfession}},
                .phone{*phone}
                }))
            {
                throw std::runtime_error("couldn't add user");
            }
        }

        return RegistrationErrorCode::SUCCESS;
    }
};