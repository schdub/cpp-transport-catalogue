#include "request_handler.h"
#include "transport_catalogue.h"
#include "domain.h"
#include "geo.h"
#include "router.h"
#include "ranges.h"
#include <cassert>
#include <sstream>
#include <unordered_map>
#include <limits>

using namespace domain;

// //////////////////////////////////////////////////////////// //

const size_t MAX_GRAPH_IDXES = 256;

class RouteGraph {
public:
    using Ty = double;
    using Ed = graph::Edge<Ty>;
    using GRAPH = graph::DirectedWeightedGraph<Ty>;
    using ROUTER = graph::Router<Ty>;

private:
    tcatalogue::TransportCatalogue & db_;
    const domain::RoutingSettings & routing_settings_;
    graph::VertexId current_vertex_id_ = 0;

    GRAPH graph_;
    std::shared_ptr<ROUTER> ptr_router_;

    struct VertexContext {
        graph::VertexId idx_waiting_ = std::numeric_limits<graph::VertexId>::max();
        graph::VertexId idx_arrive_ = std::numeric_limits<graph::VertexId>::max();
    };
    std::unordered_map<const domain::Stop*, VertexContext*> ctx_by_stop_;
    std::unordered_map<graph::VertexId, VertexContext*> vctx_by_idx_;

    enum class EDGE_TYPE {
        ed_Unknown,
        et_Wait,
        et_Bus,
    };
    struct RidingBus {
        size_t span_count_ = 0;
        const domain::Bus* bus_ = nullptr;
    };
    using EDGE_DATA = std::variant<const domain::Stop*, RidingBus >;
    std::unordered_map< graph::EdgeId, std::pair<EDGE_TYPE, EDGE_DATA> > et_by_eid_;

public:
    RouteGraph(tcatalogue::TransportCatalogue & db,
               const domain::RoutingSettings & routing_settings,
               size_t vertex_count)
        : db_(db)
        , routing_settings_(routing_settings)
        , current_vertex_id_(0)
        , graph_(vertex_count)
    {}

    ~RouteGraph() {
        for (auto [pStop, pCtx]: ctx_by_stop_) {
            delete pCtx; // !!! our class owns only context
        }
    }

    bool Build(const std::string & from, const std::string & to, ROUTER::RouteInfo & ri) {
        assert(isPrepared());
        Stop * stop_from  = db_.GetStop( from );
        Stop * stop_to    = db_.GetStop( to );

        if (stop_from && stop_to) {
            VertexContext * ctx_from = ctx_by_stop_[ stop_from ];
            VertexContext * ctx_to   = ctx_by_stop_[ stop_to ];
            graph::VertexId idx_from = ctx_from->idx_waiting_;
            graph::VertexId idx_to   = ctx_to->idx_waiting_;
            std::optional<ROUTER::RouteInfo> opt_route_info = ptr_router_->BuildRoute(idx_from, idx_to);
            if (opt_route_info.has_value()) {
                ri = std::move(opt_route_info.value());
                return true;
            }
        }
        return false;
    }

    void FillResponse(const ROUTER::RouteInfo & route_info, domain::STAT_RESP_ROUTE & route_response ) {
//        std::cerr << route_info.weight << std::endl;
        route_response.total_time = route_info.weight;
        route_response.items.clear();
        for (graph::EdgeId eid : route_info.edges) {
            const std::pair<EDGE_TYPE, EDGE_DATA> & edge = et_by_eid_[eid];
            const Ed & ed = graph_.GetEdge(eid);
            if (edge.first == EDGE_TYPE::et_Bus) {
                RidingBus bus_context = std::get<RidingBus>(edge.second);
                const domain::Bus * pBus = bus_context.bus_;
//                std::cerr << " " << pBus->id << " " << ed.weight << std::endl;
                STAT_RESP_ROUTE_ITEM_BUS item_bus;
                item_bus.bus        = pBus->id;
                item_bus.span_count = bus_context.span_count_;
                item_bus.time       = ed.weight;
                route_response.items.push_back({STAT_RESP_ROUTE_ITEM_TYPE::BUS, std::move(item_bus)});
            } else if (edge.first == EDGE_TYPE::et_Wait) {
                const domain::Stop* pStop = std::get<const domain::Stop*>(edge.second);
//                std::cerr << " waiting " << pStop->name << " " << ed.weight << std::endl;
                STAT_RESP_ROUTE_ITEM_WAIT item_wait;
                item_wait.stop_name = pStop->name;
                item_wait.time = ed.weight;
                route_response.items.push_back({STAT_RESP_ROUTE_ITEM_TYPE::WAIT, std::move(item_wait)});
            }
        }
    }

    bool isPrepared() const {
        return (ptr_router_ != nullptr);
    }

    double DistanceToTime(size_t distance) {
        //((2600 + 890) / (40. * 5./18.) ) / 60.
        double result = distance;
        result /= (routing_settings_.bus_velocity * 5 / 18);
        result /= 60;
        return result;
    }

