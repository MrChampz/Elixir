#pragma once

#include <string>

namespace Elixir::UTF8
{
    /**
     * Converts a UTF-8 encoded character at the specified byte index to its Unicode codepoint.
     * @param str UTF-8 encoded string
     * @param index Byte index in the string where the UTF-8 character starts
     * @return Unicode codepoint of the character, or 0 if invalid
     */
    inline uint32_t UTF8ToCodepoint(const std::string& str, const int index)
    {
        const uint8_t c = str[index];

        if ((c & 0x80) == 0x00)  // 1 byte
            return c;

        if ((c & 0xE0) == 0xC0)  // 2 bytes
            return ((c & 0x1F) << 6)
                 | (str[index + 1] & 0x3F);

        if ((c & 0xF0) == 0xE0)  // 3 bytes
            return ((c & 0x0F) << 12)
                 | ((str[index + 1] & 0x3F) << 6)
                 | ( str[index + 2] & 0x3F);

        if ((c & 0xF8) == 0xF0)  // 4 bytes
            return ((c & 0x07) << 18)
                 | ((str[index + 1] & 0x3F) << 12)
                 | ((str[index + 2] & 0x3F) << 6)
                 |  (str[index + 3] & 0x3F);

        return 0;
    }

    /**
     * Converts a Unicode codepoint to its UTF-8 encoded string representation.
     * @param codepoint Unicode codepoint to convert to UTF-8
     * @return UTF-8 encoded string representing the codepoint, or empty string if invalid
     */
    inline std::string CodepointToUTF8(const uint32_t codepoint)
    {
        std::string result;

        if (codepoint < 0x80)
        {
            // 1 byte: 0xxxxxxx
            result += (char)codepoint;
        }
        else if (codepoint < 0x800)
        {
            // 2 bytes: 110xxxxx 10xxxxxx
            result += (char)(0xC0 | (codepoint >> 6));
            result += (char)(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint < 0x10000)
        {
            // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
            result += (char)(0xE0 |  (codepoint >> 12));
            result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
            result += (char)(0x80 |  (codepoint & 0x3F));
        }
        else if (codepoint < 0x110000)
        {
            // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            result += (char)(0xF0 |  (codepoint >> 18));
            result += (char)(0x80 | ((codepoint >> 12) & 0x3F));
            result += (char)(0x80 | ((codepoint >> 6)  & 0x3F));
            result += (char)(0x80 |  (codepoint & 0x3F));
        }

        return result;
    }

    /**
     * Determines the byte length of a UTF-8 character based on its first byte.
     * @param c First byte of a UTF-8 character
     * @return Byte length of the UTF-8 character (1 to 4)
     */
    inline int UTF8CharLength(const char c)
    {
        if ((c & 0x80) == 0x00) return 1;  // 0xxxxxxx
        if ((c & 0xE0) == 0xC0) return 2;  // 110xxxxx
        if ((c & 0xF0) == 0xE0) return 3;  // 1110xxxx
        if ((c & 0xF8) == 0xF0) return 4;  // 11110xxx
        return 1;                          // fallback
    }

    /**
     * Calculates the byte length of the previous UTF-8 character in a string given a byte
     * index.
     * @param str UTF-8 encoded string
     * @param index Byte index in the string (should be greater than 0)
     * @return Byte length of the previous UTF-8 character
     */
    inline int UTF8PrevCharLength(const std::string& str, int index)
    {
        int len = 0;

        do {
            index--;
            len++;
        } while (index > 0 && (str[index] & 0xC0) == 0x80);  // skip 10xxxxxx bytes

        return len;
    }
}