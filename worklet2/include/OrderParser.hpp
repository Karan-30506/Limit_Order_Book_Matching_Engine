#ifndef ORDER_PARSER_HPP
#define ORDER_PARSER_HPP

#include <string>
#include <vector>

#include "Order.hpp"

// Defined in MatchingEngine.hpp.
// Keep OrderType outside the Order class.
enum class OrderType;

struct OrderRequest {
    int id;
    OrderType type;
    Side side;
    int price;
    int quantity;
};

class OrderParser {
public:
    // Parses the complete file.
    // On failure, requests is cleared and error explains the problem.
    static bool parseFile(
        const std::string& filename,
        std::vector<OrderRequest>& requests,
        std::string& error
    );
};

#endif