    VertexContext * GetContextForStop(const domain::Stop * pStop) {
        auto it_ctx = ctx_by_stop_.find(pStop);
        if (it_ctx != ctx_by_stop_.end()) {
            return it_ctx->second;
        }
        VertexContext * result = new VertexContext;
        ctx_by_stop_[pStop]  = result;
        result->idx_waiting_ = current_vertex_id_++;
        result->idx_arrive_  = current_vertex_id_++;
        vctx_by_idx_[result->idx_waiting_] = result;
        return result;
    }

    // создаем кольцевой маршрут
    void PrepareRouteRing(const domain::Bus * pBus) {
        assert(pBus);
        assert(pBus->is_round_trip == true);

        std::vector<double> lengths;
        for (size_t i = 0, ie = pBus->stops.size() - 1; i < ie; ++i) {
            const Stop * pStopA = pBus->stops[i];
            VertexContext * vctxA = GetContextForStop(pStopA);

            Ed e_waitA;
            e_waitA.weight = routing_settings_.bus_wait_time;
            e_waitA.from   = vctxA->idx_waiting_;
            e_waitA.to     = vctxA->idx_arrive_;

            auto wid = graph_.AddEdge(e_waitA);
            et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Wait, pStopA);

            const Stop * pStopB = pBus->stops[(i+1) % ie];
            VertexContext * vctxB = GetContextForStop(pStopB);
            size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

            Ed e_stopB;
            e_stopB.weight = DistanceToTime(distance);
            e_stopB.from   = e_waitA.to;
            e_stopB.to     = vctxB->idx_waiting_;
            wid = graph_.AddEdge(e_stopB);
            et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{1, pBus});

            lengths.push_back(e_stopB.weight);
        }
        // создаем пути со span_count >= 2
        for (size_t i = 0, ie = pBus->stops.size() - 1; i < ie; ++i) {
            const Stop * pStopA = pBus->stops[i];
            VertexContext * vctxA = GetContextForStop(pStopA);
            for (size_t j = i + 2, je = pBus->stops.size() - 1 + 2; j < je; ++j) {
                const Stop * pStopB = pBus->stops[j % ie];
                VertexContext * vctxB = GetContextForStop(pStopB);
                Ed e_stopB;
                e_stopB.from   = vctxA->idx_arrive_;
                e_stopB.to     = vctxB->idx_waiting_;
                e_stopB.weight = 0.0;
                size_t span_count = j - i;
                for (size_t cnt = 0; cnt < span_count; ++cnt) {
                    e_stopB.weight += lengths[(i + cnt) % lengths.size()];
                }
                auto wid = graph_.AddEdge(e_stopB);
                et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{span_count, pBus});
            }
        }
    } // PrepareRouteRing()

    // не кольцевой маршрут
    void PrepareRouteNotRing(const domain::Bus * pBus) {
        assert(pBus);
        assert(pBus->is_round_trip == false);

        std::vector<double> lengths_up;
        for (size_t i = 0, ie = pBus->stops.size(); i < ie; ++i) {
            const Stop * pStopA = pBus->stops[i];
            VertexContext * vctxA = GetContextForStop(pStopA);

            Ed e_waitA;
            e_waitA.weight = routing_settings_.bus_wait_time;
            e_waitA.from   = vctxA->idx_waiting_;
            e_waitA.to     = vctxA->idx_arrive_;

            auto wid = graph_.AddEdge(e_waitA);
            et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Wait, pStopA);

            if (i + 1 >= ie) break;

            const Stop * pStopB = pBus->stops[(i+1) % ie];
            VertexContext * vctxB = GetContextForStop(pStopB);

            size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

            Ed e_stopB;
            e_stopB.weight = DistanceToTime(distance);
            e_stopB.from   = e_waitA.to;
            e_stopB.to     = vctxB->idx_waiting_;
            wid = graph_.AddEdge(e_stopB);
            et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{1, pBus});

            lengths_up.push_back(e_stopB.weight);
        }
        // идем в обратном направлении
        std::vector<double> lengths_dn;
        for (size_t i = pBus->stops.size() - 1; i > 0; --i) {
            const Stop * pStopA = pBus->stops[i];
            const Stop * pStopB = pBus->stops[i-1];
            size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

            VertexContext * vctxA = GetContextForStop(pStopA);
            assert(vctxA);

            VertexContext * vctxB = GetContextForStop(pStopB);
            assert(vctxB);

            Ed edge;
            edge.weight = DistanceToTime(distance);
            edge.from   = vctxA->idx_arrive_;
            edge.to     = vctxB->idx_waiting_;
            auto wid    = graph_.AddEdge(edge);
            et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{1, pBus});

            lengths_dn.push_back(edge.weight);
        }
