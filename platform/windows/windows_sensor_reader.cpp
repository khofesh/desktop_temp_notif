// Windows sensor reader using LibreHardwareMonitor's built-in HTTP server.
//
// Prerequisites:
//   1. LibreHardwareMonitor running as Administrator.
//   2. Options > Remote Web Server > Run  (exposes http://localhost:8085)
//
// The endpoint GET /data.json returns a recursive JSON tree of hardware nodes.
// Temperature sensor nodes have a Value string like "45.0 °C".

#include "windows_sensor_reader.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

// ---------------------------------------------------------------------------
// Minimal JSON parser — enough to traverse LHM's data.json tree.
// ---------------------------------------------------------------------------
namespace {

struct JVal {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool        b{};
    double      n{};
    std::string s;
    std::vector<JVal>                        arr;
    std::vector<std::pair<std::string, JVal>> obj;

    const JVal* get(const std::string& key) const {
        for (auto& [k, v] : obj)
            if (k == key) return &v;
        return nullptr;
    }
};

class JsonParser {
    const char* p_;
    const char* end_;

    void skip_ws() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' ||
                              *p_ == '\n' || *p_ == '\r'))
            ++p_;
    }

    std::string parse_string() {
        ++p_; // opening "
        std::string r;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\' && p_ + 1 < end_) {
                ++p_;
                switch (*p_) {
                    case '"':  r += '"';  break;
                    case '\\': r += '\\'; break;
                    case '/':  r += '/';  break;
                    case 'n':  r += '\n'; break;
                    case 'r':  r += '\r'; break;
                    case 't':  r += '\t'; break;
                    default:   r += *p_;  break;
                }
            } else {
                r += *p_;
            }
            ++p_;
        }
        if (p_ < end_) ++p_; // closing "
        return r;
    }

    JVal parse_value() {
        skip_ws();
        if (p_ >= end_) return {};
        JVal v;
        char c = *p_;
        if (c == '"') {
            v.type = JVal::Str;
            v.s    = parse_string();
        } else if (c == '{') {
            v.type = JVal::Obj;
            ++p_;
            skip_ws();
            while (p_ < end_ && *p_ != '}') {
                skip_ws();
                if (*p_ != '"') { ++p_; continue; } // malformed, skip
                std::string key = parse_string();
                skip_ws();
                if (p_ < end_ && *p_ == ':') ++p_;
                v.obj.emplace_back(std::move(key), parse_value());
                skip_ws();
                if (p_ < end_ && *p_ == ',') ++p_;
                skip_ws();
            }
            if (p_ < end_) ++p_; // '}'
        } else if (c == '[') {
            v.type = JVal::Arr;
            ++p_;
            skip_ws();
            while (p_ < end_ && *p_ != ']') {
                v.arr.push_back(parse_value());
                skip_ws();
                if (p_ < end_ && *p_ == ',') ++p_;
                skip_ws();
            }
            if (p_ < end_) ++p_; // ']'
        } else if (c == 't') { v.type = JVal::Bool; v.b = true;  p_ += 4; }
          else if (c == 'f') { v.type = JVal::Bool; v.b = false; p_ += 5; }
          else if (c == 'n') { p_ += 4; }
          else {
            v.type = JVal::Num;
            char* ep{};
            v.n = std::strtod(p_, &ep);
            p_  = ep;
        }
        return v;
    }

public:
    explicit JsonParser(const std::string& s)
        : p_(s.data()), end_(s.data() + s.size()) {}
    JVal parse() { return parse_value(); }
};

// Recursively collect all nodes whose Value field contains "°C".
void collect_temps(const JVal& node, std::map<std::string, float>& out) {
    if (node.type != JVal::Obj) return;

    const JVal* text_v  = node.get("Text");
    const JVal* value_v = node.get("Value");

    if (text_v  && text_v->type  == JVal::Str &&
        value_v && value_v->type == JVal::Str) {
        // LHM uses UTF-8 degree sign (0xC2 0xB0) followed by 'C'
        const std::string degree_c = "\xc2\xb0""C";
        if (value_v->s.find(degree_c) != std::string::npos) {
            try {
                // LHM may use a comma as decimal separator depending on the
                // system locale (e.g. "41,8 °C"). Normalise to period first.
                std::string num_str = value_v->s;
                for (char& c : num_str)
                    if (c == ',') c = '.';
                float temp = std::stof(num_str);
                out[text_v->s] = temp;
            } catch (...) {}
        }
    }

    const JVal* children = node.get("Children");
    if (children && children->type == JVal::Arr) {
        for (const auto& child : children->arr)
            collect_temps(child, out);
    }
}

// HTTP GET via WinHTTP. Prints detailed errors to stderr. Returns body or "".
std::string winhttp_get(const std::wstring& host, INTERNET_PORT port,
                        const wchar_t* path) {
    HINTERNET hSession = WinHttpOpen(
        L"desktop_temp_notif/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::cerr << "WinHttpOpen failed: " << GetLastError() << "\n";
        return {};
    }

    HINTERNET hConn = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConn) {
        std::cerr << "WinHttpConnect failed: " << GetLastError()
                  << " (is LHM web server running?)\n";
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hReq = WinHttpOpenRequest(
        hConn, L"GET", path,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq) {
        std::cerr << "WinHttpOpenRequest failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return {};
    }

    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        std::cerr << "WinHttpSendRequest failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return {};
    }

    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        std::cerr << "WinHttpReceiveResponse failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return {};
    }

    // Check HTTP status code
    DWORD status = 0, statusSize = sizeof(status);
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        std::cerr << "LHM HTTP server returned status " << status << "\n";
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return {};
    }

    std::string body;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::string buf(avail, '\0');
        DWORD read = 0;
        WinHttpReadData(hReq, buf.data(), avail, &read);
        body.append(buf.data(), read);
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
    return body;
}

// Narrow UTF-8 -> wide string for WinHTTP host parameter.
std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring r(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, r.data(), n);
    return r;
}

} // namespace

// ---------------------------------------------------------------------------

WindowsSensorReader::WindowsSensorReader(const char* host, unsigned short port)
    : host_(host), port_(port) {}

std::map<std::string, float> WindowsSensorReader::read() {
    std::map<std::string, float> result;

    std::cerr << "Fetching http://" << host_ << ":" << port_ << "/data.json ...\n";
    std::string body = winhttp_get(to_wide(host_), port_, L"/data.json");
    if (body.empty()) {
        std::cerr << "  ERROR: empty response — ensure LHM is running as Administrator\n"
                  << "  and Options > Remote Web Server > Run is enabled.\n"
                  << "  Verify manually: http://" << host_ << ":" << port_ << "/data.json\n";
        return result;
    }

    std::cerr << "  OK: received " << body.size() << " bytes\n";

    JsonParser parser(body);
    JVal root = parser.parse();
    collect_temps(root, result);

    if (result.empty())
        std::cerr << "  WARNING: JSON parsed but no temperature sensors found.\n"
                  << "  Check that LHM is detecting hardware sensors.\n";

    return result;
}
