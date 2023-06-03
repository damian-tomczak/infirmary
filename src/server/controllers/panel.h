#include "login_system.h"
#include "mailer.hpp"

#include "drogon/HttpController.h"

class Panel final : public drogon::HttpController<Panel>
{
public:
    // TODO: panel's redirects were designed for arch where doctor has its own separated interface
    // it would be nice to see refactor
    METHOD_LIST_BEGIN
    METHOD_ADD(::Panel::index, "");

    METHOD_ADD(::Panel::patient, "patient");
    METHOD_ADD(::Panel::receptionist, "receptionist");

    METHOD_ADD(::Panel::visitInformation, "visit-information");

    METHOD_ADD(::Panel::editPersonal, "edit-personal");

    METHOD_ADD(::Panel::patientPersonal, "patient/personal");
    METHOD_ADD(::Panel::patientCalendar, "patient/calendar");

    METHOD_ADD(::Panel::doctorInformation, "admin/doctor-information");
    METHOD_ADD(::Panel::patientInformation, "admin/patient-information");

    METHOD_ADD(::Panel::receptionistPendingRequests, "admin/pending-requests");
    METHOD_ADD(::Panel::statistics, "admin/statistics");
    METHOD_ADD(::Panel::addDoctor, "admin/add-doctor");
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

    void editPersonal(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void patientPersonal(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patientCalendar(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void doctorInformation(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patientInformation(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void receptionistPendingRequests(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void statistics(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void addDoctor(const drogon::HttpRequestPtr& pReq,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    bool appendNote(std::string& currentNote, const std::string& noteToAppend);
    void appendDoctorsToSideMenu(drogon::HttpViewData& data);
    bool cancelVisit(const std::optional<int32_t> visitId);

    inline static constexpr auto maxRequestedVisitPerUser{2};
    inline static std::array<std::string, 12> hours{
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
};