#include "request_handler.h"
#include "domain.h"

std::vector<const domain::Bus*> RequestHandler::GetAllBuses() const {
    std::vector<const domain::Bus*> result;
    for (const auto bus_id : db_) {
        result.push_back(db_.GetBusPtr(bus_id));
    }
    return result;
}
