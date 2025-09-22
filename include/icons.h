#pragma once

#include <cstdint>

namespace Icons {
    constexpr uint8_t RAIN_CODE = 1;
    constexpr uint8_t SUN_CODE  = 2;
    constexpr uint8_t WIDE_COLON_CODE  = ';';
    constexpr uint8_t DEG_C_CODE  = '&';

    inline constexpr uint8_t DEG_C_DATA[] = {
      6, 
      2, 5, 2, 62, 65, 65,
    };

    inline constexpr uint8_t WIDE_COLON_DATA[] = {
      5, 
      0, 0, 108, 0, 0,
    };

    inline constexpr uint8_t RAIN_DATA[] = {
      8,
      0x20, 0x44, 0x88, 0x82, 0x84, 0x88, 0x40, 0x20
    };

    inline constexpr uint8_t SUN_DATA[] = {
      8,
      0x18, 0x3c, 0x7e, 0xff, 0xff, 0x7e, 0x3c, 0x18
    };
}