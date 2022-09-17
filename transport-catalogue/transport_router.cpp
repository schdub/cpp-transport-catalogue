#include "transport_router.h"
#include "transport_catalogue.h"
#include "log_duration.h"

using namespace domain;

RouteGraph::RouteGraph(tcatalogue::TransportCatalogue & db,
                       const domain::RoutingSettings & routing_settings)
    : db_(db)
    , routing_settings_(routing_settings)
    , current_vertex_id_(0)
    , graph_(db.StopCount() * 2)
{}

RouteGraph::~RouteGraph() {
    for (auto [pStop, pCtx]: ctx_by_stop_) {
        delete pCtx; // !!! our class owns only context
    }
}

// получаем информацию о пути из графа и возвращаем его в переменную ri,
// если есть таковой. в случае ошибочных ситуаций функция возвращает ложь.
bool RouteGraph::Build(const std::string & from, const std::string & to, ROUTER::RouteInfo & ri) {
    assert(isPrepared());
    Stop * stop_from  = db_.GetStop( from );
    Stop * stop_to    = db_.GetStop( to );
    if (stop_from && stop_to) {
        VertexContext * ctx_from = ctx_by_stop_[ stop_from ];
        VertexContext * ctx_to   = ctx_by_stop_[ stop_to ];
        if (ctx_from && ctx_to) {
            graph::VertexId idx_from = ctx_from->idx_waiting_;
            graph::VertexId idx_to   = ctx_to->idx_waiting_;
            std::optional<ROUTER::RouteInfo> opt_route_info = ptr_router_->BuildRoute(idx_from, idx_to);
            if (opt_route_info.has_value()) {
                ri = std::move(opt_route_info.value());
                return true;
            }
        }
    }
    return false;
}

// заполняем отклик на основании данных из о пути из графа.
void RouteGraph::FillResponse(const ROUTER::RouteInfo & route_info, domain::STAT_RESP_ROUTE & route_response ) {
    route_response.total_time = route_info.weight;
    route_response.items.clear();
    for (graph::EdgeId eid : route_info.edges) {
        const std::pair<EDGE_TYPE, EDGE_DATA> & edge = et_by_eid_[eid];
        const Ed & ed = graph_.GetEdge(eid);
        if (edge.first == EDGE_TYPE::et_Bus) {
            RidingBus bus_context = std::get<RidingBus>(edge.second);
            const domain::Bus * pBus = bus_context.bus_;
            STAT_RESP_ROUTE_ITEM_BUS item_bus;
            item_bus.bus        = pBus->id;
            item_bus.span_count = bus_context.span_count_;
            item_bus.time       = ed.weight;
            route_response.items.emplace_back(STAT_RESP_ROUTE_ITEM{STAT_RESP_ROUTE_ITEM_TYPE::BUS, std::move(item_bus)});
        } else if (edge.first == EDGE_TYPE::et_Wait) {
            const domain::Stop* pStop = std::get<const domain::Stop*>(edge.second);
            STAT_RESP_ROUTE_ITEM_WAIT item_wait;
            item_wait.stop_name = pStop->name;
            item_wait.time = ed.weight;
            route_response.items.emplace_back(STAT_RESP_ROUTE_ITEM{STAT_RESP_ROUTE_ITEM_TYPE::WAIT, std::move(item_wait)});
        }
    }
}

// построин ли уже был граф?
bool RouteGraph::isPrepared() const {
    return (ptr_router_ != nullptr);
}

// конвертируем указанное расстояние в метрах во
// время, которое будет затрачено при указанной скорости
double RouteGraph::DistanceToTime(size_t distance) {
    double result = distance;
#if 1
    result /= (routing_settings_.bus_velocity * 5 / 18);
    result /= 60;
#endif
    return result;
}

// получаем контекст указанной вершины
RouteGraph::VertexContext * RouteGraph::GetContextForStop(const domain::Stop * pStop) {
    auto it_ctx = ctx_by_stop_.find(pStop);
    if (it_ctx != ctx_by_stop_.end()) {
        return it_ctx->second;
    }
    VertexContext * result = new VertexContext; // удаляется в деструкторе нашего класса
    ctx_by_stop_[pStop]  = result;
    result->idx_waiting_ = current_vertex_id_++;
    result->idx_arrive_  = current_vertex_id_++;
    return result;
}

