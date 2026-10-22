
#include "OrderParser.hpp"

#include <exception>
#include <fstream>
#include <limits>
#include <string>

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

        if (number > static_cast<unsigned long long>(
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

    // Validate the record's structure.
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

    const std::string type = item["type"].get<std::string>();
    const std::string side = item["side"].get<std::string>();

    request.originalType = type;
    request.originalSide = side;
    // Validate data types, not business-rule values.
    if (!item["type"].is_string()) {
        error = "'type' must be a string.";
        return false;
    }

    if (!item["side"].is_string()) {
        error = "'side' must be a string.";
        return false;
    }

    if (!readInt(item["id"], request.id, "id", error) ||
        !readInt(item["price"], request.price, "price", error) ||
        !readInt(item["quantity"], request.quantity,
                 "quantity", error)) {
        return false;
    }

    // Convert recognized values normally.
    // Preserve unsupported values as invalid enum values so
    // the engine can reject them.
    const std::string req_type = item["type"].get<std::string>();
    const std::string req_side = item["side"].get<std::string>();

    if (req_type == "LIMIT") {
        request.type = OrderType::Limit;
    } else if (type == "MARKET") {
        request.type = OrderType::Market;
    } else {
        request.type = static_cast<OrderType>(-1);
    }

    if (req_side == "BUY") {
        request.side = Side::Buy;
    } else if (side == "SELL") {
        request.side = Side::Sell;
    } else {
        request.side = static_cast<Side>(-1);
    }

    // Deliberately do not reject:
    // id <= 0, quantity <= 0, invalid order type/side,
    // LIMIT price <= 0, or MARKET price != 0.
    // These are engine-level validation responsibilities.

    return true;
}

} // namespace

bool OrderParser::parseFile(
    const std::string& filename,
    std::vector<OrderRequest>& requests,
    std::vector<ParserRejection>& rejections,
    std::string& error
) {
    requests.clear();
    rejections.clear();
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
        const json& item = root[i];

        OrderRequest request = {};
        request.requestIndex = i;

        std::string requestError;

        if (!parseRequest(item, request, requestError)) {
            ParserRejection rejection;
            rejection.requestIndex = i;
            rejection.hasOrderId = false;
            rejection.orderId = 0;
            rejection.reason = requestError;
            rejection.originalRecord = item.dump();

            // Include the ID if it can be safely extracted,
            // even if another field made the record invalid.
            if (item.is_object()) {
                json::const_iterator idIt = item.find("id");

                if (idIt != item.end()) {
                    int id = 0;
                    std::string ignoredError;

                    if (readInt(*idIt, id, "id", ignoredError)) {
                        rejection.hasOrderId = true;
                        rejection.orderId = id;
                    }
                }
            }

            rejections.push_back(rejection);
            continue;
        }

        requests.push_back(request);
    }

    return true;
}