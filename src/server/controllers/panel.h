#include "login_system.h"

#include <drogon/HttpController.h>

#include <iomanip>
#include <chrono>

class Panel final : public drogon::HttpController<Panel>
{
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(::Panel::index, "");

    METHOD_ADD(::Panel::patient, "patient");
    METHOD_ADD(::Panel::doctor, "doctor");
    METHOD_ADD(::Panel::receptionist, "receptionist");

    METHOD_ADD(::Panel::visitInformation, "visit-information");

    METHOD_ADD(::Panel::patientPersonal, "patient/personal");
    METHOD_ADD(::Panel::patientEditPersonal, "patient/edit-personal");
    METHOD_ADD(::Panel::patientCalendar, "patient/calendar");

    METHOD_ADD(::Panel::doctorPersonal, "doctor/personal");

    METHOD_ADD(::Panel::receptionistPendingRequests, "receptionist/pending_requests");
    METHOD_LIST_END

    // REDIRECTS
    void index(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patient(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void doctor(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void receptionist(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // MEAT
    void visitInformation(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // Patient
    void patientPersonal(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patientEditPersonal(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patientCalendar(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // Doctor
    void doctorPersonal(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // Receptionist
    void receptionistPendingRequests(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};