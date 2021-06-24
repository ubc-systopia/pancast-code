// Removes defines to correct IntelliSense for cross-compilation
// Adapted from Simon Wijk
// https://gitlab.endian.se/simon/vscode-zephyr-intro

#ifndef C_CPP_UNDEF_H__
#define C_CPP_UNDEF_H__

#undef __x86_64
#undef __i386
#undef __APPLE__
#undef __unix__
#undef __unix
#undef unix
#undef _WIN32

#endif /* C_CPP_UNDEF_H__ */