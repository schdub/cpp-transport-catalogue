#include "input_reader.h"
#include "stat_reader.h"
#include "transport_catalogue.h"
#include <iostream>

using namespace tcatalogue;

int main() {

    /*
     * Примерная структура программы:
     *
     * Считать JSON из stdin
     * Построить на его основе JSON базу данных транспортного справочника
     * Выполнить запросы к справочнику, находящиеся в массиве "stat_requests", построив JSON-массив
     * с ответами.
     * Вывести в stdout ответы в виде JSON
     */

    TransportCatalogue db;
    InputReader(std::cin, db);
    StatReader(std::cin, std::cout, db);
}
