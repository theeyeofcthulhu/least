#ifndef MACROS_H_
#define MACROS_H_

#define DBG(x) std::cout << #x << ": " << (x) << '\n';

#define GREEN_ARG(x) fmt::format(fmt::fg(fmt::color::pale_green), (x))
#define RED_ARG(x) fmt::format(fmt::fg(fmt::color::pale_violet_red), (x))

#endif // MACROS_H_
