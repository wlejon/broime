#pragma once

#include <chrono>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace bstest {

inline int& failures() {
    static int n = 0;
    return n;
}

inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, what.c_str());
    std::fflush(stderr);
}

template <class T>
concept HasToString = requires(const T& t) {
    { t.to_string() } -> std::convertible_to<std::string>;
};

template <class T>
concept HasOstream = requires(std::ostream& os, const T& t) {
    os << t;
};

template <class T>
auto printable(const T& v) {
    if constexpr (std::is_enum_v<T>) {
        return static_cast<long long>(v);
    } else if constexpr (std::is_pointer_v<T> && !std::is_same_v<T, const char*>) {
        return static_cast<const void*>(v);
    } else if constexpr (HasToString<T>) {
        return v.to_string();
    } else if constexpr (HasOstream<T>) {
        return v;
    } else {
        return "<object>";
    }
}

template <class A, class B>
std::string describe(const char* ea, const char* eb, const A& a, const B& b) {
    std::ostringstream s;
    s << ea << " == " << eb << " (got '" << printable(a) << "' vs '" << printable(b) << "')";
    return s.str();
}

template <class A, class B>
std::string describe_op(const char* ea, const char* op, const char* eb, const A& a, const B& b) {
    std::ostringstream s;
    s << ea << " " << op << " " << eb << " (got '" << printable(a) << "' vs '" << printable(b) << "')";
    return s.str();
}

inline int finish(const char* name) {
    if (failures() == 0) {
        std::printf("[%s] PASSED\n", name);
        std::fflush(stdout);
        return 0;
    }
    std::printf("[%s] FAILED (%d check%s)\n", name, failures(), failures() == 1 ? "" : "s");
    std::fflush(stdout);
    return 1;
}

[[noreturn]] inline void skip(const char* name, const std::string& why) {
    std::printf("[%s] SKIP: %s\n", name, why.c_str());
    std::fflush(stdout);
    std::exit(failures() == 0 ? 77 : 1);
}

} // namespace bstest

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) ::bstest::fail(__FILE__, __LINE__, #cond);        \
    } while (0)

#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!(check_a_ == check_b_))                                                   \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe(#a, #b, check_a_, check_b_));            \
    } while (0)

#define CHECK_NE(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (check_a_ == check_b_)                                                      \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe_op(#a, "!=", #b, check_a_, check_b_));   \
    } while (0)

#define CHECK_LT(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!(check_a_ < check_b_))                                                    \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe_op(#a, "<", #b, check_a_, check_b_));    \
    } while (0)

#define CHECK_LE(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!(check_a_ <= check_b_))                                                   \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe_op(#a, "<=", #b, check_a_, check_b_));   \
    } while (0)

#define CHECK_GT(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!(check_a_ > check_b_))                                                    \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe_op(#a, ">", #b, check_a_, check_b_));    \
    } while (0)

#define CHECK_GE(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!(check_a_ >= check_b_))                                                   \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe_op(#a, ">=", #b, check_a_, check_b_));   \
    } while (0)

#define REQUIRE(cond)                                                  \
    do {                                                               \
        if (!(cond)) {                                                 \
            ::bstest::fail(__FILE__, __LINE__, "required: " #cond);    \
            return 1;                                                  \
        }                                                              \
    } while (0)

#define REQUIRE_EQ(a, b)                                                               \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!(check_a_ == check_b_)) {                                                \
            ::bstest::fail(__FILE__, __LINE__,                                         \
                           ::bstest::describe(#a, #b, check_a_, check_b_));            \
            return 1;                                                                  \
        }                                                                              \
    } while (0)
