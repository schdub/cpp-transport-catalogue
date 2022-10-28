#include "serialization.h"

#include "domain.h"

#include <transport_catalogue.pb.h>
#include <cassert>
#include <fstream>

const std::string& Serialization::GetFilePath() {
    const auto & ref_settings = domain::Settings::instance().serialize_settings;
    assert(ref_settings.has_value());
    assert(!ref_settings.value().file.empty());
    return ref_settings.value().file;
}

void Serialization::Write(const domain::BUSES & busses, const domain::STOPS & stops) {
    ::transport_catalogue_pb::Catalogue cat;
    // stops
    for (const domain::STOP & stop : stops) {
        ::transport_catalogue_pb::Stop* pbStop = cat.add_stops();
        pbStop->set_name(stop.stop_name_);
        pbStop->set_lat(stop.coordinates_.lat);
        pbStop->set_lng(stop.coordinates_.lng);
        for (const std::pair<std::string, size_t> & pair : stop.distances_) {
            ::transport_catalogue_pb::Distance* pbDistance = pbStop->add_road_distances();
            pbDistance->set_name(pair.first);
            pbDistance->set_distance(pair.second);
        }
    }
    // busses
    for (const domain::BUS & bus : busses) {
        ::transport_catalogue_pb::Bus* pbBus = cat.add_buses();
        pbBus->set_bus_id(std::string(bus.bus_id_));
        pbBus->set_is_round_trip(bus.is_round_trip_);
        for (const std::string & stop_name : bus.stops_) {
            pbBus->add_stops(stop_name);
        }
    }
    // output to file stream
    std::ofstream output_file(GetFilePath(), std::ios::binary);
    cat.SerializeToOstream(&output_file);
}

bool Serialization::Read(domain::BUSES & busses, domain::STOPS & stops) {


    // input from file stream
    std::ifstream input_file(GetFilePath(), std::ios::binary);

    ::transport_catalogue_pb::Catalogue cat;
    if (!cat.ParseFromIstream(&input_file)) {
        return false;
    }

    stops.clear();
    for (int i = 0; i < cat.stops_size(); ++i) {
        domain::STOP stop;
        const ::transport_catalogue_pb::Stop& ref_stop = cat.stops(i);
        stop.stop_name_ = ref_stop.name();
        stop.coordinates_.lat = ref_stop.lat();
        stop.coordinates_.lng = ref_stop.lng();

        for (int j = 0; j < ref_stop.road_distances_size(); ++j) {
            const ::transport_catalogue_pb::Distance& ref_dist = ref_stop.road_distances(j);
            stop.distances_.emplace_back(ref_dist.name(), ref_dist.distance());
        }
        stops.emplace_back(std::move(stop));
    }

    busses.clear();
    for (int i = 0; i < cat.buses_size(); ++i) {
        domain::BUS bus;
        const ::transport_catalogue_pb::Bus& ref_bus = cat.buses(i);
        bus.bus_id_ = ref_bus.bus_id();
        bus.is_round_trip_ = ref_bus.is_round_trip();
        for (int j = 0; j < ref_bus.stops_size(); ++j) {
            const std::string & stop_name = ref_bus.stops(j);
            bus.stops_.emplace_back(stop_name);
        }
        busses.emplace_back(std::move(bus));
    }

    return true;
}
