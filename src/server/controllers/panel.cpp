#include "panel.h"

using namespace std::chrono_literals;

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

    enum class ErrorCode
    {
        NOT_REQUESTED,
        CANCEL_VISIT_FAILURE,
        CANCEL_VISIT_SUCCESS,
        UPDATE_PRESCRIPTION_FAILURE,
        UPDATE_PRESCRIPTION_SUCCESS,
    } errorCode{};

    auto pCancelVisitId{pReq->getOptionalParameter<std::int32_t>("cancelVisit")};
    if (pCancelVisitId)
    {
        if (!cancelVisit(pCancelVisitId))
        {
            errorCode = ErrorCode::CANCEL_VISIT_FAILURE;
        }
        else
        {
            errorCode = ErrorCode::CANCEL_VISIT_SUCCESS;
        }
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    auto pVisitId{pReq->getOptionalParameter<int32_t>("id")};
    if (!pVisitId)
    {
        throw std::runtime_error{"visit id should be specified"};
    }
    auto pVisit{database.getVisitById(*pVisitId)};
    if ((pUser->role == tsrpp::Database::User::Role::PATIENT) && (pVisit->patient_id != pUser->id))
    {
        throw std::runtime_error{"you are not allowed to see that"};
    }

    if (pReq->method() == drogon::HttpMethod::Post)
    {
        if (auto pPrescription{pReq->getOptionalParameter<std::string>("prescription")}; pPrescription)
        {
            errorCode = ErrorCode::UPDATE_PRESCRIPTION_FAILURE;

            if ((pUser->role != tsrpp::Database::User::Role::DOCTOR) || (pUser->id != pVisit->doctor_id))
            {
                throw std::runtime_error{"you are not allowed to do this"};
            }

            pVisit->receipt = *pPrescription;
            if (database.updateVisitPrescription(pVisit->id, pVisit->receipt))
            {
                errorCode = ErrorCode::UPDATE_PRESCRIPTION_SUCCESS;
            }
        }
    }

    auto pPatient{database.getUserById(pVisit->patient_id)};
    if (pPatient == std::nullopt)
    {
        throw std::runtime_error{"patient doesn't exist"};
    }
    auto pDoctor{database.getUserById(pVisit->doctor_id)};

    drogon::HttpViewData data;
    appendDoctorsToSideMenu(data);
    data.insert("errorCode", static_cast<int>(errorCode));
    data.insert("role", static_cast<int>(pUser->role));
    data.insert("id", std::to_string(pVisit->id));
    data.insert("status", tsrpp::Database::Visit::status2Str(static_cast<tsrpp::Database::Visit::Status>(pVisit->status)));
    data.insert("date", pVisit->date);
    data.insert("time", pVisit->time);
    data.insert("profession", tsrpp::Database::User::profession2Str(pVisit->profession));
    data.insert("receipt", pVisit->receipt);

    data.insert("patientFirstName", pPatient->first_name);
    data.insert("patientLastName", pPatient->last_name);
    data.insert("patientPesel", pPatient->pesel);
    data.insert("patientEmail", pPatient->email);
    data.insert("patientPhone", pPatient->phone);
    data.insert("patientNote", pPatient->note);


    if (pDoctor)
    {
        data.insert("doctorId", pDoctor->id);
        data.insert("doctorFirstName", pDoctor->first_name);
        data.insert("doctorLastName", pDoctor->last_name);
        data.insert("doctorProfession", tsrpp::Database::User::profession2Str(pDoctor->type));
        data.insert("doctorPesel", pDoctor->pesel);
        data.insert("doctorPhone", pDoctor->phone);
    }

    auto pControlVisit{pReq->getOptionalParameter<int32_t>("controlVisit")};
    bool isControlVisitAdded{};
    if (pControlVisit != std::nullopt)
    {
        for (int32_t days{14}; !isControlVisitAdded; ++days)
        {
            // TODO: still the lack of the C++20 days feature
            // TODO: about the performance it would good to only modify time_t structure by adding the right amount of seconds
            // but this variable usually should be created only once
            auto now{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(days * 24))};
            std::tm buffer;
            localtime_r(&now, &buffer);
            std::stringstream ss;
            ss << std::put_time(&buffer, "%Y-%m-%d");
            auto controlVisitDate{ss.str()};
            for (auto it{hours.begin()}; it != hours.end(); ++it)
            {
                if (database.checkAvailabilityOfVisit(
                    pPatient->id,
                    static_cast<int32_t>(pVisit->profession),
                    controlVisitDate,
                    *it).status == tsrpp::Database::VisitAvailability::Status::FREE)
                {
                    database.addVisit(
                        pPatient->id,
                        controlVisitDate,
                        *it,
                        static_cast<int32_t>(pVisit->profession),
                        tsrpp::Database::Visit::Status::SCHEDULED,
                        pDoctor->id);
                    data.insert("controlVisitDate", controlVisitDate);
                    data.insert("controlVisitTime", *it);
                    isControlVisitAdded = true;
                    break;
                }
            }
        }
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

    enum class ErrorCode
    {
        NOT_REQUESTED,
        UPDATE_NOTE_FAILURE,
        UPDATE_NOTE_SUCCESS,
        CANCEL_VISIT_FAILURE,
        CANCEL_VISIT_SUCCESS
    } errorCode{};

    auto pCancelVisitId{pReq->getOptionalParameter<int32_t>("cancelVisit")};
    if (pCancelVisitId)
    {
        if (!cancelVisit(pCancelVisitId))
        {
            errorCode = ErrorCode::CANCEL_VISIT_FAILURE;
        }
        else
        {
            errorCode = ErrorCode::CANCEL_VISIT_SUCCESS;
        }
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    if (pReq->method() == drogon::HttpMethod::Post)
    {

        auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        auto pNote{pReq->getOptionalParameter<std::string>("note")};

        if ((pUser != std::nullopt) && (pNote != std::nullopt))
        {
            errorCode = ErrorCode::UPDATE_NOTE_FAILURE;

            if ((pNote->length()) && (pUser->note.length() < tsrpp::Database::User::maxNoteLength))
            {
                std::ostringstream ss;
                ss << *pNote << "\n" << pUser->note;
                pUser->note = ss.str();

                if (database.updateUser(*pUser))
                {
                    pReq->getSession()->modify<tsrpp::Database::User>("user", [&pUser](tsrpp::Database::User& sessionUser) {
                        sessionUser = *pUser;
                    });

                    errorCode = ErrorCode::UPDATE_NOTE_SUCCESS;
                }
            }
        }
    }

    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
    drogon::HttpViewData data;
    data.insert("errorCode", static_cast<int>(errorCode));
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
    data.insert("errorCode", static_cast<int>(errorCode));
    appendDoctorsToSideMenu(data);
    data.insert("role", static_cast<int>(pUser->role));
    data.insert("id", pEditableUser->id);
    data.insert("firstName", pEditableUser->first_name);
    data.insert("lastName", pEditableUser->last_name);
    data.insert("email", pEditableUser->email);
    data.insert("phone", pEditableUser->phone);
    data.insert("profession", static_cast<int>(pEditableUser->type));
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

            auto RegistrationErrorCode{database.checkAvailabilityOfVisit(
                pUser->id,
                profession,
                registrationDate,
                registrationTime
            ).status};
            if (RegistrationErrorCode == tsrpp::Database::VisitAvailability::Status::FREE)
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
    std::vector<int> availability;
    std::vector<int> ids;
    std::vector<bool> pastVisits;
    std::vector<std::string> yourVisitsStatuses;

    auto isVisitPast{[&date](auto hour) -> bool {
        std::tm tm{};
        std::stringstream ss;
        ss << date << " " << hour;
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");

        tm.tm_isdst = 1;

        std::time_t inputTime{mktime(&tm)};
        std::time_t nowTime{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        if (inputTime < nowTime)
        {
            return true;
        }

        return false;
    }};

    for (auto it{hours.begin()}; it != hours.end(); ++it)
    {
        // TODO: omg it beggs for the refactor
        auto avail{database.checkAvailabilityOfVisit(pUser->id, profession, date, *it)};
        availability.emplace_back(static_cast<int>(avail.status));
        int32_t id{};
        if (avail.status == tsrpp::Database::VisitAvailability::Status::YOUR_VISIT)
        {
            id = *avail.pYourVisitId;
            auto pStatusVisit{database.getVisitById(id)};
            if (!pStatusVisit)
            {
                throw std::runtime_error{"visit should exist but then couldn't be found"};
            }
            yourVisitsStatuses.emplace_back(tsrpp::Database::Visit::status2Str(pStatusVisit->status));
        }
        else
        {
            id = -1;
            yourVisitsStatuses.emplace_back();
        }
        ids.emplace_back(id);

        pastVisits.emplace_back(isVisitPast(*it));
    }
    data.insert("date", date);
    data.insert("pastVisits", pastVisits);
    data.insert("doctorProfession", *pDoctorProfession);
    data.insert("hours", hours);
    data.insert("availability", availability);
    data.insert("ids", ids);
    data.insert("ec", static_cast<int>(ec));
    data.insert("yourVisitsStatuses", yourVisitsStatuses);
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
    if (pDoctorId == std::nullopt)
    {
        pDoctorId = std::make_optional(pUser->id);
    }
    if ((pUser->role == tsrpp::Database::User::Role::DOCTOR) && (pDoctorId != pUser->id))
    {
        throw std::runtime_error{"you are not allowed to investigate other doctors"};
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
        if (*pDecision == "Approve")
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

        Mailer::sendMail(pPatient->email, ss.str());
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
    for (auto pVisitIt{visits.begin()}; pVisitIt != visits.end(); ++pVisitIt)
    {
        auto pPatient{database.getUserById(pVisitIt->patient_id)};

        auto takenDoctorsIds{database.checkAvailabilityOfVisit(
            pUser->id,
            static_cast<int32_t>(pVisitIt->profession),
            pVisitIt->date,
            pVisitIt->time
        ).takenDoctorsIds};
        auto freeDoctors{database.getFreeDoctors(pVisitIt->profession, takenDoctorsIds)};

        std::vector<int> visitDoctorIdsThisVisit;
        std::vector<std::string> visitDoctorFirstNamesThisVisit;
        std::vector<std::string> visitDoctorLastNamesThisVisit;

        for (auto doctorIt{freeDoctors.begin()}; doctorIt != freeDoctors.end(); ++doctorIt)
        {
            visitDoctorIdsThisVisit.emplace_back(doctorIt->id);
            visitDoctorFirstNamesThisVisit.emplace_back(doctorIt->first_name);
            visitDoctorLastNamesThisVisit.emplace_back(doctorIt->last_name);
        }

        if (visitDoctorIdsThisVisit.size())
        {
            visitIds.emplace_back(static_cast<int>(pVisitIt->id));
            visitPesels.emplace_back(pPatient->pesel);
            visitPatientFirstNames.emplace_back(pPatient->first_name);
            visitPatientLastNames.emplace_back(pPatient->last_name);
            visitDoctorProfessions.emplace_back(tsrpp::Database::User::profession2Str(pVisitIt->profession));

            visitDoctorIds.emplace_back(std::move(visitDoctorIdsThisVisit));
            visitDoctorFirstNames.emplace_back(std::move(visitDoctorFirstNamesThisVisit));
            visitDoctorLastNames.emplace_back(std::move(visitDoctorLastNamesThisVisit));

            visitDates.emplace_back(pVisitIt->date);
            visitTimes.emplace_back(pVisitIt->time);
        }
        else
        {
            // database.updateVisitStatus(pVisitIt->id, tsrpp::Database::Visit::Status::REJECTED);

            // std::ostringstream ss;
            // ss << "Welcome " << pPatient->first_name << " " << pPatient->last_name << "<br>"
            // << "Your appointment with " << tsrpp::Database::User::profession2Str(pVisitIt->profession) << " at "
            // << pVisitIt->date << " " << pVisitIt->time << "<br>"
            // "Has been automatically rejected due to the lack of doctors available in selected date and time.";

            // Mailer::sendMail(pPatient->email, ss.str());
        }
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

    if (!LoginSystem::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST))
    {
        callback(pResp);
        return;
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};

    std::string date;
    auto pData{pReq->getOptionalParameter<std::string>("date")};
    if (pData != std::nullopt)
    {
        date = *pData;
    }
    else
    {
        auto now{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        std::tm buffer;
        localtime_r(&now, &buffer);
        std::stringstream ss;
        ss << std::put_time(&buffer, "%Y-%m");
        date = ss.str();
    }

    drogon::HttpViewData data;
    appendDoctorsToSideMenu(data);
    data.insert("date", date);
    data.insert("visitsWithInternists", database.getVisitStats(date, tsrpp::Database::User::Profession::INTERNIST));
    data.insert("visitsWithGastroenterologists", database.getVisitStats(date, tsrpp::Database::User::Profession::GASTROENTEROLOGIST));
    data.insert("visitsWithPulmonologists", database.getVisitStats(date, tsrpp::Database::User::Profession::PULMONOLOGIST));
    data.insert("visitsWithOculists", database.getVisitStats(date, tsrpp::Database::User::Profession::OCULIST));
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

    RegistrationController::RegistrationErrorCode errorCode{};
    if (pReq->method() == drogon::HttpMethod::Post)
    {
        errorCode = RegistrationController::postRegister<tsrpp::Database::User::Role::DOCTOR>(pReq);
    }

    drogon::HttpViewData data;
    appendDoctorsToSideMenu(data);
    data.insert("errorCode", static_cast<int>(errorCode));
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_add_doctor", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE(e);
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

bool Panel::cancelVisit(const std::optional<int32_t> pVisitId)
{
    tsrpp::Database database{SQLite::OPEN_READWRITE};
    if (auto pVisit{database.getVisitById(*pVisitId)}; pVisit)
    {
        if ((pVisit->status != tsrpp::Database::Visit::Status::REQUESTED) &&
            (pVisit->status != tsrpp::Database::Visit::Status::SCHEDULED))
        {
            throw std::runtime_error{"you are not allowed to cancel visit with current status"};
        }

        std::tm t{};
        std::istringstream ss(pVisit->date + " " + pVisit->time);
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M");
        auto visitTimePoint{std::chrono::system_clock::from_time_t(std::mktime(&t))};
        auto now{std::chrono::system_clock::now()};
        auto duration{std::chrono::duration_cast<std::chrono::hours>(visitTimePoint - now)};
        if (duration > 24h)
        {
            database.updateVisitStatus(*pVisitId, tsrpp::Database::Visit::Status::CANCELLED);
            return true;
        }
    }

    return false;
}