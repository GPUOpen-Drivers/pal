/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
#pragma once

#include "palLibrary.h"
#include "palInlineFuncs.h"
#include "palSysUtil.h"
#include "palMutex.h"

#include <linux/input.h>

namespace Util
{

constexpr KeyCode KeyLookupTable[] =
{
    KeyCode::Undefined,
    KeyCode::Esc,        // KEY_ESC              = 1
    KeyCode::One,        // KEY_1                = 2
    KeyCode::Two,        // KEY_2                = 3
    KeyCode::Three,      // KEY_3                = 4
    KeyCode::Four,       // KEY_4                = 5
    KeyCode::Five,       // KEY_5                = 6
    KeyCode::Six,        // KEY_6                = 7
    KeyCode::Seven,      // KEY_7                = 8
    KeyCode::Eight,      // KEY_8                = 9
    KeyCode::Nine,       // KEY_9                = 10
    KeyCode::Zero,       // KEY_0                = 11
    KeyCode::Minus,      // KEY_MINUS            = 12
    KeyCode::Equal,      // KEY_EQUAL            = 13
    KeyCode::Backspace,  // KEY_BACKSPACE        = 14
    KeyCode::Tab,        // KEY_TAB              = 15
    KeyCode::Q,          // KEY_Q                = 16
    KeyCode::W,          // KEY_W                = 17
    KeyCode::E,          // KEY_E                = 18
    KeyCode::R,          // KEY_R                = 19
    KeyCode::T,          // KEY_T                = 20
    KeyCode::Y,          // KEY_Y                = 21
    KeyCode::U,          // KEY_U                = 22
    KeyCode::I,          // KEY_I                = 23
    KeyCode::O,          // KEY_O                = 24
    KeyCode::P,          // KEY_P                = 25
    KeyCode::LBrace,     // KEY_LEFTBRACE        = 26
    KeyCode::RBrace,     // KEY_RIGHTBRACE       = 27
    KeyCode::Enter,      // KEY_ENTER            = 28
    KeyCode::LControl,   // KEY_LEFTCTRL         = 29
    KeyCode::A,          // KEY_A                = 30
    KeyCode::S,          // KEY_S                = 31
    KeyCode::D,          // KEY_D                = 32
    KeyCode::F,          // KEY_F                = 33
    KeyCode::G,          // KEY_G                = 34
    KeyCode::H,          // KEY_H                = 35
    KeyCode::J,          // KEY_J                = 36
    KeyCode::K,          // KEY_K                = 37
    KeyCode::L,          // KEY_L                = 38
    KeyCode::Semicolon,  // KEY_SEMICOLON        = 39
    KeyCode::Apostrophe, // KEY_APOSTROPHE       = 40
    KeyCode::Backtick,   // KEY_GRAVE            = 41
    KeyCode::LShift,     // KEY_LEFTSHIFT        = 42
    KeyCode::Backslash,  // KEY_BACKSLASH        = 43
    KeyCode::Z,          // KEY_Z                = 44
    KeyCode::X,          // KEY_X                = 45
    KeyCode::C,          // KEY_C                = 46
    KeyCode::V,          // KEY_V                = 47
    KeyCode::B,          // KEY_B                = 48
    KeyCode::N,          // KEY_N                = 49
    KeyCode::M,          // KEY_M                = 50
    KeyCode::Comma,      // KEY_COMMA            = 51
    KeyCode::Dot,        // KEY_DOT              = 52
    KeyCode::Slash,      // KEY_SLASH            = 53
    KeyCode::RShift,     // KEY_RIGHTSHIFT       = 54
    KeyCode::NumAsterisk,// KEY_KPASTERISK       = 55
    KeyCode::LAlt,       // KEY_LEFTALT          = 56
    KeyCode::Space,      // KEY_SPACE            = 57
    KeyCode::Capslock,   // KEY_CAPSLOCK         = 58
    KeyCode::F1,         // KEY_F1               = 59
    KeyCode::F2,         // KEY_F2               = 60
    KeyCode::F3,         // KEY_F3               = 61
    KeyCode::F4,         // KEY_F4               = 62
    KeyCode::F5,         // KEY_F5               = 63
    KeyCode::F6,         // KEY_F6               = 64
    KeyCode::F7,         // KEY_F7               = 65
    KeyCode::F8,         // KEY_F8               = 66
    KeyCode::F9,         // KEY_F9               = 67
    KeyCode::F10,        // KEY_F10              = 68
    KeyCode::Numlock,    // KEY_NUMLOCK          = 69
    KeyCode::Scroll,     // KEY_SCROLLLOCK       = 70
    KeyCode::Num7,       // KEY_KP7              = 71
    KeyCode::Num8,       // KEY_KP8              = 72
    KeyCode::Num9,       // KEY_KP9              = 73
    KeyCode::NumMinus,   // KEY_KPMINUS          = 74
    KeyCode::Num4,       // KEY_KP4              = 75
    KeyCode::Num5,       // KEY_KP5              = 76
    KeyCode::Num6,       // KEY_KP6              = 77
    KeyCode::NumPlus,    // KEY_KPPLUS           = 78
    KeyCode::Num1,       // KEY_KP1              = 79
    KeyCode::Num2,       // KEY_KP2              = 80
    KeyCode::Num3,       // KEY_KP3              = 81
    KeyCode::Num0,       // KEY_KP0              = 82
    KeyCode::NumDot,     // KEY_KPDOT            = 83
    KeyCode::Undefined,  // 84
    KeyCode::Undefined,  // KEY_ZENKAKUHANKAKU   = 85
    KeyCode::Undefined,  // KEY_102ND            = 86
    KeyCode::F11,        // KEY_F11              = 87
    KeyCode::F12,        // KEY_F12              = 88
    KeyCode::Undefined,  // KEY_RO               = 89
    KeyCode::Undefined,  // KEY_KATAKANA         = 90
    KeyCode::Undefined,  // KEY_HIRAGANA         = 91
    KeyCode::Undefined,  // KEY_HENKAN           = 92
    KeyCode::Undefined,  // KEY_KATAKANAHIRAGANA = 93
    KeyCode::Undefined,  // KEY_MUHENKAN         = 94
    KeyCode::Undefined,  // KEY_KPJPCOMMA        = 95
    KeyCode::NumEnter,   // KEY_KPENTER          = 96
    KeyCode::RControl,   // KEY_RIGHTCTRL        = 97
    KeyCode::NumSlash,   // KEY_KPSLASH          = 98
    KeyCode::Undefined,  // KEY_SYSRQ            = 99
    KeyCode::RAlt,       // KEY_RIGHTALT         = 100
    KeyCode::Undefined,  // KEY_LINEFEED         = 101
    KeyCode::Home,       // KEY_HOME             = 102
    KeyCode::ArrowUp,    // KEY_UP               = 103
    KeyCode::PageUp,     // KEY_PAGEUP           = 104
    KeyCode::ArrowLeft,  // KEY_LEFT             = 105
    KeyCode::ArrowRight, // KEY_RIGHT            = 106
    KeyCode::End,        // KEY_END              = 107
    KeyCode::ArrowDown,  // KEY_DOWN             = 108
    KeyCode::PageDown,   // KEY_PAGEDOWN         = 109
    KeyCode::Insert,     // KEY_INSERT           = 110
    KeyCode::Delete,     // KEY_DELETE           = 111
    KeyCode::Undefined,  // 112
    KeyCode::Undefined,  // 113
    KeyCode::Undefined,  // 114
    KeyCode::Undefined,  // 115
    KeyCode::Undefined,  // 116
    KeyCode::Undefined,  // 117
    KeyCode::Undefined,  // 118
    KeyCode::Undefined,  // 119
    KeyCode::Undefined,  // 120
    KeyCode::Undefined,  // 121
    KeyCode::Undefined,  // 122
    KeyCode::Undefined,  // 123
    KeyCode::Undefined,  // 124
    KeyCode::Undefined,  // 125
    KeyCode::Undefined,  // 126
    KeyCode::Undefined,  // 127
    KeyCode::Undefined,  // 128
    KeyCode::Undefined,  // 129
    KeyCode::Undefined,  // 130
    KeyCode::Undefined,  // 131
    KeyCode::Undefined,  // 132
    KeyCode::Undefined,  // 133
    KeyCode::Undefined,  // 134
    KeyCode::Undefined,  // 135
    KeyCode::Undefined,  // 136
    KeyCode::Undefined,  // 137
    KeyCode::Undefined,  // 138
    KeyCode::Undefined,  // 139
    KeyCode::Undefined,  // 140
    KeyCode::Undefined,  // 141
    KeyCode::Undefined,  // 142
    KeyCode::Undefined,  // 143
    KeyCode::Undefined,  // 144
    KeyCode::Undefined,  // 145
    KeyCode::Undefined,  // 146
    KeyCode::Undefined,  // 147
    KeyCode::Undefined,  // 148
    KeyCode::Undefined,  // 149
    KeyCode::Undefined,  // 150
    KeyCode::Undefined,  // 151
    KeyCode::Undefined,  // 152
    KeyCode::Undefined,  // 153
    KeyCode::Undefined,  // 154
    KeyCode::Undefined,  // 155
    KeyCode::Undefined,  // 156
    KeyCode::Undefined,  // 157
    KeyCode::Undefined,  // 158
    KeyCode::Undefined,  // 159
    KeyCode::Undefined,  // 160
    KeyCode::Undefined,  // 161
    KeyCode::Undefined,  // 162
    KeyCode::Undefined,  // 163
    KeyCode::Undefined,  // 164
    KeyCode::Undefined,  // 165
    KeyCode::Undefined,  // 166
    KeyCode::Undefined,  // 167
    KeyCode::Undefined,  // 168
    KeyCode::Undefined,  // 169
    KeyCode::Undefined,  // 170
    KeyCode::Undefined,  // 171
    KeyCode::Undefined,  // 172
    KeyCode::Undefined,  // 173
    KeyCode::Undefined,  // 174
    KeyCode::Undefined,  // 175
    KeyCode::Undefined,  // 176
    KeyCode::Undefined,  // 177
    KeyCode::Undefined,  // 178
    KeyCode::Undefined,  // 179
    KeyCode::Undefined,  // 180
    KeyCode::Undefined,  // 181
    KeyCode::Undefined,  // 182
    KeyCode::F13,         // KEY_F13             = 183
    KeyCode::F14,         // KEY_F14             = 184
    KeyCode::F15,         // KEY_F15             = 185
    KeyCode::F16         // KEY_F16              = 186
};

static_assert(KeyLookupTable[KEY_BACKSLASH] == KeyCode::Backslash, "Wrong KeyLookupTable");
static_assert(KeyLookupTable[KEY_DELETE]    == KeyCode::Delete,    "Wrong KeyLookupTable");

constexpr uint32 MaxKeyboards    = 16;
constexpr uint32 MaxPathStrWidth = 128;

// =====================================================================================================================
// This class provides a thread-safe bitset for storing currently depressed keys.
class KeyBitset
{
public:
    KeyBitset()
    {
        for (uint32 i = 0; i < BITMAP_WORDS; ++i)
        {
            m_bitmap[i] = 0;
        }
    }

