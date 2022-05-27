#pragma once

#include <cctype>

static const std::array<uint8_t, 256> base64_index{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63, 52, 53,
    54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63, 0,  26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

std::vector<uint8_t> base64_decode(const std::string& encoded_data) {
    const auto pad =
        !encoded_data.empty() && (encoded_data.size() % 4 > 0 || encoded_data[encoded_data.size() - 1] == '=');
    const auto unpadded_size = ((encoded_data.size() + 3) / 4 - pad) * 4;
    std::vector<uint8_t> result(unpadded_size / 4 * 3 + pad);
    for (std::size_t input_index = 0, output_index = 0; input_index < unpadded_size; input_index += 4) {
        const uint32_t block =
            ((base64_index[encoded_data[input_index]] << 18) | (base64_index[encoded_data[input_index + 1]] << 12)
             | (base64_index[encoded_data[input_index + 2]] << 6) | (base64_index[encoded_data[input_index + 3]]));
        result[output_index++] = block >> 16;
        result[output_index++] = (block >> 8) & 0xFF;
        result[output_index++] = block & 0xFF;
    }
    if (pad) {
        uint32_t block =
            (base64_index[encoded_data[unpadded_size]] << 18) | (base64_index[encoded_data[unpadded_size + 1]] << 12);
        result[result.size() - 1] = block >> 16;
        if (encoded_data.size() > unpadded_size + 2 && encoded_data[unpadded_size + 2] != '=') {
            block |= (base64_index[encoded_data[unpadded_size + 2]] << 6);
            result.push_back((block >> 8) & 0xFF);
        }
    }
    return result;
}

const auto monaco_bytes = base64_decode(
#include "../third_party/monaco.ttf.base64.hpp"
);
