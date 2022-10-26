#ifndef MACROS_H_
#define MACROS_H_

#define DBG(x) std::cout << #x << ": " << (x) << '\n';

#define GREEN_ARG(x) fmt::format(fmt::fg(fmt::color::pale_green), (x))
#define RED_ARG(x) fmt::format(fmt::fg(fmt::color::pale_violet_red), (x))

#define COLOR_CMD(...) fmt::format("[CMD] {}\n", fmt::format(__VA_ARGS__))
#define RUN_CMD(x, ...) std::system(fmt::format((x), __VA_ARGS__).c_str())

#define UNREACHABLE() assert(false && "unreachable")

// Does container v have element x
#define HAS(v, x) (std::find((v).begin(), (v).end(), (x)) != v.end())

// For debugging with valgrind
#define SEGFAULT() *(int *) 0x0 = 0;

#endif // MACROS_H_
