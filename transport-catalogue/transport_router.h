#pragma once

#include <unordered_map>
#include <memory>
#include <limits>

#include "graph.h"
#include "router.h"
#include "domain.h"

namespace tcatalogue {
    class TransportCatalogue;
}

class RouteGraph {
public:
    using Ty = double;
    using Ed = graph::Edge<Ty>;
    using GRAPH = graph::DirectedWeightedGraph<Ty>;
    using ROUTER = graph::Router<Ty>;

    RouteGraph(tcatalogue::TransportCatalogue & db,
               const domain::RoutingSettings &routing_settings);

    ~RouteGraph();

    bool Build(const std::string & from, const std::string & to, ROUTER::RouteInfo & ri);

    void FillResponse(const ROUTER::RouteInfo & route_info, domain::STAT_RESP_ROUTE & route_response);

    bool isPrepared() const;

    void Prepare();

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

    double DistanceToTime(size_t distance);

    VertexContext * GetContextForStop(const domain::Stop * pStop);

    // создаем кольцевой маршрут
    void PrepareRouteRing(const domain::Bus * pBus);

    // не кольцевой маршрут
    void PrepareRouteNotRing(const domain::Bus * pBus);
}; // class RouteGraph
