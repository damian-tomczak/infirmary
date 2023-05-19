#include "tools.hpp"

#include "SQLiteCpp/SQLiteCpp.h"
#include "fmt/core.h"
#include "fmt/color.h"
#include "fmt/ostream.h"

#include <iostream>
#include <stdexcept>
#include <functional>
#include <optional>
#include <queue>

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
        enum class Profession
        {
            INTERNIST,
            GASTROENTEROLOGIST,
            PULMONOLOGIST,
            OCULIST
        };
        static std::int32_t roleString2Int(const std::string& profession)
        {
            if (profession == "internist")
            {
                return 0;
            }
            else if (profession == "gastroenterologist")
            {
                return 1;
            }
            else if (profession == "pulmonologist")
            {
                return 2;
            }
            else if (profession == "oculist")
            {
                return 3;
            }

            return -1;
        }

        std::int32_t id;
        std::string pesel;
        std::string password;
        std::string first_name;
        std::string last_name;
        std::string email;
        std::string note;
        Role role;
        Profession type;
    };
    bool addUser(const User& user);
    bool updateUser(const User& user);
    std::optional<Database::User> getUserByPesel(const std::string& pesel);
    std::optional<Database::User> getUserById(const std::int32_t id);

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
        static std::string statusInt2Str(const Status status)
        {
            switch(status)
            {
                case Status::REQUESTED: return "Requested";
                case Status::REJECTED: return "Requested";
                case Status::CANCELLED: return "Cancelled";
                case Status::SCHEDULED: return "Scheduled";
                case Status::COMPLETED: return "Completed";
                default: throw std::runtime_error{"invalid status"};
            }
        }

        std::int32_t id;
        std::int32_t patient_id;
        std::int32_t doctor_id;
        Status status;
        std::string date;
        std::string time;
        std::string receipt;
    };
    bool addVisit(const std::int32_t patientId,
        const std::int32_t doctorId,
        const std::string& date, const std::string& time);
    std::vector<Visit> getVisitsByPatient(const std::string& pesel);
    bool updateVisitStatus(const std::int32_t visitId, const Visit::Status status);
    std::optional<Database::Visit> getVisitById(const std::int32_t id);

    struct VisitAvailability final
    {
        enum class Status
        {
            FREE,
            TAKEN,
            YOUR_VISIT
        } status;

        std::vector<std::int32_t> takenDoctorsIds;
    };
    VisitAvailability checkAvailabilityOfVisit(const std::int32_t patientId,
        const std::int32_t profession,
        const std::string& date,
        const std::string time);

    std::optional<std::int32_t> getFreeDoctor(const std::int32_t& profession, const std::vector<std::int32_t> takenDcotorsIds);

private:
    std::unique_ptr<SQLite::Database> mpDatabase;
};
}