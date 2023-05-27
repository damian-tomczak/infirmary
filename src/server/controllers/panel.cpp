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
    else if ((user->role == tsrpp::Database::User::Role::DOCTOR) || (user->role == tsrpp::Database::User::Role::RECEPTIONIST))
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

void Panel::receptionist(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST)) &&
        (!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)))
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

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    std::string url;
    if (pUser->role == tsrpp::Database::User::Role::DOCTOR)
    {
        url = tsrpp::createUrl("/panel/admin/doctor-information");
    }
    else
    {
        url = tsrpp::createUrl("/panel/admin/pending-requests");
    }

    pResp = drogon::HttpResponse::newRedirectionResponse(url);
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
    auto pCancelVisitId{pReq->getOptionalParameter<int32_t>("cancelVisit")};
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

    drogon::HttpViewData data;
    appendDoctorsToSideMenu(data);
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
    data.insert("patientPhone", pPatient->phone);
    data.insert("patientNote", pPatient->note);

    if (!pDoctor)
    {
        data.insert("doctorId", pDoctor->id);
        data.insert("doctorFirstName", pDoctor->first_name);
        data.insert("doctorLastName", pDoctor->last_name);
        data.insert("doctorPesel", pDoctor->pesel);
        data.insert("doctorPhone", pDoctor->phone);
        data.insert("errorCode", errorCode);
    }

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
    data.insert("phone", pUser->phone);
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

