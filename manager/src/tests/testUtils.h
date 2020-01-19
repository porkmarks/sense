#pragma once

#include "DB.h"

#define CHECK_SUCCESS(x) \
{\
    auto __result = x;\
    if (__result != success)\
    {\
        std::cout << "Failed " << __FILE__ << "@" << __LINE__ << "|" << #x << ": " << __result.error().what() << std::endl;\
        exit(1);\
    }\
}
#define CHECK_FAILURE(x) \
{\
    auto __result = x;\
    if (__result == success)\
    {\
        std::cout << "Failed " << __FILE__ << "@" << __LINE__ << "|" << #x << ": Expected failure, got success" << std::endl;\
        exit(1);\
    }\
}
#define CHECK_RESULT(x, succ)\
if (succ)\
{\
    CHECK_SUCCESS(x);\
}\
else\
{\
    CHECK_FAILURE(x);\
}
#define CHECK_TRUE(x) \
{\
    auto __result = x;\
    if (!__result)\
    {\
        std::cout << "Failed " << __FILE__ << "@" << __LINE__ << "|" << #x << ": Expected true, got false" << std::endl;\
        exit(1);\
    }\
}
#define CHECK_FALSE(x) \
{\
    auto __result = x;\
    if (__result)\
    {\
        std::cout << "Failed " << __FILE__ << "@" << __LINE__ << "|" << #x << ": Expected false, got true" << std::endl;\
        exit(1);\
    }\
}
#define CHECK_EQUALS(x, y) \
{\
    auto __x = x;\
    auto __y = y;\
    if (__x != __y)\
    {\
        std::cout << "Failed " << __FILE__ << "@" << __LINE__ << "|" << #x << " == " << #y << ": Expected " << std::to_string(__y) << ", got " << std::to_string(__x) << std::endl;\
        exit(1);\
    }\
}

void closeDB(DB& db);
void createDB(DB& db);
void loadDB(DB& db);
