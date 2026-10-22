
#ifndef ORDER_PARSER_HPP
#define ORDER_PARSER_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "Order.hpp"

// Defined in MatchingEngine.hpp.
enum class OrderType;

struct OrderRequest {
    int id;
    OrderType type;
    Side side;
    int price;
    int quantity;
    std::size_t requestIndex;
    // Preserve the original values from the JSON input.
    std::string originalType;
    std::string originalSide;
};

struct ParserRejection {
    std::size_t requestIndex;
    bool hasOrderId;
    int orderId;
    std::string reason;
    std::string originalRecord;
};

class OrderParser {
public:
    // Returns false only if the file cannot be read or the
    // complete JSON document/root structure is invalid.
    //
    // Invalid individual records are stored in rejections.
    // Validly structured records are stored in requests.
    static bool parseFile(
        const std::string& filename,
        std::vector<OrderRequest>& requests,
        std::vector<ParserRejection>& rejections,
        std::string& error
    );
};

#endif