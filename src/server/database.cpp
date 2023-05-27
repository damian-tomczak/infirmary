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
    SQLite::Statement q{*mpDatabase, "INSERT INTO users(pesel, password, first_name, last_name, email, note, role, phone)"
        "VALUES (:pesel, :password, :first_name, :last_name, :email, :note, 0, :phone)"};

    q.bind(":pesel", user.pesel);
    q.bind(":password", user.password);
    q.bind(":first_name", user.first_name);
    q.bind(":last_name", user.last_name);
    q.bind(":email", user.email);
    q.bind(":note", user.note);
    q.bind(":phone", user.phone);

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
        "password = :password,"
        "email = :email,"
        "note = :note,"
        "type = :profession,"
        "phone = :phone "
        "WHERE id = :id"
    };
    q.bind(":id", user.id);
    q.bind(":first_name", user.first_name);
    q.bind(":last_name", user.last_name);
    q.bind(":pesel", user.pesel);
    q.bind(":email", user.email);
    q.bind(":password", user.password);
    q.bind(":note", user.note);
    q.bind(":profession", static_cast<int32_t>(user.type));
    q.bind(":phone", user.phone);

    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

std::optional<Database::User> Database::getUserByPesel(const std::string& pesel)
{
    std::optional<User> result;
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
            .type{User::Profession{q.getColumn("type").getInt()}},
            .phone{{q.getColumn("phone").getString()}}
        });
    }

    return result;
}

std::optional<Database::User> Database::getUserById(const int32_t id)
{
    std::optional<User> result;
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
            .type{User::Profession{q.getColumn("type").getInt()}},
            .phone{{q.getColumn("phone").getString()}}
        });
    }

    return result;
}

std::vector<Database::User> Database::getUsersByRole(const User::Role role)
{
    std::vector<Database::User> result;

    SQLite::Statement q{*mpDatabase, "SELECT * FROM users WHERE role = :role"};
    q.bind(":role", static_cast<int32_t>(role));

    while (q.executeStep())
    {
        result.emplace_back(User{
            .id{q.getColumn("id")},
            .pesel{q.getColumn("pesel").getString()},
            .password{q.getColumn("password").getString()},
            .first_name{q.getColumn("first_name").getString()},
            .last_name{q.getColumn("last_name").getString()},
            .email{q.getColumn("email").getString()},
            .note{q.getColumn("note").getString()},
            .role{User::Role{q.getColumn("role").getInt()}},
            .type{User::Profession{q.getColumn("type").getInt()}},
            .phone{{q.getColumn("phone").getString()}}
        });
    }

    return result;
}

bool Database::addVisit(const int32_t patientId,
    const std::string& date,
    const std::string& time,
    const int32_t professionId
    )
{
    SQLite::Statement q{*mpDatabase, "INSERT INTO visits(patient_id, doctor_id, status, date, time, profession)"
        "VALUES (:patient_id, -1, :status, :date, :time, :profession)"};

    q.bind(":patient_id", patientId);
    q.bind(":status", static_cast<int32_t>(Visit::Status::REQUESTED));
    q.bind(":date", date);
    q.bind(":time", time);
    q.bind(":profession", professionId);

    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

std::vector<Database::Visit> Database::getVisitsByPatientPesel(const std::string& pesel)
{
    std::vector<Visit> result;

    std::optional<std::uint32_t> pPatientid;
    {
        SQLite::Statement q{*mpDatabase, "SELECT id FROM users WHERE pesel = :pesel LIMIT 1"};
        q.bind(":pesel", pesel);

        if (q.executeStep())
        {
            pPatientid = std::make_optional(q.getColumn("id"));
        }
    }

    if (!pPatientid)
    {
        throw std::runtime_error{"user with this pesel doesn't exist"};
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
            q.getColumn("receipt"),
            User::Profession{q.getColumn("profession").getInt()}
        });
    }

    return result;
}

std::vector<Database::Visit> Database::getVisitsByDoctorIdAndDate(const int32_t id, const std::string& date)
{
    std::vector<Visit> result;

    SQLite::Statement q{*mpDatabase, "SELECT * FROM visits WHERE doctor_id = :doctor_id AND date = :date"};
    q.bind(":doctor_id", id);
    q.bind(":date", date);

    while (q.executeStep())
    {
        result.emplace_back(Visit{
            q.getColumn("id"),
            q.getColumn("patient_id").getInt(),
            q.getColumn("doctor_id").getInt(),
            Visit::Status{q.getColumn("status").getInt()},
            q.getColumn("date"),
            q.getColumn("time"),
            q.getColumn("receipt"),
            User::Profession{q.getColumn("profession").getInt()}
        });
    }

    return result;
}

