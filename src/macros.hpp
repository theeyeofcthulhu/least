#ifndef MACROS_H_
#define MACROS_H_

#define DBG(x) std::cout << #x << ": " << (x) << '\n';

#define GREEN_ARG(x) fmt::format(fmt::fg(fmt::color::pale_green), (x))
#define RED_ARG(x) fmt::format(fmt::fg(fmt::color::pale_violet_red), (x))

#define ECHO_CMD(...) fmt::print("[CMD] {}\n", fmt::format(__VA_ARGS__))
#define RUN_CMD(x, ...) std::system(fmt::format((x), __VA_ARGS__).c_str())

#endif // MACROS_H_