void Panel::editPersonal(const drogon::HttpRequestPtr& pReq,
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

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    std::unique_ptr<tsrpp::Database::User> pEditableUser;
    if ((pUser->role == tsrpp::Database::User::Role::PATIENT) || (pUser->role == tsrpp::Database::User::Role::DOCTOR))
    {
        pEditableUser = std::make_unique<tsrpp::Database::User>(*pUser);;
    }
    else
    {
        auto pUserId{pReq->getOptionalParameter<int32_t>("userId")};
        if (pUserId != std::nullopt)
        {
            auto pUserFromDb{database.getUserById(*pUserId)};
            if (pUserId == std::nullopt)
            {
                throw std::runtime_error{"user couldn't be found"};
            }
            pEditableUser = std::make_unique<tsrpp::Database::User>(*pUserFromDb);
        }
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
        auto pFirstName{pReq->getOptionalParameter<std::string>("firstName")};
        auto pLastName{pReq->getOptionalParameter<std::string>("lastName")};
        auto pEmail{pReq->getOptionalParameter<std::string>("email")};
        auto pPhone{pReq->getOptionalParameter<std::string>("phone")};
        auto pProfession{pReq->getOptionalParameter<int32_t>("profession")};
        auto pCurrentPassword{pReq->getOptionalParameter<std::string>("currentPassword")};
        auto pNewPassword{pReq->getOptionalParameter<std::string>("newPassword")};
        auto pRepeatedNewPassword{pReq->getOptionalParameter<std::string>("repeatedNewPassword")};

        if ((pFirstName != std::nullopt) && LoginSystem::validFirstName(*pFirstName) &&
            (pLastName != std::nullopt) && LoginSystem::validLastName(*pLastName) &&
            (pEmail != std::nullopt) && LoginSystem::validEmail(*pEmail) &&
            (pPhone != std::nullopt) && LoginSystem::validPhone(*pPhone) &&
            (((pEditableUser->role == tsrpp::Database::User::Role::DOCTOR) && (pProfession != std::nullopt)) || (pEditableUser->role == tsrpp::Database::User::Role::PATIENT)) &&
            (pCurrentPassword != std::nullopt) && tsrpp::verifyPassword(*pCurrentPassword, pUser->password) &&
            (pNewPassword != std::nullopt) && (pRepeatedNewPassword != std::nullopt) &&
            (((pNewPassword->length() > 0) && LoginSystem::validPassword(*pNewPassword) && (*pNewPassword == *pRepeatedNewPassword)) ||
            (pNewPassword->length() == 0)))
        {
            tsrpp::Database database{SQLite::OPEN_READWRITE};
            pEditableUser->first_name = *pFirstName;
            pEditableUser->last_name = *pLastName;
            pEditableUser->email = *pEmail;
            pEditableUser->phone = *pPhone;
            if (pEditableUser->role == tsrpp::Database::User::Role::DOCTOR)
            {
                pEditableUser->type = static_cast<tsrpp::Database::User::Profession>(*pProfession);
            }
            if (pNewPassword->length() > 0)
            {
                pEditableUser->password = tsrpp::hashPassword(*pNewPassword);
            }
            if (database.updateUser(*pEditableUser))
            {
                if (pEditableUser->id == pUser->id)
                {
                    pReq->getSession()->modify<tsrpp::Database::User>("user", [&pEditableUser](tsrpp::Database::User& sessionUser) {
                        sessionUser = *pEditableUser;
                    });
                }
                errorCode = ErrorCode::SUCCESS;
            }
        }
    }

    drogon::HttpViewData data;
    data.insert("role", static_cast<int>(pUser->role));
    data.insert("id", pEditableUser->id);
    data.insert("firstName", pEditableUser->first_name);
    data.insert("lastName", pEditableUser->last_name);
    data.insert("email", pEditableUser->email);
    data.insert("phone", pEditableUser->phone);
    data.insert("profession", static_cast<int>(pEditableUser->type));
    data.insert("errorCode", static_cast<int>(errorCode));
    appendDoctorsToSideMenu(data);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_edit_personal", data);
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

    enum class ErrorCode
    {
        DEFAULT,
        EXCEEDING_MAX_REQUESTED_VISITS
    } ec{};

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pDoctorProfession{pReq->getOptionalParameter<std::string>("doctorProfession")};
    if (pDoctorProfession == std::nullopt)
    {
        throw std::runtime_error{"doctorProfession should be specified"};
    }
    int32_t profession{tsrpp::Database::User::professionStr2Int(*pDoctorProfession)};

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

        std::tm* nowTm{std::localtime(&nowTimeT)};

        std::tm yearMonthDayOnlyTm{};
        yearMonthDayOnlyTm.tm_year = nowTm->tm_year;
        yearMonthDayOnlyTm.tm_mon = nowTm->tm_mon;
        yearMonthDayOnlyTm.tm_mday = nowTm->tm_mday;

        nowTimeT = std::mktime(&yearMonthDayOnlyTm);

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
        if (database.getNumberOfRequestedVisitPerPatientId(pUser->id) < maxRequestedVisitPerUser)
        {
            // TODO: Pity for the lack of std::views feature
            std::string registrationDate, registrationTime;
            std::istringstream ss(*pRegister);
            std::getline(ss, registrationDate, ' ');
            std::getline(ss, registrationTime, ' ');

            auto registrationStatus{database.checkAvailabilityOfVisit(
                pUser->id,
                profession,
                registrationDate,
                registrationTime
            ).status};
            if (registrationStatus == tsrpp::Database::VisitAvailability::Status::FREE)
            {
                database.addVisit(pUser->id, registrationDate, registrationTime, profession);
            }
        }
        else
        {
            ec = ErrorCode::EXCEEDING_MAX_REQUESTED_VISITS;
        }
    }

    drogon::HttpViewData data;
    std::array<std::string, 12> hours {
        "09:00",
        "09:40",
        "10:20",
        "11:00",
        "11:40",
        "12:20",
        "13:00",
        "13:40",
        "14:20",
        "15:00",
        "15:40",
        "16:20",
    };
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
    data.insert("ec", static_cast<int>(ec));
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

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)) &&
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
    appendDoctorsToSideMenu(data);
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