bool Database::updateVisitStatus(const int32_t visitId, const Database::Visit::Status status)
{
    SQLite::Statement q{*mpDatabase, "UPDATE visits SET status = :status WHERE id = :id"};
    q.bind(":status", static_cast<int32_t>(status));
    q.bind(":id", visitId);

    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

std::optional<Database::Visit> Database::getVisitById(const int32_t id)
{
    std::optional<Visit> result;
    SQLite::Statement q{*mpDatabase, "SELECT * FROM visits WHERE id = :id LIMIT 1"};

    q.bind(":id", id);

    if (q.executeStep())
    {
        result = std::make_optional(Visit{
            .id{q.getColumn("id")},
            .patient_id{q.getColumn("patient_id").getInt()},
            .doctor_id{q.getColumn("doctor_id").getInt()},
            .status{static_cast<Visit::Status>(q.getColumn("status").getInt())},
            .date{q.getColumn("date").getString()},
            .time{q.getColumn("time").getString()},
            .receipt{q.getColumn("receipt").getString()},
            .profession{User::Profession{q.getColumn("profession").getInt()}}
        });
    }

    return result;
}

std::vector<Database::Visit> Database::getVisitsByStatus(const Visit::Status status)
{
    std::vector<Visit> result;

    SQLite::Statement q{*mpDatabase, "SELECT * FROM visits WHERE status = :status"};
    q.bind(":status", static_cast<int32_t>(status));

    while (q.executeStep())
    {
        result.emplace_back(Visit{
            q.getColumn("id"),
            q.getColumn("patient_id").getInt(),
            q.getColumn("doctor_id").getInt(),
            Visit::Status{q.getColumn("status").getInt()},
            q.getColumn("date"),
            q.getColumn("time"),
            q.getColumn("receipt"),
            User::Profession{q.getColumn("profession").getInt()}
        });
    }

    return result;
}

bool Database::updateVisitDoctorId(const int32_t visitId, const int32_t doctorId)
{
    SQLite::Statement q{*mpDatabase, "UPDATE visits SET doctor_id = :doctorId WHERE id = :id"};
    q.bind(":doctorId", doctorId);
    q.bind(":id", visitId);

    if (q.exec() == 1)
    {
        return true;
    }

    return false;
}

Database::VisitAvailability Database::checkAvailabilityOfVisit(const int32_t patientId,
    const int32_t profession,
    const std::string& date,
    const std::string time)
{
    VisitAvailability result{};
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
        "WHERE date = :date AND time = :time AND profession = :profession"};
    q.bind(":date", date);
    q.bind(":time", time);
    q.bind(":profession", profession);

    std::uint32_t takenCounter{};
    while (q.executeStep())
    {
        auto visitPatientId{q.getColumn("patient_id").getInt()};
        if (visitPatientId == patientId)
        {
            result.status = VisitAvailability::Status::YOUR_VISIT;
        }

        auto status{static_cast<Visit::Status>(q.getColumn("status").getInt())};
        if ((status == Visit::Status::SCHEDULED) || (status == Visit::Status::COMPLETED))
        {
            takenCounter++;
            result.takenDoctorsIds.emplace_back(q.getColumn("doctor_id").getInt());
        }
    }

    if ((result.status != VisitAvailability::Status::YOUR_VISIT) && (takenCounter >= numberOfDoctorsWithProfession))
    {
        result.status = VisitAvailability::Status::TAKEN;
    }

    return result;
}

int32_t Database::getNumberOfRequestedVisitPerPatientId(const int32_t patientId)
{
    SQLite::Statement q{*mpDatabase, "SELECT COUNT(*) FROM visits WHERE patient_id = :patientId AND status = 0"};
    q.bind(":patientId", patientId);

    if (q.executeStep())
    {
        return q.getColumn(0);
    }

    throw std::runtime_error{"couldn't calculate the number of requested visits"};
}

std::vector<Database::User> Database::getFreeDoctors(const User::Profession profession,
    const std::vector<int32_t> takenDoctorsIds)
{
    std::vector<User> result;

    std::string takenDoctorsStr;
    for (auto& id : takenDoctorsIds)
    {
        takenDoctorsStr += std::to_string(id) + ',';
    }
    if (takenDoctorsStr.length() != 0)
    {
        takenDoctorsStr.pop_back();
    }

    SQLite::Statement q{*mpDatabase, "SELECT id FROM users "
        "WHERE role = :role AND type = :type AND id NOT IN (" + takenDoctorsStr + ")"};

    q.bind(":role", static_cast<int32_t>(User::Role::DOCTOR));
    q.bind(":type", static_cast<int32_t>(profession));

    while (q.executeStep())
    {
        result.emplace_back(*getUserById(q.getColumn("id")));
    }

    return result;
}
}