//
// Copyright (c) 2017 Regents of the SIGNET lab, University of Padova.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Padova (SIGNET lab) nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

/**
 * @file   string_utils.h
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides the TcpListener class
 *
 */

#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H

#include <string>
#include <sstream>
#include <iomanip>

/** Convert the given value into ist hexadecimal representation */
template <typename T>
std::string to_hexstring(const T &in, bool withSpace = false, bool withLeadingX = false)
{
    bool upper_case = true;
    std::stringstream ss;
    if (withLeadingX)
        ss << "0x";

    ss << std::hex << std::setfill('0') << std::setw(2) << (upper_case ? std::uppercase : std::nouppercase) << (int)in;

    //ss << std::hex << in;
    if (withSpace)
        ss << " ";
    return ss.str();
}

/** Convert the given string into ist hexadecimal representation (may be binary) */
inline std::string string_to_hexstring(const std::string &in, bool withSpace = false, bool withLeadingX = false)
{
    std::stringstream ss;
    for (auto c : in)
        ss << to_hexstring(c, withSpace, withLeadingX);
    return ss.str();
}

/** Convert the given char array into ist hexadecimal representation (may be binary) */
inline std::string buffer_to_hexstring(const char *in, int size, bool withSpace = false, bool withLeadingX = false)
{
    std::stringstream ss;
    for (auto i = 0; i < size; i++)
        ss << to_hexstring(in[i], withSpace, withLeadingX);
    return ss.str();
}

#endif // __STRING_UTILS_H
