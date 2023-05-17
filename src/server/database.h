#include "tools.hpp"

#include "SQLiteCpp/SQLiteCpp.h"
#include "fmt/core.h"
#include "fmt/color.h"
#include "fmt/ostream.h"

#include <iostream>
#include <stdexcept>
#include <functional>
#include <optional>

namespace tsrpp
{
// TODO: use GLFS for database.db
class Database final
{
public:
    Database(const int flags = SQLite::OPEN_READONLY) :
        mpDatabase{std::make_unique<SQLite::Database>(DATABASE_PATH, flags)}
    {}
    ~Database() = default;

    struct User final
    {
        enum class Role
        {
            PATIENT,
            DOCTOR,
            RECEPTIONIST
        };
        enum class Type
        {
            INTERNIST,
            GASTROENTEROLOGIST,
            PULMONOLOGIST,
            OCULIST
        };

        std::int32_t id;
        std::string pesel;
        std::string password;
        std::string first_name;
        std::string last_name;
        std::string email;
        std::string note;
        Role role;
        Type type;
    };
    bool addUser(const User& user);
    std::optional<Database::User> getUserbyPesel(const std::string& pesel);
    std::optional<Database::User> getUserbyId(const std::uint32_t id);

    struct Visit final
    {
        enum class Status
        {
            REQUESTED,
            REJECTED,
            CANCELLED,
            SCHEDULED,
            COMPLETED
        };
        std::int32_t id;
        std::int32_t patient_id;
        std::int32_t doctor_id;
        Status status;
        std::string date;
        std::string time;
    };
    std::vector<Visit> getVisitsByPatient(const std::string& pesel);

private:
    std::unique_ptr<SQLite::Database> mpDatabase;
};
}