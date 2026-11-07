//
// Created by User on 07.11.2026.
//
#pragma once

#include <vector>
#include <string>

#include "sha256.h"
#include "md5.h"

inline std::string calcHash256(const std::vector<unsigned char>& buffer) {
        SHA256 sha256;
        return sha256(buffer.data(), buffer.size());
}

inline std::string calcHashMD5(const std::vector<unsigned char>& buffer) {
        MD5 md5;
        return md5(buffer.data(), buffer.size());
}