void Panel::doctorInformation(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST)) &&
        (!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)))
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

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    tsrpp::Database database{SQLite::OPEN_READWRITE};
    drogon::HttpViewData data;
    data.insert("role", static_cast<int>(pUser->role));
    auto pDoctorId{pReq->getOptionalParameter<int32_t>("doctorId")};
    if ((pUser->role == tsrpp::Database::User::Role::DOCTOR) &&
        (pDoctorId != std::nullopt))
    {
        throw std::runtime_error{"you are not allowed to investigate other doctors"};
    }

    if (pDoctorId == std::nullopt)
    {
        pDoctorId = std::make_optional(pUser->id);
    }

    std::unique_ptr<tsrpp::Database::User> pDoctorInfo;
    if ((pUser->role == tsrpp::Database::User::Role::DOCTOR))
    {
        pDoctorInfo = std::make_unique<tsrpp::Database::User>(*pUser);
    }
    else
    {
        auto pDoctorFromDb{database.getUserById(*pDoctorId)};
        if (pDoctorFromDb == std::nullopt)
        {
            throw std::runtime_error{"doctor doesn't exist"};
        }
        pDoctorInfo = std::make_unique<tsrpp::Database::User>(*pDoctorFromDb);
    }

    std::string date;
    data.insert("doctorId", *pDoctorId);
    data.insert("doctorFirstName", pDoctorInfo->first_name);
    data.insert("doctorLastName", pDoctorInfo->last_name);
    data.insert("doctorPesel", pDoctorInfo->pesel);
    data.insert("doctorEmail", pDoctorInfo->email);
    data.insert("doctorPhone", pDoctorInfo->phone);
    data.insert("doctorProfession", tsrpp::Database::User::profession2Str(static_cast<tsrpp::Database::User::Profession>(pDoctorInfo->type)));

    appendDoctorsToSideMenu(data);
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
    std::vector<tsrpp::Database::Visit> visits{database.getVisitsByDoctorIdAndDate(*pDoctorId, date)};
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
    data.insert("date", date);
    data.insert("ids", ids);
    data.insert("statuses", statuses);
    data.insert("patientFirstNames", patientFirstNames);
    data.insert("patientLastNames", patientLastNames);
    data.insert("patientPesels", patientPesels);
    data.insert("dates", dates);
    data.insert("times", times);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_doctor_information", data);
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

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)) &&
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
    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};

    if (pReq->method() == drogon::HttpMethod::Post)
    {
        auto pDecision{pReq->getOptionalParameter<std::string>("decision")};
        auto pVisitId{pReq->getOptionalParameter<int32_t>("visitId")};
        auto pDoctorId{pReq->getOptionalParameter<int32_t>("doctorId")};
        tsrpp::Database::Visit::Status status;
        if (*pDecision == "approve")
        {
            status = tsrpp::Database::Visit::Status::SCHEDULED;
        }
        else
        {
            status = tsrpp::Database::Visit::Status::REJECTED;
        }
        database.updateVisitStatus(*pVisitId, status);
        database.updateVisitDoctorId(*pVisitId, *pDoctorId);

        auto pVisit{database.getVisitById(*pVisitId)};
        auto pReason{pReq->getOptionalParameter<std::string>("reason")};
        std::transform(pDecision->begin(), pDecision->end(), pDecision->begin(),
                [](unsigned char c){ return std::tolower(c); });

        auto pPatient{database.getUserById(pVisit->patient_id)};
        std::ostringstream ss;
        ss << "Welcome " << pPatient->first_name << " " << pPatient->last_name << "<br>"
        << "Your appointment with " << tsrpp::Database::User::profession2Str(pVisit->profession) << " at "
        << pVisit->date << " " << pVisit->time << " ";

        if (*pDecision == "approve")
        {
            auto pDoctor{database.getUserById(pVisit->doctor_id)};
            ss << "has been approved<br>Doctor Info: " << pDoctor->first_name << " " << pDoctor->last_name << "<br>"
            << "More Info: " << *pReason;
        }
        else
        {
            ss << "has been declined<br>"
            << "Reason: " << *pReason;
        }
        std::string message = ss.str();
        Mailer::sendMail(pPatient->email, message);
    }

    drogon::HttpViewData data;
    auto visits{database.getVisitsByStatus(tsrpp::Database::Visit::Status::REQUESTED)};
    std::vector<int> visitIds;
    std::vector<std::string> visitPesels;
    std::vector<std::string> visitPatientFirstNames;
    std::vector<std::string> visitPatientLastNames;
    std::vector<std::string> visitDoctorProfessions;
    std::vector<std::string> visitDates;
    std::vector<std::string> visitTimes;
    std::vector<std::vector<int>> visitDoctorIds;
    std::vector<std::vector<std::string>> visitDoctorFirstNames;
    std::vector<std::vector<std::string>> visitDoctorLastNames;
    for (auto visitIt{visits.begin()}; visitIt != visits.end(); ++visitIt)
    {
        auto pPatient{database.getUserById(visitIt->patient_id)};

        visitIds.emplace_back(static_cast<int>(visitIt->id));
        visitPesels.emplace_back(pPatient->pesel);
        visitPatientFirstNames.emplace_back(pPatient->first_name);
        visitPatientLastNames.emplace_back(pPatient->last_name);
        visitDoctorProfessions.emplace_back(tsrpp::Database::User::profession2Str(visitIt->profession));

        auto takenDoctorsIds{database.checkAvailabilityOfVisit(
            pUser->id,
            static_cast<int32_t>(visitIt->profession),
            visitIt->date,
            visitIt->time
        ).takenDoctorsIds};
        auto freeDoctors{database.getFreeDoctors(visitIt->profession, takenDoctorsIds)};

        std::vector<int> visitDoctorIdsThisVisit;
        std::vector<std::string> visitDoctorFirstNamesThisVisit;
        std::vector<std::string> visitDoctorLastNamesThisVisit;

        for (auto doctorIt{freeDoctors.begin()}; doctorIt != freeDoctors.end(); ++doctorIt)
        {
            visitDoctorIdsThisVisit.emplace_back(doctorIt->id);
            visitDoctorFirstNamesThisVisit.emplace_back(doctorIt->first_name);
            visitDoctorLastNamesThisVisit.emplace_back(doctorIt->last_name);
        }

        visitDoctorIds.emplace_back(std::move(visitDoctorIdsThisVisit));
        visitDoctorFirstNames.emplace_back(std::move(visitDoctorFirstNamesThisVisit));
        visitDoctorLastNames.emplace_back(std::move(visitDoctorLastNamesThisVisit));

        visitDates.emplace_back(visitIt->date);
        visitTimes.emplace_back(visitIt->time);
    }
    data.insert("visitIds", visitIds);
    data.insert("visitPatientFirstNames", visitPatientFirstNames);
    data.insert("visitPatientLastNames", visitPatientLastNames);
    data.insert("visitPesels", visitPesels);
    data.insert("visitDates", visitDates);
    data.insert("visitTimes", visitTimes);
    data.insert("visitDoctorProfessions", visitDoctorProfessions);
    data.insert("visitDoctorIds", visitDoctorIds);
    data.insert("visitDoctorFirstNames", visitDoctorFirstNames);
    data.insert("visitsDoctorLastNames", visitDoctorLastNames);
    appendDoctorsToSideMenu(data);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_pending_requests", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::statistics(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)) &&
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
    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    drogon::HttpViewData data;
    appendDoctorsToSideMenu(data);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_statistics", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
}

void Panel::addDoctor(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if ((!LoginSystem::isUserShouldSeeThis<false>(pReq, pResp, tsrpp::Database::User::Role::DOCTOR)) &&
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
    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    drogon::HttpViewData data;
    appendDoctorsToSideMenu(data);
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_add_doctor", data);
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

void Panel::appendDoctorsToSideMenu(drogon::HttpViewData& data)
{
    tsrpp::Database database;
    auto users{database.getUsersByRole(tsrpp::Database::User::Role::DOCTOR)};

    std::vector<int> doctorIds;
    std::vector<std::string> doctorFirstNames;
    std::vector<std::string> doctorLastNames;
    std::vector<std::string> doctorProfessions;
    for (auto it{users.begin()}; it != users.end(); ++it)
    {
        doctorIds.emplace_back(it->id);
        doctorFirstNames.emplace_back(it->first_name);
        doctorLastNames.emplace_back(it->last_name);
        doctorProfessions.emplace_back(tsrpp::Database::User::profession2Str(it->type));
    }
    data.insert("doctorIds", doctorIds);
    data.insert("doctorFirstNames", doctorFirstNames);
    data.insert("doctorLastNames", doctorLastNames);
    data.insert("doctorProfessions", doctorProfessions);
}