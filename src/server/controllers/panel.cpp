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

void Panel::patientPersonal(const drogon::HttpRequestPtr& pReq,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) try
{
    drogon::HttpResponsePtr pResp;

    if (!LoginSystemController::isUserShouldSeeThis(pReq, pResp, tsrpp::Database::User::Role::PATIENT))
    {
        callback(pResp);
        return;
    }

    drogon::HttpViewData data;
    pResp = drogon::HttpResponse::newHttpViewResponse("panel_patient_personal", data);
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