// создаем кольцевой маршрут
void RouteGraph::PrepareRouteRing(const domain::Bus * pBus) {
    assert(pBus);
    assert(pBus->is_round_trip == true);

    std::vector<Ty> lengths; // информация о весах ребер для span_cout == 1
    for (size_t i = 0, ie = pBus->stops.size()-1; i < ie; ++i) {
        // получаем либо создаем контекст данной остановки
        const Stop * pStopA = pBus->stops[i];
        VertexContext * vctxA = GetContextForStop(pStopA);

        // создаем ребро ожидания на данной остановке
        Ed e_waitA;
        e_waitA.weight = routing_settings_.bus_wait_time;
        e_waitA.from   = vctxA->idx_waiting_;
        e_waitA.to     = vctxA->idx_arrive_;

        // добавим в граф
        auto wid = graph_.AddEdge(e_waitA);
        et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Wait, pStopA);

        // информация об остановке назначения
        const Stop * pStopB = pBus->stops[(i+1) % pBus->stops.size()];
        VertexContext * vctxB = GetContextForStop(pStopB);
        size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

        // создаем ребро поездки, при span_count == 1
        Ed e_stopB;
        e_stopB.weight = DistanceToTime(distance);
        e_stopB.from   = e_waitA.to;
        e_stopB.to     = vctxB->idx_waiting_;
        wid = graph_.AddEdge(e_stopB);
        et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{1, pBus});

        lengths.push_back(e_stopB.weight);
    }
    // создаем пути со span_count >= 2
    for (size_t i = 0, ie = pBus->stops.size() - 2; i < ie; ++i) {
        const Stop * pStopA = pBus->stops[i];
        VertexContext * vctxA = GetContextForStop(pStopA);
        for (size_t j = i + 2; j < pBus->stops.size(); ++j) {
            const Stop * pStopB = pBus->stops[j % (pBus->stops.size()-1)];
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
void RouteGraph::PrepareRouteNotRing(const domain::Bus * pBus) {
    assert(pBus);
    assert(pBus->is_round_trip == false);

    std::vector<Ty> lengths_up; // храним веса ребер между соедененными остановками
                                // для прохода в прямом направлении
    // выполняем действия для span_count == 1
    for (size_t i = 0;; ++i) {
        // получаем контекси для соответствующей остановки
        const Stop * pStopA = pBus->stops[i];
        VertexContext * vctxA = GetContextForStop(pStopA);

        // создаем ребро ожидания
        Ed e_waitA;
        e_waitA.weight = routing_settings_.bus_wait_time;
        e_waitA.from   = vctxA->idx_waiting_;
        e_waitA.to     = vctxA->idx_arrive_;

        auto wid = graph_.AddEdge(e_waitA);
        et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Wait, pStopA);

        // нужно ли создавать ребро поездки?
        if (i + 1 >= pBus->stops.size()) break;

        // получаем информации о следующей остановке
        const Stop * pStopB = pBus->stops[(i+1)];
        VertexContext * vctxB = GetContextForStop(pStopB);

        size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

        // соединяем две остановки ребром. причем, начальная вершина
        // ребра это вершина после окончания ожидания на остановке А
        // конечная же, это начало ожидания на остановке Б
        Ed e_stopB;
        e_stopB.weight = DistanceToTime(distance);
        e_stopB.from   = e_waitA.to;
        e_stopB.to     = vctxB->idx_waiting_;
        wid = graph_.AddEdge(e_stopB);
        et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{1, pBus});

        // запоминаем вес ребра для последующих
        lengths_up.push_back(e_stopB.weight);
    }

    // идем в обратном направлении
    std::vector<Ty> lengths_dn;
    for (size_t i = pBus->stops.size() - 1; i > 0; --i) {
        const Stop * pStopA = pBus->stops[i];
        const Stop * pStopB = pBus->stops[i-1];
        size_t distance = db_.GetDistanceBetween( pStopA, pStopB );

        // оба контектса созданы в цикле выше, и уже
        // должны существовать
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

    // создаем пути со span_count >= 2 в прямом направлении
    for (size_t i = 0, ie = pBus->stops.size() - 2; i < ie; ++i) {
        const Stop * pStopA = pBus->stops[i];
        VertexContext * vctxA = GetContextForStop(pStopA);
        for (size_t j = i + 2, je = pBus->stops.size(); j < je; ++j) {
            const Stop * pStopB = pBus->stops[j];
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
    std::reverse(lengths_dn.begin(), lengths_dn.end());
    for (size_t i = pBus->stops.size()-1; i >= 2; --i) {
        const Stop * pStopA = pBus->stops[i];
        VertexContext * vctxA = GetContextForStop(pStopA);
        for (size_t j = i - 2;; --j) {
            const Stop * pStopB = pBus->stops[j];
            VertexContext * vctxB = GetContextForStop(pStopB);
            Ed e_stopB;
            e_stopB.from   = vctxA->idx_arrive_;
            e_stopB.to     = vctxB->idx_waiting_;
            e_stopB.weight = 0.0;
            size_t span_count = i - j;
            for (size_t cnt = 0; cnt < span_count; ++cnt) {
                e_stopB.weight += lengths_dn[(i - cnt - 1)];
            }
            auto wid = graph_.AddEdge(e_stopB);
            et_by_eid_[wid] = std::make_pair(EDGE_TYPE::et_Bus, RidingBus{span_count, pBus});
            if (j == 0) break;
        }
    }
} // PrepareRouteNotRing()

// поочередно пересчитываем информацию о всех маршрутах.
void RouteGraph::Prepare() {
//    LOG_DURATION(__FUNCTION__);
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
