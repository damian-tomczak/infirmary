#include "database.h"

namespace SQLite
{
void assertion_failed(const char* apFile, const long apLine, const char* apFunc, const char* apExpr, const char* apMsg)
{
    assert(false && (std::string{apFile} + ":" + std::to_string(apLine) + ":" + " error: assertion failed (" + apExpr + ") in " + apFunc + "() with message \"" + apMsg + "\"\n").c_str());
}
}

namespace tsrpp
{
bool Database::addUser(const User& user)
{
    SQLite::Statement q{*mpDatabase, "INSERT INTO users(pesel, password, first_name, last_name, email, note, role)"
        "VALUES (:pesel, :password, :first_name, :last_name, :email, :note, 0)"};

    q.bind(":pesel", user.pesel);
    q.bind(":password", user.password);
    q.bind(":first_name", user.first_name);
    q.bind(":last_name", user.last_name);
    q.bind(":email", user.email);
    q.bind(":note", user.note);

    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

bool Database::updateUser(const User& user)
{
    SQLite::Statement q{*mpDatabase, "UPDATE users SET "
        "first_name = :first_name,"
        "last_name = :last_name,"
        "pesel = :pesel,"
        "email = :email,"
        "note = :note "
        "WHERE id = :id"
    };
    q.bind(":id", user.id);
    q.bind(":first_name", user.first_name);
    q.bind(":last_name", user.last_name);
    q.bind(":pesel", user.pesel);
    q.bind(":email", user.email);
    q.bind(":note", user.note);

    // TODO: add validation
    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

std::optional<Database::User> Database::getUserbyPesel(const std::string& pesel)
{
    std::optional<Database::User> result;
    SQLite::Statement q{*mpDatabase, "SELECT * FROM users WHERE pesel = :pesel LIMIT 1"};

    q.bind(":pesel", pesel);

    if (q.executeStep())
    {
        result = std::make_optional(User{
            .id{q.getColumn("id")},
            .pesel{q.getColumn("pesel").getString()},
            .password{q.getColumn("password").getString()},
            .first_name{q.getColumn("first_name").getString()},
            .last_name{q.getColumn("last_name").getString()},
            .email{q.getColumn("email").getString()},
            .note{q.getColumn("note").getString()},
            .role{User::Role{q.getColumn("role").getInt()}},
            .type{User::Profession{q.getColumn("type").getInt()}}
        });
    }

    return result;
}

std::optional<Database::User> Database::getUserbyId(const std::uint32_t id)
{
    std::optional<Database::User> result;
    SQLite::Statement q{*mpDatabase, "SELECT * FROM users WHERE id = :id LIMIT 1"};

    q.bind(":id", id);

    if (q.executeStep())
    {
        result = std::make_optional(User{
            .id{q.getColumn("id")},
            .pesel{q.getColumn("pesel").getString()},
            .password{q.getColumn("password").getString()},
            .first_name{q.getColumn("first_name").getString()},
            .last_name{q.getColumn("last_name").getString()},
            .email{q.getColumn("email").getString()},
            .note{q.getColumn("note").getString()},
            .role{User::Role{q.getColumn("role").getInt()}},
            .type{User::Profession{q.getColumn("type").getInt()}}
        });
    }

    return result;
}

std::vector<Database::Visit> Database::getVisitsByPatient(const std::string& pesel)
{
    std::vector<Database::Visit> result;

    std::optional<std::uint32_t> pPatientid;
    {
        SQLite::Statement q{*mpDatabase, "SELECT id FROM users WHERE pesel = :pesel LIMIT 1"};
        q.bind(":pesel", pesel);

        if (q.executeStep())
        {
            pPatientid = std::make_optional(q.getColumn("id"));
        }
    }

    SQLite::Statement q{*mpDatabase, "SELECT * FROM visits WHERE patient_id = :patient_id"};
    q.bind(":patient_id", *pPatientid);

    while (q.executeStep())
    {
        result.emplace_back(Visit{
            q.getColumn("id"),
            q.getColumn("patient_id").getInt(),
            q.getColumn("doctor_id").getInt(),
            Visit::Status{q.getColumn("status").getInt()},
            q.getColumn("date"),
            q.getColumn("time"),
            q.getColumn("receipt")
        });
    }

    return result;
}

bool Database::updateVisitStatus(const std::uint32_t visitId, const Database::Visit::Status status)
{
    SQLite::Statement q{*mpDatabase, "UPDATE visits SET status = :status WHERE id = :id"};
    q.bind(":status", static_cast<uint32_t>(status));
    q.bind(":id", visitId);

    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

Database::VisitAvailabilityStatus Database::checkAvailabilityOfVisit(const std::int32_t patientId,
    const std::string& profession,
    const std::string& date,
    const std::string time)
{
    std::uint32_t numberOfDoctorsWithProfession{};
    {
        SQLite::Statement q{*mpDatabase, "SELECT COUNT(*) FROM users WHERE role = :role AND type = :type"};
        q.bind(":role", static_cast<int>(User::Role::DOCTOR));
        q.bind(":type", profession);

        if (q.executeStep())
        {
            numberOfDoctorsWithProfession = q.getColumn(0);
        }
    }

    SQLite::Statement q{*mpDatabase,
        "SELECT patient_id, doctor_id, status FROM visits "
        "JOIN users ON doctor_id = users.id "
        "WHERE date = :date AND time = :time AND type = :type"};
    q.bind(":type", profession);
    q.bind(":date", date);
    q.bind(":time", time);

    std::uint32_t takenCounter{};
    while (q.executeStep())
    {
        auto visitPatientId{q.getColumn("patient_id").getInt()};
        if (visitPatientId == patientId)
        {
            return VisitAvailabilityStatus::YOUR_VISIT;
        }

        auto status{static_cast<Visit::Status>(q.getColumn("status").getInt())};
        if ((status != Visit::Status::REQUESTED) && (status != Visit::Status::SCHEDULED) && (status != Visit::Status::COMPLETED))
        {
            takenCounter++;
        }
    }

    if (takenCounter >= numberOfDoctorsWithProfession)
    {
        return VisitAvailabilityStatus::TAKEN;
    }

    return VisitAvailabilityStatus::FREE;
}
}