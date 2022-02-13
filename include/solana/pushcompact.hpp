#pragma once

namespace sol {
    class PushCompact {
    public:
        static void pushCompactVecU8(const std::vector <uint8_t> &vec, std::vector <uint8_t> &buffer) {
            PushCompact::pushCompactU16(vec.size(), buffer);
            buffer.insert(buffer.end(), vec.begin(), vec.end());
        }

        static void pushCompactU16(uint16_t num, std::vector <uint8_t> &buffer) {
            buffer.push_back(num & 0x7f);
            num >>= 7;
            if (num == 0)
                return;

            buffer.back() |= 0x80;
            buffer.push_back(num & 0x7f);
            num >>= 7;
            if (num == 0)
                return;

            buffer.back() |= 0x80;
            buffer.push_back(num & 0x3);
        };
    };
}