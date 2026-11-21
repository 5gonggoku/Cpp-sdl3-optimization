#include "data/Json.h"

#include <charconv>
#include <fstream>

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    bool Run(JsonValue& out, JsonError& err)
    {
        err_ = &err;
        if (text_.size() > kJsonMaxTextSize) return Fail(0, "입력이 너무 큼 (한도 1MiB)");

        if (text_.size() >= 3 && static_cast<unsigned char>(text_[0]) == 0xEF &&
            static_cast<unsigned char>(text_[1]) == 0xBB && static_cast<unsigned char>(text_[2]) == 0xBF) {
            pos_ = 3;
        }
        SkipWhitespace();
        if (!ParseValue(out, 1)) return false;
        SkipWhitespace();
        if (pos_ != text_.size()) return Fail(pos_, "값 뒤에 불필요한 내용이 있음");
        return true;
    }

private:
    bool AtEnd() const { return pos_ >= text_.size(); }
    char Peek() const { return text_[pos_]; }

    void LineCol(std::size_t offset, int& line, int& col) const
    {
        line = 1;
        col = 1;
        for (std::size_t i = 0; i < offset && i < text_.size(); ++i) {
            if (text_[i] == '\n') { ++line; col = 1; }
            else ++col;
        }
    }

    bool Fail(std::size_t offset, const char* message)
    {
        LineCol(offset, err_->line, err_->column);
        err_->message = message;
        return false;
    }

    void SkipWhitespace()
    {
        while (!AtEnd() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\n' || Peek() == '\r')) ++pos_;
    }

    bool ParseValue(JsonValue& out, int depth)
    {
        if (depth > kJsonMaxDepth) return Fail(pos_, "중첩이 너무 깊음 (한도 64)");
        if (AtEnd()) return Fail(pos_, "값이 필요한데 입력이 끝남");
        LineCol(pos_, out.line, out.column);

        const char c = Peek();
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == '"') {
            out.type = JsonType::String;
            return ParseString(out.string);
        }
        if (c == '-' || IsDigit(c)) return ParseNumber(out);
        if (MatchWord("true")) { out.type = JsonType::Bool; out.boolean = true; return true; }
        if (MatchWord("false")) { out.type = JsonType::Bool; out.boolean = false; return true; }
        if (MatchWord("null")) { out.type = JsonType::Null; return true; }
        return Fail(pos_, "알 수 없는 값");
    }

    bool MatchWord(const std::string& word)
    {
        if (text_.compare(pos_, word.size(), word) != 0) return false;
        pos_ += word.size();
        return true;
    }

    bool ParseObject(JsonValue& out, int depth)
    {
        out.type = JsonType::Object;
        ++pos_;
        SkipWhitespace();
        if (!AtEnd() && Peek() == '}') { ++pos_; return true; }
        while (true) {
            SkipWhitespace();
            if (AtEnd() || Peek() != '"') return Fail(pos_, "객체의 키는 문자열이어야 함");
            const std::size_t keyPos = pos_;
            std::string key;
            if (!ParseString(key)) return false;
            for (const std::string& existing : out.keys) {
                if (existing == key) return Fail(keyPos, "중복된 키");
            }
            SkipWhitespace();
            if (AtEnd() || Peek() != ':') return Fail(pos_, "':'가 필요함");
            ++pos_;
            SkipWhitespace();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.keys.push_back(std::move(key));
            out.values.push_back(std::move(value));
            SkipWhitespace();
            if (AtEnd()) return Fail(pos_, "객체가 닫히지 않음");
            if (Peek() == ',') { ++pos_; continue; }
            if (Peek() == '}') { ++pos_; return true; }
            return Fail(pos_, "',' 또는 '}'가 필요함");
        }
    }

    bool ParseArray(JsonValue& out, int depth)
    {
        out.type = JsonType::Array;
        ++pos_;
        SkipWhitespace();
        if (!AtEnd() && Peek() == ']') { ++pos_; return true; }
        while (true) {
            SkipWhitespace();
            JsonValue element;
            if (!ParseValue(element, depth + 1)) return false;
            out.array.push_back(std::move(element));
            SkipWhitespace();
            if (AtEnd()) return Fail(pos_, "배열이 닫히지 않음");
            if (Peek() == ',') { ++pos_; continue; }
            if (Peek() == ']') { ++pos_; return true; }
            return Fail(pos_, "',' 또는 ']'가 필요함");
        }
    }

    bool ParseString(std::string& out)
    {
        ++pos_;
        while (true) {
            if (AtEnd()) return Fail(pos_, "문자열이 닫히지 않음");
            const char c = Peek();
            if (c == '"') { ++pos_; return true; }
            if (static_cast<unsigned char>(c) < 0x20) return Fail(pos_, "문자열 안에 제어 문자가 있음");
            if (c != '\\') { out.push_back(c); ++pos_; continue; }

            ++pos_;
            if (AtEnd()) return Fail(pos_, "문자열이 닫히지 않음");
            switch (Peek()) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': return Fail(pos_ - 1, "\\uXXXX 이스케이프는 지원하지 않음 (알려진 제한)");
                default: return Fail(pos_ - 1, "알 수 없는 이스케이프");
            }
            ++pos_;
        }
    }

    bool ParseNumber(JsonValue& out)
    {
        const std::size_t start = pos_;
        if (Peek() == '-') ++pos_;
        if (AtEnd() || !IsDigit(Peek())) return Fail(pos_, "숫자에 자릿수가 필요함");
        if (Peek() == '0') {
            ++pos_;
            if (!AtEnd() && IsDigit(Peek())) return Fail(pos_, "숫자는 0으로 시작할 수 없음");
        } else {
            while (!AtEnd() && IsDigit(Peek())) ++pos_;
        }
        if (!AtEnd() && Peek() == '.') {
            ++pos_;
            if (AtEnd() || !IsDigit(Peek())) return Fail(pos_, "소수점 뒤에 자릿수가 필요함");
            while (!AtEnd() && IsDigit(Peek())) ++pos_;
        }
        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
            ++pos_;
            if (!AtEnd() && (Peek() == '+' || Peek() == '-')) ++pos_;
            if (AtEnd() || !IsDigit(Peek())) return Fail(pos_, "지수에 자릿수가 필요함");
            while (!AtEnd() && IsDigit(Peek())) ++pos_;
        }

        double value = 0.0;
        const char* first = text_.data() + start;
        const char* last = text_.data() + pos_;
        const std::from_chars_result r = std::from_chars(first, last, value);
        if (r.ec != std::errc() || r.ptr != last) return Fail(start, "숫자가 범위를 벗어남");
        out.type = JsonType::Number;
        out.number = value;
        return true;
    }

    static bool IsDigit(char c) { return c >= '0' && c <= '9'; }

    const std::string& text_;
    std::size_t pos_ = 0;
    JsonError* err_ = nullptr;
};

}

const JsonValue* JsonValue::Find(const std::string& key) const
{
    if (type != JsonType::Object) return nullptr;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) return &values[i];
    }
    return nullptr;
}

std::string FormatError(const std::string& path, const JsonError& err)
{
    std::string s = path;
    if (err.line > 0) s += ":" + std::to_string(err.line) + ":" + std::to_string(err.column);
    s += ": " + err.message;
    return s;
}

bool ParseJson(const std::string& text, JsonValue& out, JsonError& err)
{
    out = JsonValue{};
    err = JsonError{};
    Parser parser(text);
    return parser.Run(out, err);
}

bool ReadTextFile(const std::string& path, std::string& out, std::string& errMsg)
{

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { errMsg = path + ": 파일을 열 수 없음"; return false; }
    const std::streamoff size = file.tellg();
    if (size < 0) { errMsg = path + ": 파일 크기를 알 수 없음"; return false; }
    if (static_cast<unsigned long long>(size) > kJsonMaxTextSize) {
        errMsg = path + ": 파일이 너무 큼 (한도 1MiB)";
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    if (size > 0 && !file.read(&out[0], size)) { errMsg = path + ": 읽기 실패"; return false; }
    return true;
}
