
#include "OrderParser.hpp"

#include <fstream>
#include <limits>
#include <exception>

#include "MatchingEngine.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace {

bool readInt(
    const json& value,
    int& result,
    const std::string& field,
    std::string& error
) {
    if (!value.is_number_integer()) {
        error = "'" + field + "' must be an integer.";
        return false;
    }

    if (value.is_number_unsigned()) {
        const unsigned long long number =
            value.get<unsigned long long>();

        if (number >
            static_cast<unsigned long long>(
                std::numeric_limits<int>::max())) {
            error = "'" + field + "' is out of range.";
            return false;
        }

        result = static_cast<int>(number);
        return true;
    }

    const long long number = value.get<long long>();

    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max()) {
        error = "'" + field + "' is out of range.";
        return false;
    }

    result = static_cast<int>(number);
    return true;
}

bool parseRequest(
    const json& item,
    OrderRequest& request,
    std::string& error
) {
    if (!item.is_object()) {
        error = "Each request must be a JSON object.";
        return false;
    }

    const char* requiredFields[] = {
        "id", "type", "side", "price", "quantity"
    };

    // Reject unexpected fields, including a timestamp field.
    for (json::const_iterator it = item.begin();
         it != item.end(); ++it) {
        bool known = false;

        for (const char* field : requiredFields) {
            if (it.key() == field) {
                known = true;
                break;
            }
        }

        if (!known) {
            error = "Unexpected field '" + it.key() + "'.";
            return false;
        }
    }

    for (const char* field : requiredFields) {
        if (item.find(field) == item.end()) {
            error = "Missing required field '" +
                    std::string(field) + "'.";
            return false;
        }
    }

    if (!item["type"].is_string()) {
        error = "'type' must be a string.";
        return false;
    }

    if (!item["side"].is_string()) {
        error = "'side' must be a string.";
        return false;
    }

    const std::string type = item["type"].get<std::string>();
    const std::string side = item["side"].get<std::string>();

    if (type == "LIMIT") {
        request.type = OrderType::Limit;
    } else if (type == "MARKET") {
        request.type = OrderType::Market;
    } else {
        error = "Invalid type '" + type +
                "'. Expected LIMIT or MARKET.";
        return false;
    }

    if (side == "BUY") {
        request.side = Side::Buy;
    } else if (side == "SELL") {
        request.side = Side::Sell;
    } else {
        error = "Invalid side '" + side +
                "'. Expected BUY or SELL.";
        return false;
    }

    if (!readInt(item["id"], request.id, "id", error) ||
        !readInt(item["price"], request.price, "price", error) ||
        !readInt(item["quantity"], request.quantity,
                 "quantity", error)) {
        return false;
    }

    if (request.id <= 0) {
        error = "'id' must be positive.";
        return false;
    }

    if (request.quantity <= 0) {
        error = "'quantity' must be positive.";
        return false;
    }

    if (request.type == OrderType::Limit &&
        request.price <= 0) {
        error = "LIMIT order price must be positive.";
        return false;
    }

    if (request.type == OrderType::Market &&
        request.price != 0) {
        error = "MARKET order price must be zero.";
        return false;
    }

    return true;
}

} // namespace

bool OrderParser::parseFile(
    const std::string& filename,
    std::vector<OrderRequest>& requests,
    std::string& error
) {
    requests.clear();
    error.clear();

    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        error = "Could not open file: " + filename;
        return false;
    }

    json root;

    try {
        file >> root;
    } catch (const std::exception& e) {
        error = std::string("Invalid JSON: ") + e.what();
        return false;
    }

    if (!root.is_array()) {
        error = "The JSON root must be an array of orders.";
        return false;
    }

    for (std::size_t i = 0; i < root.size(); ++i) {
        OrderRequest request;
        std::string requestError;

        if (!parseRequest(root[i], request, requestError)) {
            error = "Request at array index " +
                    std::to_string(i) + ": " + requestError;
            requests.clear();
            return false;
        }

        requests.push_back(request);
    }

    return true;
}