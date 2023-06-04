#pragma once

#include "mailio/message.hpp"
#include "mailio/smtp.hpp"

#include <optional>

// TODO: it should take config from the file
// TODO: mailio puts whitespaces randomly, it needs a investigation
class Mailer final
{
using message = mailio::message;
using mail_address = mailio::mail_address;
using smtps = mailio::smtps;

static constexpr auto sender{"infirmary"};
static constexpr auto hostName{"smtp.gmail.com"};
static constexpr auto hostPort{587};

public:
    static void init(const std::string& login, const std::string& password)
    {
        pLogin = std::make_optional(login);
        pPassword = std::make_optional(password);
    }

    static void sendMail(const std::string& recipient, const std::string& content, const std::string& title = "infirmary")
    {
        // TODO: login/email validation
        if ((pLogin == std::nullopt) || (!pLogin->length()) ||
            (pPassword == std::nullopt) || (!pPassword->length()))
        {
            throw std::runtime_error{"invalid mailers configuration"};
        }

        message msg;
        msg.content_type(message::media_type_t::TEXT, "html", "utf-8");
        msg.from(mail_address(sender, sender));
        msg.add_recipient(mail_address(recipient, recipient));
        msg.subject(title);
        msg.content(content);

        smtps conn(hostName, hostPort);
        conn.authenticate(*pLogin, *pPassword, smtps::auth_method_t::START_TLS);
        conn.submit(msg);
    }

private:
    inline static std::optional<std::string> pLogin;
    inline static std::optional<std::string> pPassword;

};