#pragma once

#include <cstdint>

namespace Icons {
    // --- Public Interface: The Codes ---
    // These are the codes you will use in your application logic.
    // They are explicitly typed, scoped, and evaluated at compile time.
    constexpr uint8_t RAIN_CODE = 1;
    constexpr uint8_t SUN_CODE  = 2;
    // Add more icon codes here...
    // constexpr uint8_t SNOW_CODE = 2;


    // --- Implementation Detail: The Icon Data ---
    // The `inline` keyword ensures only one copy of this data exists in the final binary.
    inline constexpr uint8_t RAIN_DATA[] = {
      8,
      0x20, 0x44, 0x88, 0x82, 0x84, 0x88, 0x40, 0x20
    };

    inline constexpr uint8_t SUN_DATA[] = {
      8,
      0x18, 0x3c, 0x7e, 0xff, 0xff, 0x7e, 0x3c, 0x18
    };
}