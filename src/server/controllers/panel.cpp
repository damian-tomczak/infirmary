#include "panel.h"

void Panel::index(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis<true>(pReq, pResp))
    {
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
    ERROR_PAGE;
}

void Panel::patient(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
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
    ERROR_PAGE;
}

void Panel::doctor(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::DOCTOR))
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
    ERROR_PAGE;
}

void Panel::receptionist(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST))
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
    ERROR_PAGE;
}

void Panel::visitInformation(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    tsrpp::Database database{SQLite::OPEN_READWRITE};
    auto pCancelVisit{pReq->getOptionalParameter<std::uint32_t>("cancelVisit")};
    if (pCancelVisit != std::nullopt)
    {
        database.updateVisitStatus(*pCancelVisit, tsrpp::Database::Visit::Status::CANCELLED);
    }
    auto pVisit{pReq->getSession()->getOptional<tsrpp::Database::Visit>("visit")};
    drogon::HttpViewData data;
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_visit_information", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE;
}

void Panel::patientPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
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

    // TODO: complex note
    tsrpp::PostAction postAction;
    if (pReq->method() == drogon::HttpMethod::Post)
    {
        postAction = tsrpp::PostAction::REQUESTED_FAILURE;
        auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        auto pNote{pReq->getOptionalParameter<std::string>("note")};
        if (pUser != std::nullopt && pNote != std::nullopt)
        {
            auto user{*pUser};
            user.note = *pNote;
            if (database.updateUser(user))
            {
                postAction = tsrpp::PostAction::REQUESTED_SUCCESS;
                pReq->getSession()->modify<tsrpp::Database::User>("user", [&user](tsrpp::Database::User& sessionUser) {
                    sessionUser = user;
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
    auto visits{database.getVisitsByPatient(pUser->pesel)};
    std::vector<int> ids;
    std::vector<int> statuses;
    std::vector<int> doctorsType;
    std::vector<std::string> doctorsFirstName;
    std::vector<std::string> doctorsLastName;
    std::vector<std::string> dates;
    std::vector<std::string> times;
    (void)postAction;
    for (auto it{visits.begin()}; it != visits.end(); ++it)
    {
        ids.emplace_back(static_cast<int>(it->id));
        statuses.emplace_back(static_cast<int>(it->status));
        doctorsType.emplace_back(static_cast<int>(database.getUserbyId(it->doctor_id)->type));
        doctorsFirstName.emplace_back(database.getUserbyId(it->doctor_id)->first_name);
        doctorsLastName.emplace_back(database.getUserbyId(it->doctor_id)->last_name);
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
    ERROR_PAGE;
}

void Panel::patientEditPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    tsrpp::PostAction postAction{};

    if (pReq->method() == drogon::HttpMethod::Post)
    {
        postAction = tsrpp::PostAction::REQUESTED_FAILURE;
        auto pUser{pReq->getSession()->getOptional<tsrpp::Database::User>("user")};
        auto pFirstName{pReq->getOptionalParameter<std::string>("firstName")};
        auto pLastName{pReq->getOptionalParameter<std::string>("lastName")};
        auto pPesel{pReq->getOptionalParameter<std::string>("pesel")};
        auto pEmail{pReq->getOptionalParameter<std::string>("email")};
        // TODO: passwords

        if (pUser != std::nullopt &&
            pFirstName != std::nullopt && LoginSystemController::validFirstName(*pFirstName) &&
            pLastName != std::nullopt && LoginSystemController::validLastName(*pLastName) &&
            pPesel != std::nullopt && LoginSystemController::validPesel(*pPesel) &&
            pEmail != std::nullopt && LoginSystemController::validEmail(*pEmail))
        {
            tsrpp::Database database{SQLite::OPEN_READWRITE};
            auto user{*pUser};
            user.first_name = *pFirstName;
            user.last_name = *pLastName;
            user.pesel = *pPesel;
            user.email = *pEmail;
            if (database.updateUser(user))
            {
                postAction = tsrpp::PostAction::REQUESTED_SUCCESS;
                pReq->getSession()->modify<tsrpp::Database::User>("user", [&user](tsrpp::Database::User& sessionUser) {
                    sessionUser = user;
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
    (void)postAction;
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_edit_personal", data);
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE;
}

void Panel::doctorPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::DOCTOR))
    {
        callback(pResp);
        return;
    }

    pResp = drogon::HttpResponse::newHttpResponse();
    pResp->setBody("doctorPersonalInformations(0)");
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE;
}

void Panel::receptionistPendingRequests(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::RECEPTIONIST))
    {
        callback(pResp);
        return;
    }

    pResp = drogon::HttpResponse::newHttpResponse();
    pResp->setBody("receptionistPendingRequests()");
    callback(pResp);
}
catch(const std::exception& e)
{
    ERROR_PAGE;
}