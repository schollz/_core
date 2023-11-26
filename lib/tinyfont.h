// BSD 3-Clause License

// Copyright (c) 2017, Botond Kis
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.

// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.

// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// https://github.com/BotiKis/Arduboy-TinyFont

#include <stdint.h>

const uint8_t tinyfont_glyphs[192] = {
    // #32 & #33 - Symbol ' ' (space) & Symbol '!'.
    0x00,  //  B00000000 → ! ░░░░   ░░░░
    0xB0,  //  B10110000 →   ▓░▓▓   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #34 & #35 - Symbol '"' & Symbol '#'.
    0xA1,  //  B10100001 → # ▓░▓░ " ░░░▓
    0x70,  //  B01110000 →   ░▓▓▓   ░░░░
    0xE1,  //  B11100001 →   ▓▓▓░   ░░░▓
    0x50,  //  B01010000 →   ░▓░▓   ░░░░

    // #36 & #37 - Symbol '$' & Symbol '%'.
    0x96,  //  B10010110 → % ▓░░▓ $ ░▓▓░
    0x4F,  //  B01001111 →   ░▓░░   ▓▓▓▓
    0x26,  //  B00100110 →   ░░▓░   ░▓▓░
    0x90,  //  B10010000 →   ▓░░▓   ░░░░

    // #38 & #39 - Symbol '&' & Symbol '''.
    0x0F,  //  B00001111 → ' ░░░░ & ▓▓▓▓
    0x1D,  //  B00011101 →   ░░░▓   ▓▓░▓
    0x07,  //  B00000111 →   ░░░░   ░▓▓▓
    0x0C,  //  B00001100 →   ░░░░   ▓▓░░

    // #40 & #41 - Symbol '(' & Symbol ')'.
    0x00,  //  B00000000 → ) ░░░░ ( ░░░░
    0x96,  //  B10010110 →   ▓░░▓   ░▓▓░
    0x69,  //  B01101001 →   ░▓▓░   ▓░░▓
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #42 & #43 - Symbol '*' & Symbol '+'.
    0x4A,  //  B01001010 → + ░▓░░ * ▓░▓░
    0xE4,  //  B11100100 →   ▓▓▓░   ░▓░░
    0x4A,  //  B01001010 →   ░▓░░   ▓░▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #44 & #45 - Symbol ',' & Symbol '-'.
    0x48,  //  B01001000 → - ░▓░░ , ▓░░░
    0x44,  //  B01000100 →   ░▓░░   ░▓░░
    0x40,  //  B01000000 →   ░▓░░   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #46 & #47 - Symbol '.' & Symbol '/'.
    0x80,  //  B10000000 → / ▓░░░   ░░░░
    0x68,  //  B01101000 →   ░▓▓░   ▓░░░
    0x10,  //  B00010000 →   ░░░▓   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #48 & #49 - Number '0' & Number '1'.
    0x0F,  //  B00001111 → 1 ░░░░ 0 ▓▓▓▓
    0x99,  //  B10011001 →   ▓░░▓   ▓░░▓
    0xFB,  //  B11111011 →   ▓▓▓▓   ▓░▓▓
    0x8F,  //  B10001111 →   ▓░░░   ▓▓▓▓

    // #50 & #51 - Number '2' & Number '3'.
    0x9D,  //  B10011101 → 3 ▓░░▓ 2 ▓▓░▓
    0xBD,  //  B10111101 →   ▓░▓▓   ▓▓░▓
    0xBB,  //  B10111011 →   ▓░▓▓   ▓░▓▓
    0xFB,  //  B11111011 →   ▓▓▓▓   ▓░▓▓

    // #52 & #53 - Number '4' & Number '5'.
    0x77,  //  B01110111 → 5 ░▓▓▓ 4 ░▓▓▓
    0xD4,  //  B11010100 →   ▓▓░▓   ░▓░░
    0xD4,  //  B11010100 →   ▓▓░▓   ░▓░░
    0xDF,  //  B11011111 →   ▓▓░▓   ▓▓▓▓

    // #54 & #55 - Number '6' & Number '7'.
    0x1F,  //  B00011111 → 7 ░░░▓ 6 ▓▓▓▓
    0x1A,  //  B00011010 →   ░░░▓   ▓░▓░
    0x1A,  //  B00011010 →   ░░░▓   ▓░▓░
    0xFE,  //  B11111110 →   ▓▓▓▓   ▓▓▓░

    // #56 & #57 - Number '8' & Number '9'.
    0x7F,  //  B01111111 → 9 ░▓▓▓ 8 ▓▓▓▓
    0x5D,  //  B01011101 →   ░▓░▓   ▓▓░▓
    0x5D,  //  B01011101 →   ░▓░▓   ▓▓░▓
    0xFF,  //  B11111111 →   ▓▓▓▓   ▓▓▓▓

    // #58 & #59 - Symbol ':' & Symbol ';'.
    0x80,  //  B10000000 → ; ▓░░░ : ░░░░
    0x5A,  //  B01011010 →   ░▓░▓   ▓░▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #60 & #61 - Symbol '<' & Symbol '='.
    0xA0,  //  B10100000 → = ▓░▓░ < ░░░░
    0xA4,  //  B10100100 →   ▓░▓░   ░▓░░
    0xAA,  //  B10101010 →   ▓░▓░   ▓░▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #62 & #63 - Symbol '>' & Symbol '?'.
    0x10,  //  B00010000 → ? ░░░▓ > ░░░░
    0xBA,  //  B10111010 →   ▓░▓▓   ▓░▓░
    0x34,  //  B00110100 →   ░░▓▓   ░▓░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #64 & #65 - Symbol '@' & Letter 'A'.
    0xFF,  //  B11111111 → A ▓▓▓▓ @ ▓▓▓▓
    0x59,  //  B01011001 →   ░▓░▓   ▓░░▓
    0x53,  //  B01010011 →   ░▓░▓   ░░▓▓
    0xF3,  //  B11110011 →   ▓▓▓▓   ░░▓▓

    // #66 & #67 - Letter 'B' & Letter 'C'.
    0xFF,  //  B11111111 → C ▓▓▓▓ B ▓▓▓▓
    0x9B,  //  B10011011 →   ▓░░▓   ▓░▓▓
    0x9B,  //  B10011011 →   ▓░░▓   ▓░▓▓
    0x9E,  //  B10011110 →   ▓░░▓   ▓▓▓░

    // #68 & #69 - Letter 'D' & Letter 'E'.
    0xFF,  //  B11111111 → E ▓▓▓▓ D ▓▓▓▓
    0xB9,  //  B10111001 →   ▓░▓▓   ▓░░▓
    0xB9,  //  B10111001 →   ▓░▓▓   ▓░░▓
    0x96,  //  B10010110 →   ▓░░▓   ░▓▓░

    // #70 & #71 - Letter 'F' & Letter 'G'.
    0xFF,  //  B11111111 → G ▓▓▓▓ F ▓▓▓▓
    0x95,  //  B10010101 →   ▓░░▓   ░▓░▓
    0x95,  //  B10010101 →   ▓░░▓   ░▓░▓
    0xD1,  //  B11010001 →   ▓▓░▓   ░░░▓

    // #72 & #73 - Letter 'H' & Letter 'I'.
    0x9F,  //  B10011111 → I ▓░░▓ H ▓▓▓▓
    0xF4,  //  B11110100 →   ▓▓▓▓   ░▓░░
    0x94,  //  B10010100 →   ▓░░▓   ░▓░░
    0x0F,  //  B00001111 →   ░░░░   ▓▓▓▓

    // #74 & #75 - Letter 'J' & Letter 'K'.
    0xFC,  //  B11111100 → K ▓▓▓▓ J ▓▓░░
    0x29,  //  B00101001 →   ░░▓░   ▓░░▓
    0x5F,  //  B01011111 →   ░▓░▓   ▓▓▓▓
    0x91,  //  B10010001 →   ▓░░▓   ░░░▓

    // #76 & #77 - Letter 'L' & Letter 'M'.
    0xFF,  //  B11111111 → M ▓▓▓▓ L ▓▓▓▓
    0x18,  //  B00011000 →   ░░░▓   ▓░░░
    0x38,  //  B00111000 →   ░░▓▓   ▓░░░
    0xF8,  //  B11111000 →   ▓▓▓▓   ▓░░░

    // #78 & #79 - Letter 'N' & Letter 'O'.
    0xFF,  //  B11111111 → O ▓▓▓▓ N ▓▓▓▓
    0x92,  //  B10010010 →   ▓░░▓   ░░▓░
    0x94,  //  B10010100 →   ▓░░▓   ░▓░░
    0xFF,  //  B11111111 →   ▓▓▓▓   ▓▓▓▓

    // #80 & #81 - Letter 'P' & Letter 'Q'.
    0xFF,  //  B11111111 → Q ▓▓▓▓ P ▓▓▓▓
    0x95,  //  B10010101 →   ▓░░▓   ░▓░▓
    0xD5,  //  B11010101 →   ▓▓░▓   ░▓░▓
    0xF7,  //  B11110111 →   ▓▓▓▓   ░▓▓▓

    // #82 & #83 - Letter 'R' & Letter 'S'.
    0xBF,  //  B10111111 → S ▓░▓▓ R ▓▓▓▓
    0xB5,  //  B10110101 →   ▓░▓▓   ░▓░▓
    0xDD,  //  B11011101 →   ▓▓░▓   ▓▓░▓
    0xD7,  //  B11010111 →   ▓▓░▓   ░▓▓▓

    // #84 & #85 - Letter 'T' & Letter 'U'.
    0xF1,  //  B11110001 → U ▓▓▓▓ T ░░░▓
    0x8F,  //  B10001111 →   ▓░░░   ▓▓▓▓
    0x81,  //  B10000001 →   ▓░░░   ░░░▓
    0xF1,  //  B11110001 →   ▓▓▓▓   ░░░▓

    // #86 & #87 - Letter 'V' & Letter 'W'.
    0xF7,  //  B11110111 → W ▓▓▓▓ V ░▓▓▓
    0x88,  //  B10001000 →   ▓░░░   ▓░░░
    0xC8,  //  B11001000 →   ▓▓░░   ▓░░░
    0xF7,  //  B11110111 →   ▓▓▓▓   ░▓▓▓

    // #88 & #89 - Letter 'X' & Letter 'Y'.
    0x79,  //  B01111001 → Y ░▓▓▓ X ▓░░▓
    0xC6,  //  B11000110 →   ▓▓░░   ░▓▓░
    0x46,  //  B01000110 →   ░▓░░   ░▓▓░
    0x79,  //  B01111001 →   ░▓▓▓   ▓░░▓

    // #90 & #91 - Letter 'Z' & Symbol '['.
    0x09,  //  B00001001 → [ ░░░░ Z ▓░░▓
    0xFD,  //  B11111101 →   ▓▓▓▓   ▓▓░▓
    0x9B,  //  B10011011 →   ▓░░▓   ▓░▓▓
    0x09,  //  B00001001 →   ░░░░   ▓░░▓

    // #92 & #93 - Symbol '\' & Symbol ']'.
    0x01,  //  B00000001 → ] ░░░░ \ ░░░▓
    0x96,  //  B10010110 →   ▓░░▓   ░▓▓░
    0xF8,  //  B11111000 →   ▓▓▓▓   ▓░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #94 & #95 - Symbol '^' & Symbol '_'.
    0x82,  //  B10000010 → _ ▓░░░ ^ ░░▓░
    0x81,  //  B10000001 →   ▓░░░   ░░░▓
    0x82,  //  B10000010 →   ▓░░░   ░░▓░
    0x80,  //  B10000000 →   ▓░░░   ░░░░

    // #96 & #97 - Symbol '`' & Letter 'a'.
    0x50,  //  B01010000 → a ░▓░▓ ` ░░░░
    0x71,  //  B01110001 →   ░▓▓▓   ░░░▓
    0x62,  //  B01100010 →   ░▓▓░   ░░▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #98 & #99 - Letter 'b' & Letter 'c'.
    0x77,  //  B01110111 → c ░▓▓▓ b ░▓▓▓
    0x56,  //  B01010110 →   ░▓░▓   ░▓▓░
    0x56,  //  B01010110 →   ░▓░▓   ░▓▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #100 & #101 - Letter 'd' & Letter 'e'.
    0x76,  //  B01110110 → e ░▓▓▓ d ░▓▓░
    0x76,  //  B01110110 →   ░▓▓▓   ░▓▓░
    0x37,  //  B00110111 →   ░░▓▓   ░▓▓▓
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #102 & #103 - Letter 'f' & Letter 'g'.
    0xA2,  //  B10100010 → g ▓░▓░ f ░░▓░
    0xB7,  //  B10110111 →   ▓░▓▓   ░▓▓▓
    0x73,  //  B01110011 →   ░▓▓▓   ░░▓▓
    0x01,  //  B00000001 →   ░░░░   ░░░▓

    // #104 & #105 - Letter 'h' & Letter 'i'.
    0x07,  //  B00000111 → i ░░░░ h ░▓▓▓
    0x72,  //  B01110010 →   ░▓▓▓   ░░▓░
    0x06,  //  B00000110 →   ░░░░   ░▓▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #106 & #107 - Letter 'j' & Letter 'k'.
    0x78,  //  B01111000 → k ░▓▓▓ j ▓░░░
    0x27,  //  B00100111 →   ░░▓░   ░▓▓▓
    0x50,  //  B01010000 →   ░▓░▓   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #108 & #109 - Letter 'l' & Letter 'm'.
    0x73,  //  B01110011 → m ░▓▓▓ l ░░▓▓
    0x34,  //  B00110100 →   ░░▓▓   ░▓░░
    0x74,  //  B01110100 →   ░▓▓▓   ░▓░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #110 & #111 - Letter 'n' & Letter 'o'.
    0x77,  //  B01110111 → o ░▓▓▓ n ░▓▓▓
    0x51,  //  B01010001 →   ░▓░▓   ░░░▓
    0x76,  //  B01110110 →   ░▓▓▓   ░▓▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #112 & #113 - Letter 'p' & Letter 'q'.
    0x7F,  //  B01111111 → q ░▓▓▓ p ▓▓▓▓
    0x55,  //  B01010101 →   ░▓░▓   ░▓░▓
    0xF7,  //  B11110111 →   ▓▓▓▓   ░▓▓▓
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #114 & #115 - Letter 'r' & Letter 's'.
    0x47,  //  B01000111 → s ░▓░░ r ░▓▓▓
    0x71,  //  B01110001 →   ░▓▓▓   ░░░▓
    0x10,  //  B00010000 →   ░░░▓   ░░░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #116 & #117 - Letter 't' & Letter 'u'.
    0x32,  //  B00110010 → u ░░▓▓ t ░░▓░
    0x47,  //  B01000111 →   ░▓░░   ░▓▓▓
    0x72,  //  B01110010 →   ░▓▓▓   ░░▓░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #118 & #119 - Letter 'v' & Letter 'w'.
    0x73,  //  B01110011 → w ░▓▓▓ v ░░▓▓
    0x64,  //  B01100100 →   ░▓▓░   ░▓░░
    0x73,  //  B01110011 →   ░▓▓▓   ░░▓▓
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #120 & #121 - Letter 'x' & Letter 'y'.
    0x15,  //  B00010101 → y ░░░▓ x ░▓░▓
    0xA2,  //  B10100010 →   ▓░▓░   ░░▓░
    0x75,  //  B01110101 →   ░▓▓▓   ░▓░▓
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #122 & #123 - Letter 'z' & Symbol '{'.
    0x61,  //  B01100001 → { ░▓▓░ z ░░░▓
    0x67,  //  B01100111 →   ░▓▓░   ░▓▓▓
    0x94,  //  B10010100 →   ▓░░▓   ░▓░░
    0x00,  //  B00000000 →   ░░░░   ░░░░

    // #124 & #125 - Symbol '|' & Symbol '}'.
    0x00,  //  B00000000 → } ░░░░ | ░░░░
    0x9F,  //  B10011111 →   ▓░░▓   ▓▓▓▓
    0x60,  //  B01100000 →   ░▓▓░   ░░░░
    0x60,  //  B01100000 →   ░▓▓░   ░░░░

    // #126 & #127 - Symbol '~' & Symbol '■'.
    0xF4,  //  B11110100 → ■ ▓▓▓▓ ~ ░▓░░
    0xF2,  //  B11110010 →   ▓▓▓▓   ░░▓░
    0xF6,  //  B11110110 →   ▓▓▓▓   ░▓▓░
    0xF2   //  B11110010 →   ▓▓▓▓   ░░▓░
};
