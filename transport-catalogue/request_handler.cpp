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
        const domain::Stop* stop_ = nullptr;
        graph::VertexId idx_waiting_ = std::numeric_limits<graph::VertexId>::max();
        std::unordered_map<const domain::Bus*, graph::VertexId> idx_by_bus_;
    };
    std::unordered_map<const domain::Stop*, VertexContext*> ctx_by_stop_;
    std::unordered_map<graph::VertexId, VertexContext*> vctx_by_idx_;

public:
    RouteGraph(tcatalogue::TransportCatalogue & db,
               const domain::RoutingSettings & routing_settings)
        : db_(db)
        , routing_settings_(routing_settings)
        , graph_(256)
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

        for (graph::EdgeId eid : route_info.edges) {
            const Ed & ed = graph_.GetEdge(eid);

            VertexContext * vctxA = vctx_by_idx_[ed.from];
            if (vctxA && vctxA->stop_) {
                std::cerr << "A: " << ed.from << " " << vctxA->stop_->name << std::endl;
            }

            VertexContext * vctxB = vctx_by_idx_[ed.to];
            if (vctxB && vctxB->stop_) {
                std::cerr << "B: " << ed.to << " " << vctxB->stop_->name << std::endl;
            }
        }
    }

    bool isPrepared() const {
        return (ptr_router_ != nullptr);
    }

    void Prepare() {
        const double bus_waiting_time = routing_settings_.bus_wait_time;
        const double bus_speed        = routing_settings_.bus_velocity;
        graph::VertexId current_vertex_id = 0;

        // создаем пути
        for ( const auto & bus_id : db_ ) {
            const Bus * pBus = db_.GetBusPtr(bus_id);
            // обрабатываем кольцевой маршрут
            for (size_t i = 0, ie = pBus->stops.size(); i < ie; ++i) {
                const Stop * pStopA = pBus->stops[i];
                VertexContext * vctxA = ctx_by_stop_[pStopA];
                if (!vctxA) {
                    vctxA = new VertexContext;
                    vctxA->stop_ = pStopA;
                    vctxA->idx_waiting_ = current_vertex_id++;
                    ctx_by_stop_[pStopA] = vctxA;
                    vctx_by_idx_[vctxA->idx_waiting_] = vctxA;
                }

                Ed e_waitA;
                e_waitA.weight = bus_waiting_time;
                e_waitA.from   = vctxA->idx_waiting_;
                e_waitA.to     = current_vertex_id++;
                vctxA->idx_by_bus_[pBus] = e_waitA.to;

                graph_.AddEdge(e_waitA);

                if (i + 1 >= ie) {
                    if (pBus->is_round_trip == false) break;
                }

                const Stop * pStopB = pBus->stops[(i+1) % ie];
                VertexContext * vctxB = ctx_by_stop_[pStopB];
                if (!vctxB) {
                    vctxB = new VertexContext;
                    vctxB->stop_ = pStopB;
                    vctxB->idx_waiting_ = current_vertex_id++;
                    ctx_by_stop_[pStopB] = vctxB;
                    vctx_by_idx_[vctxB->idx_waiting_] = vctxB;
                }

                size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

                Ed e_stopB;
                e_stopB.weight = distance ; //* bus_speed; // FIXME: proper calculation
                e_stopB.from   = e_waitA.to;
                e_stopB.to     = vctxB->idx_waiting_;
                graph_.AddEdge(e_stopB);
            } // for (size_t i = 0)

            if (pBus->is_round_trip == false) {
                for (size_t i = pBus->stops.size()-1; i > 0; --i) {
                    const Stop * pStopA = pBus->stops[i];
                    const Stop * pStopB = pBus->stops[i-1];
                    size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

                    VertexContext * vctxA = ctx_by_stop_[pStopA];
                    assert(vctxA);

                    VertexContext * vctxB = ctx_by_stop_[pStopB];
                    assert(vctxB);

                    Ed edge;
                    edge.weight = distance ;//* bus_speed; // TODO: fix calc!!
                    edge.from   = vctxA->idx_by_bus_[pBus];
                    edge.to     = vctxB->idx_waiting_;
                    graph_.AddEdge(edge);
                }
            } // if (pBus->is_round_trip)
        } // for ( const std::string & bus_id : db_ )

        ptr_router_.reset(new ROUTER(graph_));
    }
}; // class RouteGraph

// //////////////////////////////////////////////////////////// //

RequestHandler::RequestHandler(tcatalogue::TransportCatalogue & db,
                               renderer::MapRenderer & drawer,
                               const domain::RoutingSettings & routing_settings)
    : db_(db)
    , drawer_(drawer)
    , route_graph_(new RouteGraph(db, routing_settings))
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

    RouteGraph::ROUTER::RouteInfo ri;
    if (!route_graph_->Build(route_request.from_, route_request.to_, ri)) {
        return false;
    }

    route_graph_->FillResponse(ri, route_response);

    return true;
}
