#include "panel.h"

void Panel::index(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    if (pUser == std::nullopt)
    {
        pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/login"));
        callback(pResp);
        return;
    }

    auto user{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    if (user->role == tsrpp::Database::User::Role::PATIENT)
    {
        pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel/patient"));
    }
    else if (user->role == tsrpp::Database::User::Role::DOCTOR)
    {
        pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel/doctor"));
    }
    else if (user->role == tsrpp::Database::User::Role::RECEPTIONIST)
    {
        pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl("/panel/receptionist"));
    }
    else
    {
        throw std::runtime_error{"Logged user doesn't belong to any category"};
    }

    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::patient(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    pResp = drogon::HttpResponse::newRedirectionResponse(tsrpp::createUrl(
        "/panel/patient/personal"));
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::doctor(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::DOCTOR))
    {
        callback(pResp);
        return;
    }

    pResp = drogon::HttpResponse::newRedirectionResponse(
        tsrpp::createUrl("/panel/doctor/personal"));
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::receptionist(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST))
    {
        callback(pResp);
        return;
    }

    pResp = drogon::HttpResponse::newRedirectionResponse(
        tsrpp::createUrl("/panel/receptionist/pending_requests"));
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::visitInformation(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::PATIENT)) &&
        (!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)) &&
        (!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST)))
    {
        if (pReq->getSession()->getOptional<tsrpp::Database::User>("user") == std::nullopt)
        {
            callback(pResp);
            return;
        }
        else
        {
            throw std::runtime_error{"you are not allowed to see this"};
        }
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    enum class ErrorCode
    {
        NOT_REQUESTED,
        FAILURE,
        SUCCESS
    } errorCode{};
    if (pReq->method() == drogon::HttpMethod::Post)
    {
        errorCode = ErrorCode::FAILURE;
        auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        auto pNote{pReq->getOptionalParameter<std::string>("note")};
        if ((pUser != std::nullopt) && (pNote != std::nullopt) &&
            appendNote(pUser->role, pUser->note, *pNote))
        {
            pUser->note = *pNote;
            if (database.updateUser(*pUser))
            {
                errorCode = ErrorCode::SUCCESS;
                pReq->getSession()->modify<tsrpp::Database::User>("user", [&pUser](tsrpp::Database::User& sessionUser) {
                    sessionUser = *pUser;
                });
            }
        }
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    auto pCancelVisitId{pReq->getOptionalParameter<std::int32_t>("cancelVisit")};
    if (pCancelVisitId != std::nullopt)
    {
        database.updateVisitStatus(*pCancelVisitId, tsrpp::Database::Visit::Status::CANCELLED);
    }

    auto pVisitId{pReq->getOptionalParameter<std::string>("id")};
    if (pVisitId == std::nullopt)
    {
        throw std::runtime_error{"visit id should be specified"};
    }
    auto visitId{std::stoi(*pVisitId)};
    auto pVisit{database.getVisitById(visitId)};
    if ((pUser->role == tsrpp::Database::User::Role::PATIENT) && (pVisit->patient_id != pUser->id))
    {
        throw std::runtime_error{"you are not allowed to see that"};
    }
    auto pPatient{database.getUserById(pVisit->patient_id)};
    if (pPatient == std::nullopt)
    {
        throw std::runtime_error{"patient doesn't exist"};
    }
    auto pDoctor{database.getUserById(pVisit->doctor_id)};
    if (pDoctor == std::nullopt)
    {
        throw std::runtime_error{"doctor doesn't exist"};
    }

    drogon::HttpViewData data;
    data.insert("role", static_cast<int>(pUser->role));
    data.insert("id", std::to_string(pVisit->id));
    data.insert("status", tsrpp::Database::Visit::status2Str(static_cast<tsrpp::Database::Visit::Status>(pVisit->status)));
    data.insert("date", pVisit->date);
    data.insert("time", pVisit->time);
    data.insert("receipt", pVisit->receipt);

    data.insert("patientFirstName", pPatient->first_name);
    data.insert("patientLastName", pPatient->last_name);
    data.insert("patientPesel", pPatient->pesel);
    data.insert("patientEmail", pPatient->email);
    data.insert("patientNote", pPatient->note);

    data.insert("doctorFirstName", pDoctor->first_name);
    data.insert("doctorLastName", pDoctor->last_name);
    data.insert("doctorPesel", pDoctor->pesel);
    data.insert("doctorEmail", pDoctor->email);
    data.insert("errorCode", errorCode);

    pResp = drogon::HttpResponse::newHttpViewResponse("panel_visit_information", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::patientPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto cancelVisit{pReq->getOptionalParameter<std::uint32_t>("cancelVisit")};
    if (cancelVisit)
    {
        database.updateVisitStatus(*cancelVisit, tsrpp::Database::Visit::Status::CANCELLED);
    }

    enum class ErrorCode
    {
        NOT_REQUESTED,
        FAILURE,
        SUCCESS
    } errorCode{};
    if (pReq->method() == drogon::HttpMethod::Post)
    {
        errorCode = ErrorCode::FAILURE;
        auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        auto pNote{pReq->getOptionalParameter<std::string>("note")};
        if ((pUser != std::nullopt) && (pNote != std::nullopt) &&
            appendNote(pUser->role, pUser->note, *pNote))
        {
            pUser->note = *pNote;
            if (database.updateUser(*pUser))
            {
                errorCode = ErrorCode::SUCCESS;
                pReq->getSession()->modify<tsrpp::Database::User>("user", [&pUser](tsrpp::Database::User& sessionUser) {
                    sessionUser = *pUser;
                });
            }
        }
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    drogon::HttpViewData data;
    data.insert("firstName", pUser->first_name);
    data.insert("lastName", pUser->last_name);
    data.insert("pesel", pUser->pesel);
    data.insert("email", pUser->email);
    data.insert("note", pUser->note);
    auto visits{database.getVisitsByPatientPesel(pUser->pesel)};
    std::vector<int> ids;
    std::vector<int> statuses;
    std::vector<int> doctorsType;
    std::vector<std::string> doctorsFirstName;
    std::vector<std::string> doctorsLastName;
    std::vector<std::string> dates;
    std::vector<std::string> times;
    data.insert("errorCode", errorCode);
    for (auto it{visits.begin()}; it != visits.end(); ++it)
    {
        ids.emplace_back(static_cast<int>(it->id));
        statuses.emplace_back(static_cast<int>(it->status));
        doctorsType.emplace_back(static_cast<int>(database.getUserById(it->doctor_id)->type));
        doctorsFirstName.emplace_back(database.getUserById(it->doctor_id)->first_name);
        doctorsLastName.emplace_back(database.getUserById(it->doctor_id)->last_name);
        dates.emplace_back(it->date);
        times.emplace_back(it->time);
    }
    data.insert("ids", ids);
    data.insert("statuses", statuses);
    data.insert("doctorsType", doctorsType);
    data.insert("doctorsFirstName", doctorsFirstName);
    data.insert("doctorsLastName", doctorsLastName);
    data.insert("dates", dates);
    data.insert("times", times);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_personal", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::patientEditPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    enum class ErrorCode
    {
        NOT_REQUESTED,
        FAILURE,
        SUCCESS
    } errorCode{};
    if (pReq->method() == drogon::HttpMethod::Post)
    {
        errorCode = ErrorCode::FAILURE;
        tsrpp::Database database{SQLite::OPEN_READWRITE};
        auto pFirstName{pReq->getOptionalParameter<std::string>("firstName")};
        auto pLastName{pReq->getOptionalParameter<std::string>("lastName")};
        auto pPesel{pReq->getOptionalParameter<std::string>("pesel")};
        auto pEmail{pReq->getOptionalParameter<std::string>("email")};
        auto pCurrentPassword{pReq->getOptionalParameter<std::string>("currentPassword")};
        auto pNewPassword{pReq->getOptionalParameter<std::string>("newPassword")};
        auto pRepeatedNewPassword{pReq->getOptionalParameter<std::string>("repeatedNewPassword")};

        if ((pUser != std::nullopt) &&
            (pFirstName != std::nullopt) && LoginSystem::validFirstName(*pFirstName) &&
            (pLastName != std::nullopt) && LoginSystem::validLastName(*pLastName) &&
            (pPesel != std::nullopt) && LoginSystem::validPesel(*pPesel) &&
            (pEmail != std::nullopt) && LoginSystem::validEmail(*pEmail) &&
            (pCurrentPassword != std::nullopt) && tsrpp::verifyPassword(*pCurrentPassword, database.getUserById(pUser->id)->password) &&
            (pNewPassword != std::nullopt) && LoginSystem::validPassword(*pNewPassword) &&
            (pRepeatedNewPassword != std::nullopt) && (*pNewPassword == *pRepeatedNewPassword))
        {
            tsrpp::Database database{SQLite::OPEN_READWRITE};
            pUser->first_name = *pFirstName;
            pUser->last_name = *pLastName;
            pUser->pesel = *pPesel;
            pUser->email = *pEmail;
            pUser->password = tsrpp::hashPassword(*pNewPassword);
            if (database.updateUser(*pUser))
            {
                errorCode = ErrorCode::SUCCESS;
                pReq->getSession()->modify<tsrpp::Database::User>("user", [&pUser](tsrpp::Database::User& sessionUser) {
                    sessionUser = *pUser;
                });
            }
        }
    }

    drogon::HttpViewData data;
    data.insert("firstName", pUser->first_name);
    data.insert("lastName", pUser->last_name);
    data.insert("pesel", pUser->pesel);
    data.insert("email", pUser->email);
    data.insert("errorCode", static_cast<int>(errorCode));
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_edit_personal", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::patientCalendar(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pDoctorProfession{pReq->getOptionalParameter<std::string>("doctorProfession")};
    if (pDoctorProfession == std::nullopt)
    {
        throw std::runtime_error{"doctorProfession should be specified"};
    }
    std::int32_t profession{tsrpp::Database::User::professionStr2Int(*pDoctorProfession)};

    std::string date;
    auto pDateParameter{pReq->getOptionalParameter<std::string>("date")};
    bool isPastSelected{};
    if (pDateParameter != std::nullopt)
    {
        date = *pDateParameter;

        std::tm tm{};
        std::stringstream ss(date);
        ss >> std::get_time(&tm, "%Y-%m-%d");

        std::time_t inputTimeT = mktime(&tm);
        std::time_t nowTimeT = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        if (inputTimeT < nowTimeT)
        {
            isPastSelected = true;
        }
    }
    else
    {
        auto now{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        std::tm buffer;
        localtime_r(&now, &buffer);
        std::stringstream ss;
        ss << std::put_time(&buffer, "%Y-%m-%d");
        date = ss.str();
    }

    auto pRegister{pReq->getOptionalParameter<std::string>("register")};
    if (pRegister != std::nullopt)
    {
        // TODO: Pity for the lack of std::views feature
        std::string registrationDate, registrationTime;
        std::istringstream ss(*pRegister);
        std::getline(ss, registrationDate, ' ');
        std::getline(ss, registrationTime, ' ');

        auto takenDoctorsIds{database.checkAvailabilityOfVisit(
            pUser->id,
            profession,
            registrationDate,
            registrationTime
        ).takenDoctorsIds};
        auto pFreeDoctorId{database.getFreeDoctor(profession, takenDoctorsIds)};
        if (pFreeDoctorId != std::nullopt)
        {
            database.addVisit(pUser->id, *pFreeDoctorId, registrationDate, registrationTime);
        }
    }

    drogon::HttpViewData data;
    std::vector<std::string> hours;
    // TODO: RETARD ALARM
    hours.emplace_back("09:00");
    hours.emplace_back("09:40");
    hours.emplace_back("10:20");
    hours.emplace_back("11:00");
    hours.emplace_back("11:40");
    hours.emplace_back("12:20");
    hours.emplace_back("13:00");
    hours.emplace_back("13:40");
    hours.emplace_back("14:20");
    hours.emplace_back("15:00");
    hours.emplace_back("15:40");
    hours.emplace_back("16:20");
    std::vector<int> availability;
    for (auto it{hours.begin()}; it != hours.end(); ++it)
    {
        availability.emplace_back(static_cast<int>(
            database.checkAvailabilityOfVisit(pUser->id, profession, date, *it).status));
    }
    data.insert("date", date);
    data.insert("doctorProfession", *pDoctorProfession);
    data.insert("isPastSelected", isPastSelected);
    data.insert("hours", hours);
    data.insert("availability", availability);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_calendar", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::patientInformation(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if ((!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)) &&
        (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST)))
    {
        if (pReq->getSession()->getOptional<tsrpp::Database::User>("user") == std::nullopt)
        {
            callback(pResp);
            return;
        }
        else
        {
            throw std::runtime_error{"you are not allowed to see this"};
        }
    }

    enum class ErrorCode
    {
        PESEL_NOT_SPECIFIED,
        PESEL_CORRECT,
        PESEL_EMPTY,
        PESEL_INCORRECT,
        PESEL_USER_DOESNT_EXIST,
        PESEL_ISNT_PATIENT
    } errorCode{};

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    std::vector<tsrpp::Database::Visit> visits;
    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pPesel{pReq->getOptionalParameter<std::string>("pesel")};
    if (pPesel != std::nullopt)
    {
        if (pPesel->length() == 0)
        {
            errorCode = ErrorCode::PESEL_EMPTY;
        }
        else if (!LoginSystem::validPesel(*pPesel))
        {
            errorCode = ErrorCode::PESEL_INCORRECT;
        }
        else
        {
            visits = database.getVisitsByPatientPesel(*pPesel);
            auto pTmpUser{database.getUserByPesel(*pPesel)};
            if (pTmpUser == std::nullopt)
            {
                errorCode = ErrorCode::PESEL_USER_DOESNT_EXIST;
            }
            if (pTmpUser->role != tsrpp::Database::User::Role::PATIENT)
            {
                errorCode = ErrorCode::PESEL_ISNT_PATIENT;
            }
            errorCode = ErrorCode::PESEL_CORRECT;
        }
    }
    drogon::HttpViewData data;
    data.insert("pesel", *pPesel);
    data.insert("role", static_cast<int>(pUser->role));
    data.insert("errorCode", static_cast<int>(errorCode));
    std::vector<int> ids;
    std::vector<std::string> statuses;
    std::vector<std::string> patientFirstNames;
    std::vector<std::string> patientLastNames;
    std::vector<std::string> patientPesels;
    std::vector<std::string> dates;
    std::vector<std::string> times;
    for (auto it{visits.begin()}; it != visits.end(); ++it)
    {
        ids.emplace_back(static_cast<int>(it->id));
        statuses.emplace_back(tsrpp::Database::Visit::status2Str(static_cast<tsrpp::Database::Visit::Status>(it->status)));
        auto pVisitUser{database.getUserById(it->patient_id)};
        patientFirstNames.emplace_back(pVisitUser->first_name);
        patientLastNames.emplace_back(pVisitUser->last_name);
        dates.emplace_back(it->date);
        times.emplace_back(it->time);
    }
    data.insert("ids", ids);
    data.insert("statuses", statuses);
    data.insert("patientFirstNames", patientFirstNames);
    data.insert("patientLastNames", patientLastNames);
    data.insert("patientPesels", patientPesels);
    data.insert("dates", dates);
    data.insert("times", times);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_information", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::doctorPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::DOCTOR))
    {
        callback(pResp);
        return;
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    std::string date;
    auto pDateParameter{pReq->getOptionalParameter<std::string>("date")};
    if (pDateParameter != std::nullopt)
    {
        date = *pDateParameter;
    }
    else
    {
        auto now{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        std::tm buffer;
        localtime_r(&now, &buffer);
        std::stringstream ss;
        ss << std::put_time(&buffer, "%Y-%m-%d");
        date = ss.str();
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    drogon::HttpViewData data;
    data.insert("date", date);
    data.insert("firstName", pUser->first_name);
    data.insert("lastName", pUser->last_name);
    data.insert("pesel", pUser->pesel);
    data.insert("email", pUser->email);
    data.insert("profession", tsrpp::Database::User::profession2Str(static_cast<tsrpp::Database::User::Profession>(pUser->type)));
    auto visits{database.getVisitsByDoctorIdAndDate(pUser->id, date)};
    std::vector<int> ids;
    std::vector<std::string> statuses;
    std::vector<std::string> patientFirstNames;
    std::vector<std::string> patientLastNames;
    std::vector<std::string> patientPesels;
    std::vector<std::string> dates;
    std::vector<std::string> times;
    for (auto it{visits.begin()}; it != visits.end(); ++it)
    {
        ids.emplace_back(static_cast<int>(it->id));
        statuses.emplace_back(tsrpp::Database::Visit::status2Str(static_cast<tsrpp::Database::Visit::Status>(it->status)));
        patientFirstNames.emplace_back(database.getUserById(it->patient_id)->first_name);
        patientLastNames.emplace_back(database.getUserById(it->patient_id)->last_name);
        patientPesels.emplace_back(database.getUserById(it->patient_id)->pesel);
        times.emplace_back(it->time);
    }
    data.insert("ids", ids);
    data.insert("statuses", statuses);
    data.insert("patientFirstNames", patientFirstNames);
    data.insert("patientLastNames", patientLastNames);
    data.insert("patientPesels", patientPesels);
    data.insert("dates", dates);
    data.insert("times", times);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_doctor_personal", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::doctorEditPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::DOCTOR))
    {
        callback(pResp);
        return;
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    enum class ErrorCode
    {
        NOT_REQUESTED,
        FAILURE,
        SUCCESS
    } errorCode{};
    if (pReq->method() == drogon::HttpMethod::Post)
    {
        tsrpp::Database database{SQLite::OPEN_READWRITE};
        errorCode = ErrorCode::FAILURE;
        auto pFirstName{pReq->getOptionalParameter<std::string>("firstName")};
        auto pLastName{pReq->getOptionalParameter<std::string>("lastName")};
        auto pPesel{pReq->getOptionalParameter<std::string>("pesel")};
        auto pEmail{pReq->getOptionalParameter<std::string>("email")};
        auto pProfession{pReq->getOptionalParameter<std::int32_t>("profession")};
        auto pCurrentPassword{pReq->getOptionalParameter<std::string>("currentPassword")};
        auto pNewPassword{pReq->getOptionalParameter<std::string>("newPassword")};
        auto pRepeatedNewPassword{pReq->getOptionalParameter<std::string>("repeatedNewPassword")};

        if ((pUser != std::nullopt) &&
            (pFirstName != std::nullopt) && LoginSystem::validFirstName(*pFirstName) &&
            (pLastName != std::nullopt) && LoginSystem::validLastName(*pLastName) &&
            (pPesel != std::nullopt) && LoginSystem::validPesel(*pPesel) &&
            (pEmail != std::nullopt) && LoginSystem::validEmail(*pEmail) &&
            (pProfession != std::nullopt) && tsrpp::Database::User::isValidProfession(
                static_cast<tsrpp::Database::User::Profession>(*pProfession)) &&
            (pCurrentPassword != std::nullopt) && tsrpp::verifyPassword(*pCurrentPassword, database.getUserById(pUser->id)->password) &&
            (pNewPassword != std::nullopt) && LoginSystem::validPassword(*pNewPassword) &&
            (pRepeatedNewPassword != std::nullopt) && (*pNewPassword == *pRepeatedNewPassword))
        {
            pUser->first_name = *pFirstName;
            pUser->last_name = *pLastName;
            pUser->pesel = *pPesel;
            pUser->email = *pEmail;
            pUser->type = static_cast<tsrpp::Database::User::Profession>(*pProfession);
            pUser->password = tsrpp::hashPassword(*pNewPassword);
            if (database.updateUser(*pUser))
            {
                errorCode = ErrorCode::SUCCESS;
                pReq->getSession()->modify<tsrpp::Database::User>("user", [&pUser](tsrpp::Database::User& sessionUser) {
                    sessionUser = *pUser;
                });
            }
        }
    }

    drogon::HttpViewData data;
    data.insert("firstName", pUser->first_name);
    data.insert("lastName", pUser->last_name);
    data.insert("pesel", pUser->pesel);
    data.insert("email", pUser->email);
    data.insert("profession", static_cast<int>(pUser->type));
    data.insert("errorCode", static_cast<int>(errorCode));
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_doctor_edit_personal", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::receptionistPendingRequests(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST))
    {
        callback(pResp);
        return;
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    drogon::HttpViewData data;
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_receptionist_pending_requests", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

bool Panel::appendNote(const tsrpp::Database::User::Role role, const std::string& currentNote, const std::string& newNote)
{
    if (role == tsrpp::Database::User::Role::PATIENT)
    {
        if (newNote.length() > currentNote.length())
        {
            std::string_view origin(newNote.data(), currentNote.length());

            if (origin == currentNote)
            {
                return true;
            }
        }

        return false;
    }
    else
    {
        return true;
    }
}