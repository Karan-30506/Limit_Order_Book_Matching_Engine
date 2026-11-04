#include "ScenarioRunner.hpp"

#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Book.hpp"
#include "MatchingEngine.hpp"

namespace {

struct JsonValue {
    enum class Type { Null, Boolean, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    std::string scalar;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    JsonValue parse() {
        JsonValue value = parseValue();
        skipWhitespace();
        if (position_ != input_.size()) {
            throw std::runtime_error("unexpected trailing JSON data");
        }
        return value;
    }

private:
    const std::string& input_;
    std::size_t position_ = 0;

    void skipWhitespace() {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\t' ||
                input_[position_] == '\r' || input_[position_] == '\n')) {
            ++position_;
        }
    }

    bool consume(char expected) {
        skipWhitespace();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (!consume(expected)) {
            throw std::runtime_error(std::string("expected '") + expected + "'");
        }
    }

    static int hexDigit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    unsigned int parseHexCodepoint() {
        if (input_.size() - position_ < 4) {
            throw std::runtime_error("incomplete unicode escape");
        }
        unsigned int value = 0;
        for (int i = 0; i < 4; ++i) {
            const int digit = hexDigit(input_[position_++]);
            if (digit < 0) {
                throw std::runtime_error("invalid unicode escape");
            }
            value = value * 16 + static_cast<unsigned int>(digit);
        }
        return value;
    }

    static void appendUtf8(std::string& output, unsigned int codepoint) {
        if (codepoint <= 0x7f) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    std::string parseString() {
        expect('"');
        std::string result;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') return result;
            if (static_cast<unsigned char>(c) < 0x20) {
                throw std::runtime_error("control character in string");
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }
            if (position_ == input_.size()) {
                throw std::runtime_error("incomplete string escape");
            }
            switch (input_[position_++]) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    unsigned int codepoint = parseHexCodepoint();
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (input_.size() - position_ < 6 ||
                            input_[position_] != '\\' ||
                            input_[position_ + 1] != 'u') {
                            throw std::runtime_error("missing low surrogate");
                        }
                        position_ += 2;
                        const unsigned int low = parseHexCodepoint();
                        if (low < 0xdc00 || low > 0xdfff) {
                            throw std::runtime_error("invalid low surrogate");
                        }
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
                                    (low - 0xdc00);
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                        throw std::runtime_error("unexpected low surrogate");
                    }
                    appendUtf8(result, codepoint);
                    break;
                }
                default: throw std::runtime_error("invalid string escape");
            }
        }
        throw std::runtime_error("unterminated string");
    }

    JsonValue parseValue() {
        skipWhitespace();
        if (position_ == input_.size()) {
            throw std::runtime_error("unexpected end of JSON");
        }
        const char c = input_[position_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') {
            JsonValue value;
            value.type = JsonValue::Type::String;
            value.scalar = parseString();
            return value;
        }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        if (matchLiteral("true")) {
            JsonValue value;
            value.type = JsonValue::Type::Boolean;
            value.boolean = true;
            return value;
        }
        if (matchLiteral("false")) {
            JsonValue value;
            value.type = JsonValue::Type::Boolean;
            return value;
        }
        if (matchLiteral("null")) return JsonValue{};
        throw std::runtime_error("invalid JSON value");
    }

    bool matchLiteral(const char* literal) {
        const std::string word(literal);
        if (input_.compare(position_, word.size(), word) != 0) return false;
        position_ += word.size();
        return true;
    }

    JsonValue parseObject() {
        JsonValue value;
        value.type = JsonValue::Type::Object;
        expect('{');
        if (consume('}')) return value;
        do {
            skipWhitespace();
            if (position_ == input_.size() || input_[position_] != '"') {
                throw std::runtime_error("object key must be a string");
            }
            std::string key = parseString();
            expect(':');
            if (!value.object.emplace(std::move(key), parseValue()).second) {
                throw std::runtime_error("duplicate object key");
            }
        } while (consume(','));
        expect('}');
        return value;
    }

    JsonValue parseArray() {
        JsonValue value;
        value.type = JsonValue::Type::Array;
        expect('[');
        if (consume(']')) return value;
        do {
            value.array.push_back(parseValue());
        } while (consume(','));
        expect(']');
        return value;
    }

    JsonValue parseNumber() {
        const std::size_t start = position_;
        if (input_[position_] == '-') ++position_;
        if (position_ == input_.size()) throw std::runtime_error("invalid number");
        if (input_[position_] == '0') {
            ++position_;
            if (position_ < input_.size() && input_[position_] >= '0' &&
                input_[position_] <= '9') {
                throw std::runtime_error("leading zero in number");
            }
        } else {
            if (input_[position_] < '1' || input_[position_] > '9') {
                throw std::runtime_error("invalid number");
            }
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                ++position_;
            }
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            const std::size_t fractionStart = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                ++position_;
            }
            if (fractionStart == position_) throw std::runtime_error("invalid fraction");
        }
        if (position_ < input_.size() &&
            (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() &&
                (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            const std::size_t exponentStart = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                ++position_;
            }
            if (exponentStart == position_) throw std::runtime_error("invalid exponent");
        }
        JsonValue value;
        value.type = JsonValue::Type::Number;
        value.scalar = input_.substr(start, position_ - start);
        return value;
    }
};