#if 1
        std::reverse(lengths_dn.begin(), lengths_dn.end());

        // создаем пути со span_count >= 2 в прямом направлении
        for (size_t i = 0, ie = pBus->stops.size(); i < ie; ++i) {
            const Stop * pStopA = pBus->stops[i];
            VertexContext * vctxA = GetContextForStop(pStopA);
            for (size_t j = i + 2, je = pBus->stops.size() + 2; j < je; ++j) {
                const Stop * pStopB = pBus->stops[j % ie];
                VertexContext * vctxB = GetContextForStop(pStopB);
                Ed e_stopB;
                e_stopB.from   = vctxA->idx_arrive_;
                e_stopB.to     = vctxB->idx_waiting_;
                e_stopB.weight = 0.0;
                size_t span_count = j - i;
                for (size_t cnt = 0; cnt < span_count; ++cnt) {
                    e_stopB.weight += lengths_up[(i + cnt) % lengths_up.size()];
                }
                auto wid = graph_.AddEdge(e_stopB);
                et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{span_count, pBus});
            }
        }
        // создаем пути со span_count >= 2 в обратном направлении
        for (size_t i = pBus->stops.size()-1; i > 0; --i) {
            const Stop * pStopA = pBus->stops[i];
            VertexContext * vctxA = GetContextForStop(pStopA);
            for (size_t j = pBus->stops.size() + 2, je = i + 2; j > je; --j) {
                const Stop * pStopB = pBus->stops[j % pBus->stops.size()];
                VertexContext * vctxB = GetContextForStop(pStopB);
                Ed e_stopB;
                e_stopB.from   = vctxA->idx_arrive_;
                e_stopB.to     = vctxB->idx_waiting_;
                e_stopB.weight = 0.0;
                size_t span_count = j - i;
                for (size_t cnt = 0; cnt < span_count; ++cnt) {
                    e_stopB.weight += lengths_dn[(i + cnt) % lengths_dn.size()];
                }
                auto wid = graph_.AddEdge(e_stopB);
                et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{span_count, pBus});
            }
        }
#endif
    } // PrepareRouteNotRing()

    void Prepare() {
        current_vertex_id_ = 0;
        for ( const auto & bus_id : db_ ) {
            const Bus * pBus = db_.GetBusPtr(bus_id);
            if (pBus->is_round_trip) {
                PrepareRouteRing(pBus);
            } else {
                PrepareRouteNotRing(pBus);
            }
        }
        ptr_router_.reset(new ROUTER(graph_));
    } // Prepare()
}; // class RouteGraph

// //////////////////////////////////////////////////////////// //

RequestHandler::RequestHandler(tcatalogue::TransportCatalogue & db,
                               renderer::MapRenderer & drawer,
                               const domain::RoutingSettings & routing_settings)
    : db_(db)
    , drawer_(drawer)
    , route_graph_(new RouteGraph(db, routing_settings, MAX_GRAPH_IDXES))
{}

const domain::Bus& RequestHandler::GetBus(domain::BusId id) const {
    return db_.GetBus(id);
}

domain::StopBusesOpt RequestHandler::GetStopBuses(std::string_view stop_name) const {
    return db_.GetStopBuses(stop_name);
}

std::vector<const domain::Bus*> RequestHandler::GetAllBuses() const {
    std::vector<const domain::Bus*> result;
    for (const auto bus_id : db_) {
        result.push_back(db_.GetBusPtr(bus_id));
    }
    return result;
}

// расчитываем географическую и актуальную дистанцию для маршрута, указанного автобуса.
std::pair<double, double> RequestHandler::CalculateRouteLength(const domain::Bus & bus) const {
    double geographical = 0.0;
    double actual = 0.0;
    const auto & v = bus.stops;
    assert(v.size() >= 2);
    for (size_t i = 0; i + 1 < v.size(); ++i) {
        const Stop* pStopA = v[i];
        const Stop* pStopB = v[i+1];
        double distance = ComputeDistance( pStopA->coordinates, pStopB->coordinates );
        geographical += distance;
        size_t meters = db_.GetDistanceBetween( pStopA, pStopB );
        actual += meters;
    }
    if (bus.is_round_trip == false) {
        geographical *= 2;
        for (size_t i = v.size()-1; i > 0; --i) {
            const Stop* pStopA = v[i];
            const Stop* pStopB = v[i-1];
            size_t meters = db_.GetDistanceBetween( pStopA, pStopB );
            actual += meters;
        }
    }
    return std::make_pair(geographical, actual);
}

std::string RequestHandler::DrawMap() const {
    std::stringstream stream;
    svg::Document doc = drawer_.Render( GetAllBuses() );
    doc.Render(stream);
    return stream.str();
}

bool RequestHandler::HandleRoute(const domain::STAT_REQ_ROUTE & route_request,
                                 domain::STAT_RESP_ROUTE & route_response) const {
    if (!route_graph_->isPrepared()) {
        route_graph_->Prepare();
    }

//    std::cerr << route_request.from_ << " " << route_request.to_ << std::endl;

    RouteGraph::ROUTER::RouteInfo route_info;
    if (route_graph_->Build(route_request.from_, route_request.to_, route_info)) {
        route_graph_->FillResponse(route_info, route_response);
        return true;
    }
    return false;
}
