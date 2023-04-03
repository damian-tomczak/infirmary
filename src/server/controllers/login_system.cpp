#include "login_system.h"

#include <regex>

bool validPesel(std::string pesel);
bool validPassword(std::string password);
bool validFirstName(std::string firstName);
bool validLastName(std::string lastName);
bool validEmail(std::string email);

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

    auto user{database.getUser(*pesel)};
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

    auto password{pReq->getOptionalParameter<std::string>("password")};
    if ((!password) || (password->length() == 0) || (!validPassword(*password)))
    {
        return RegistrationStatus::INCORRECT_PASSWORD;
    }

    auto hasUser{database.getUser(pesel.value())};
    if (hasUser)
    {
        return RegistrationStatus::ALREADY_EXISTS;
    }

    auto repeatedPassword{pReq->getOptionalParameter<std::string>("repeatedPassword")};
    if (password != repeatedPassword)
    {
        return RegistrationStatus::DIFFERENT_PASSWORDS;
    }

    auto hashedPassword = tsrpp::hashPassword(*password);

    auto note{pReq->getOptionalParameter<std::string>("note").value_or("")};

    if (!database.addUser({
        .pesel{std::move(*pesel)},
        .password{std::move(hashedPassword)},
        .first_name{std::move(*firstName)},
        .last_name{std::move(*lastName)},
        .email{std::move(*email)},
        .note{std::move(note)}}))
    {
        throw std::runtime_error("couldn't add user");
    }

    return RegistrationStatus::SUCCESS;
}

bool validPesel(std::string pesel)
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
    if ((sum % 10) != (pesel.back() - '0'))
    {
        return false;
    }

    return true;
}

bool validPassword(std::string password)
{
    std::regex regex(R"(^(?=.*\d).{9,}$)");
    return std::regex_match(password, regex);
}

bool validFirstName(std::string firstName)
{
    std::regex regex(R"(^[a-zA-Z]{4,}$)");
    return std::regex_match(firstName, regex);
}

bool validLastName(std::string lastName)
{
    std::regex regex(R"(^[a-zA-Z]{4,}$)");
    return std::regex_match(lastName, regex);
}

bool validEmail(std::string email)
{
    std::regex regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, regex);
}