const JsonValue* member(const JsonValue& value, const std::string& key) {
    if (value.type != JsonValue::Type::Object) return nullptr;
    const auto found = value.object.find(key);
    return found == value.object.end() ? nullptr : &found->second;
}

bool integer(const JsonValue* value, int& result) {
    if (!value || value->type != JsonValue::Type::Number ||
        value->scalar.find_first_of(".eE") != std::string::npos) {
        return false;
    }
    try {
        const long long parsed = std::stoll(value->scalar);
        if (parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        result = static_cast<int>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool stringValue(const JsonValue* value, std::string& result) {
    if (!value || value->type != JsonValue::Type::String) return false;
    result = value->scalar;
    return true;
}

bool sideValue(const JsonValue& step, Side& result) {
    std::string side;
    if (!stringValue(member(step, "side"), side)) return false;
    if (side == "BUY") result = Side::Buy;
    else if (side == "SELL") result = Side::Sell;
    else return false;
    return true;
}

bool readRequiredInt(
    const JsonValue& step,
    const char* field,
    int& value
) {
    return integer(member(step, field), value);
}

std::vector<int> queueAt(Book& book, int price, Side side) {
    LimitNode* level = (side == Side::Buy ? book.buyLimitMap : book.sellLimitMap).get(price);
    std::vector<int> result;
    if (level) {
        for (Order* order = level->queue.head; order; order = order->next) {
            result.push_back(order->id);
        }
    }
    return result;
}

void addError(ScenarioResult& result, std::size_t step, const std::string& message) {
    result.passed = false;
    result.errors.push_back("step " + std::to_string(step) + ": " + message);
}

} // namespace

ScenarioResult runScenarioJsonText(const std::string& text) {
    ScenarioResult result;
    JsonValue root;
    try {
        root = JsonParser(text).parse();
    } catch (const std::exception& error) {
        addError(result, 0, std::string("invalid JSON: ") + error.what());
        return result;
    }

    const JsonValue* steps = member(root, "steps");
    if (!steps || steps->type != JsonValue::Type::Array) {
        addError(result, 0, "scenario must contain a steps array");
        return result;
    }

    Book book;
    MatchingEngine engine(book);
    std::size_t tradeCursor = 0;
    bool lastOperationSucceeded = false;

    for (const JsonValue& step : steps->array) {
        ++result.steps;
        std::string type;
        if (!stringValue(member(step, "type"), type)) {
            addError(result, result.steps, "step type must be a string");
            continue;
        }

        if (type == "LIMIT" || type == "MARKET") {
            int id = 0, quantity = 0, price = 0;
            Side side = Side::Buy;
            if (!readRequiredInt(step, "id", id) ||
                !readRequiredInt(step, "quantity", quantity) ||
                !sideValue(step, side) ||
                (type == "LIMIT" && !readRequiredInt(step, "price", price))) {
                addError(result, result.steps, "invalid order fields");
                continue;
            }
            const Order order(id, price, quantity,
                              static_cast<long long>(result.steps), side);
            lastOperationSucceeded = engine.submitOrder(
                order, type == "LIMIT" ? OrderType::Limit : OrderType::Market);
            continue;
        }

        if (type == "CANCEL") {
            int id = 0;
            if (!readRequiredInt(step, "id", id)) {
                addError(result, result.steps, "invalid cancel order id");
                continue;
            }
            lastOperationSucceeded = book.cancelOrder(id);
            continue;
        }

        if (type == "MODIFY") {
            int id = 0, price = 0, quantity = 0;
            if (!readRequiredInt(step, "id", id) ||
                !readRequiredInt(step, "price", price) ||
                !readRequiredInt(step, "quantity", quantity)) {
                addError(result, result.steps, "invalid modify fields");
                continue;
            }
            lastOperationSucceeded = book.modifyOrder(id, price, quantity);
            continue;
        }

        if (type == "EXPECT_ACCEPT" || type == "EXPECT_REJECT") {
            const bool expected = type == "EXPECT_ACCEPT";
            if (lastOperationSucceeded != expected) {
                addError(result, result.steps,
                         expected ? "expected operation to be accepted"
                                  : "expected operation to be rejected");
            }
            continue;
        }

        if (type == "EXPECT_TRADE") {
            int buyID = 0, sellID = 0, price = 0, quantity = 0;
            if (!readRequiredInt(step, "buyId", buyID) ||
                !readRequiredInt(step, "sellId", sellID) ||
                !readRequiredInt(step, "price", price) ||
                !readRequiredInt(step, "quantity", quantity)) {
                addError(result, result.steps, "invalid expected trade fields");
                continue;
            }
            const auto& trades = engine.getTrades();
            if (tradeCursor >= trades.size()) {
                addError(result, result.steps, "expected trade is missing");
                continue;
            }
            const Trade& trade = trades[tradeCursor++];
            if (trade.buyOrderID != buyID || trade.sellOrderID != sellID ||
                trade.price != price || trade.quantity != quantity) {
                addError(result, result.steps, "trade does not match expectation");
            }
            continue;
        }

        if (type == "EXPECT_NO_MORE_TRADES") {
            if (tradeCursor != engine.getTrades().size()) {
                addError(result, result.steps, "unexpected unconsumed trades");
            }
            continue;
        }

        if (type == "EXPECT_BEST_BID" || type == "EXPECT_BEST_ASK") {
            const JsonValue* expected = member(step, "price");
            LimitNode* best = type == "EXPECT_BEST_BID"
                ? book.getBestBid() : book.getBestAsk();
            if (expected && expected->type == JsonValue::Type::String &&
                expected->scalar == "NONE") {
                if (best) addError(result, result.steps, "expected empty best price");
                continue;
            }
            int price = 0;
            if (!integer(expected, price)) {
                addError(result, result.steps, "invalid expected best price");
            } else if (!best || best->price != price) {
                addError(result, result.steps, "best price does not match expectation");
            }
            continue;
        }

        if (type == "EXPECT_VOLUME") {
            int price = 0, quantity = 0;
            Side side = Side::Buy;
            if (!readRequiredInt(step, "price", price) ||
                !readRequiredInt(step, "quantity", quantity) ||
                !sideValue(step, side)) {
                addError(result, result.steps, "invalid expected volume fields");
            } else if (book.getVolumeAt(price, side) != quantity) {
                addError(result, result.steps, "volume does not match expectation");
            }
            continue;
        }

        if (type == "EXPECT_QUEUE") {
            int price = 0;
            Side side = Side::Buy;
            const JsonValue* ids = member(step, "ids");
            std::vector<int> expected;
            bool valid = readRequiredInt(step, "price", price) &&
                         sideValue(step, side) && ids &&
                         ids->type == JsonValue::Type::Array;
            if (valid) {
                for (const JsonValue& id : ids->array) {
                    int parsed = 0;
                    if (!integer(&id, parsed)) {
                        valid = false;
                        break;
                    }
                    expected.push_back(parsed);
                }
            }
            if (!valid) {
                addError(result, result.steps, "invalid expected queue fields");
            } else if (queueAt(book, price, side) != expected) {
                addError(result, result.steps, "queue does not match expectation");
            }
            continue;
        }

        addError(result, result.steps, "unknown step type '" + type + "'");
    }

    return result;
}

ScenarioResult runScenarioJsonFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        ScenarioResult result;
        addError(result, 0, "unable to open scenario file '" + path + "'");
        return result;
    }
    std::ostringstream content;
    content << input.rdbuf();
    if (input.bad()) {
        ScenarioResult result;
        addError(result, 0, "unable to read scenario file '" + path + "'");
        return result;
    }
    return runScenarioJsonText(content.str());
}