    bool IsSet(KeyCode key)
    {
        const uint32 wordIndex = KeyWordIndex(key);
        const uint32 bitIndex = KeyBitIndex(key);
        return TestAnyFlagSet(m_bitmap[wordIndex], 1U << bitIndex);
    }

    void Clear(KeyCode key)
    {
        const uint32 wordIndex = KeyWordIndex(key);
        const uint32 bitIndex = KeyBitIndex(key);
        m_bitmap[wordIndex] &= ~(1U << bitIndex);
    }

    void Set(KeyCode key)
    {
        const uint32 wordIndex = KeyWordIndex(key);
        const uint32 bitIndex = KeyBitIndex(key);
        m_bitmap[wordIndex] |= 1U << bitIndex;
    }

    bool Test(KeyCode key)
    {
        bool result = IsSet(key);
        if (!result)
        {
            if (key == KeyCode::Shift)
            {
                result = (IsSet(KeyCode::LShift) ||
                          IsSet(KeyCode::RShift));
            }
            else if (key == KeyCode::Control)
            {
                result = (IsSet(KeyCode::LControl) ||
                          IsSet(KeyCode::RControl));
            }
            else if (key == KeyCode::Alt)
            {
                result = (IsSet(KeyCode::LAlt) ||
                          IsSet(KeyCode::RAlt));
            }
        }
        return result;
    }

    Mutex* GetKeyBitsetLock() { return &m_keyBitsetLock; }

private:
    Mutex m_keyBitsetLock; // Serializes access to IsKeyPressed() and subsequently keyBitset

    static constexpr size_t BITMAP_WORDS =
        (static_cast<size_t>(KeyCode::Undefined) / sizeof(uint32)) + 1;
    volatile uint32 m_bitmap[BITMAP_WORDS];

    uint32 KeyWordIndex(KeyCode key)
    {
        const uint32 value = static_cast<uint32>(key);
        const uint32 wordIndex = value / (sizeof(uint32) * 8);
        return wordIndex;
    }

    uint32 KeyBitIndex(KeyCode key)
    {
        const uint32 value = static_cast<uint32>(key);
        const uint32 bitIndex = value % (sizeof(uint32) * 8);
        return bitIndex;
    }
};

} // namespace Util
