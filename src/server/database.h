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
    // TODO:: it would be a great idea to specify data types used by the database

public:
    Database(const int flags = SQLite::OPEN_READONLY) :
        mpDatabase{std::make_unique<SQLite::Database>(DATABASE_PATH, flags)}
    {}

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
        static bool isValidProfession(const Profession profession)
        {
            return (profession >= Profession::INTERNIST) && (profession <= Profession::OCULIST);
        }
        static int32_t professionStr2Int(const std::string& profession)
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
        static std::string profession2Str(const Profession profession)
        {
            switch(profession)
            {
                case Profession::INTERNIST: return "Internist";
                case Profession::GASTROENTEROLOGIST: return "Gastroenterologist";
                case Profession::PULMONOLOGIST: return "Pulmonologist";
                case Profession::OCULIST: return "Oculist";
                default: throw std::runtime_error{"invalid profession"};
            }
        }

        int32_t id;
        std::string pesel;
        std::string password;
        std::string first_name;
        std::string last_name;
        std::string email;
        std::string note;
        Role role;
        // TODO: it should be named profession in the database
        Profession type;
        std::string phone;
    };
    bool addUser(const User& user);
    bool updateUser(const User& user);
    std::optional<User> getUserByPesel(const std::string& pesel);
    std::optional<User> getUserById(const int32_t id);
    std::vector<User> getUsersByRole(const User::Role role);

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
        static std::string status2Str(const Status status)
        {
            switch(status)
            {
                case Status::REQUESTED: return "Requested";
                case Status::REJECTED: return "Rejected";
                case Status::CANCELLED: return "Cancelled";
                case Status::SCHEDULED: return "Scheduled";
                case Status::COMPLETED: return "Completed";
                default: throw std::runtime_error{"invalid status"};
            }
        }

        int32_t id;
        int32_t patient_id;
        int32_t doctor_id;
        Status status;
        std::string date;
        std::string time;
        // TODO: it should be named prescription
        std::string receipt;
        User::Profession profession;
    };
    bool addVisit(const int32_t patientId,
        const std::string& date,
        const std::string& time,
        const int32_t professionId);
    std::vector<Visit> getVisitsByPatientPesel(const std::string& pesel);
    std::vector<Visit> getVisitsByDoctorIdAndDate(const int32_t id, const std::string& date);
    bool updateVisitStatus(const int32_t visitId, const Visit::Status status);
    std::optional<Visit> getVisitById(const int32_t id);
    std::vector<Visit> getVisitsByStatus(const Visit::Status status);

    struct VisitAvailability final
    {
        enum class Status
        {
            FREE,
            TAKEN,
            YOUR_VISIT
        } status;

        std::vector<int32_t> takenDoctorsIds;
    };
    VisitAvailability checkAvailabilityOfVisit(const int32_t patientId,
        const int32_t profession,
        const std::string& date,
        const std::string time);
    int32_t getNumberOfRequestedVisitPerPatientId(const int32_t patientId);

    std::vector<User> getFreeDoctors(const int32_t& profession, const std::vector<int32_t> takenDcotorsIds);

private:
    std::unique_ptr<SQLite::Database> mpDatabase;
};
}