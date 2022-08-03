#include "request_handler.h"
#include "domain.h"

std::list<const domain::Bus*> RequestHandler::GetAllBuses() const {
    std::list<const domain::Bus*> result;
    for (const auto bus_id : db_) {
        result.push_back(db_.GetBusPtr(bus_id));
    }
    return result;
}
