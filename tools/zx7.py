#!/usr/bin/env python3
"""Forward ZX7 compressor compatible with the SGlib RAM/VRAM decoders.

Python translation from gfx2sg; optimizer fixes from Ultima1/tools/zx7.py.
See zx7.LICENSE.txt for the translation's license.
"""
'''
 * (c) Copyright 2012-2016 by Einar Saukas. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of its author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 '''

from dataclasses import dataclass
from functools import cache

MAX_OFFSET = 2176
MAX_LEN = 256**2
@dataclass
class State:
    output_data: list
    bitmask: int = 0
    bit_index: int = 0
    output_index: int = 0


def write_byte(state, byte):
    state.output_data[state.output_index] = byte
    state.output_index += 1

def write_bit(state, bit):
    if state.bitmask == 0:
        state.bitmask = 0x80
        state.bit_index = state.output_index
        write_byte(state, 0)
    if bit > 0:
        state.output_data[state.bit_index] |= state.bitmask

    state.bitmask >>= 1

def encode_sequence_length(state, value):
    i = 2

    while i <= value:
        write_bit(state, 0)
        i <<= 1

    while (i := i >> 1) > 0:
        write_bit(state, value & i)

def compress_data(input_data, optimal):
    # first character always uncompressed as literal
    state = State([], 0, 0, 0)
    opt_bits, opt_offset, opt_len = optimal

    input_size = len(input_data)
    input_index = input_size - 1
    output_size = (opt_bits[input_index] + 18 + 7) // 8
    state.output_data = bytearray(output_size)

    # un-reverse optimal sequence
    opt_bits[input_index] = 0
    while input_index != 0:
        input_prev = input_index - (opt_len[input_index] if opt_len[input_index] > 0 else 1)
        opt_bits[input_prev] = input_index
        input_index = input_prev

    # first byte is always literal
    write_byte(state, input_data[input_index])

    while (input_index := opt_bits[input_index]) > 0:

        if opt_len[input_index] == 0:
            # literal indicator
            write_bit(state, 0)

            # literal value
            write_byte(state, input_data[input_index])
        else:
            # sequence indicator
            write_bit(state, 1)

            # sequence length
            encode_sequence_length(state, opt_len[input_index] - 1)

            offset1 = opt_offset[input_index] - 1

            # sequence offset
            if offset1 < 0x80:
                write_byte(state, offset1)
            else:
                offset1 -= 0x80
                write_byte(state, (offset1 & 0x7F) | 0x80)
                mask = 1024
                while mask > 0x7F:
                    write_bit(state, offset1 & mask)
                    mask >>= 1

    # sequence indicator
    write_bit(state, 1)

    # end marker
    for _ in range(16):
        write_bit(state, 0)
    write_bit(state, 1)

    return state.output_data

@cache
def elias_gamma_bits(value):
    bits = 1

    while value > 1:
        bits += 2
        value >>= 1

    return bits

@cache
def count_bits(offset, length):
    return 1 + (12 if offset > 128 else 8) + elias_gamma_bits(length - 1)

def optimize(input_data):
    min_arr = [0] * (MAX_OFFSET + 1)
    max_arr = [0] * (MAX_OFFSET + 1)
    matches = [0] * 256**2
    match_slots = [0] * len(input_data)

    input_size = len(input_data)
    opt_bits   = [0] * input_size
    opt_offset = [0] * input_size
    opt_len    = [0] * input_size

    opt_bits[0] = 8
    for i in range(1, input_size):
        opt_bits[i] = opt_bits[i - 1] + 9
        match_index = (input_data[i - 1] << 8) | input_data[i]
        best_length = 1

        match = matches[match_index]
        while (match != 0) and (best_length < MAX_LEN):
            offset = i - match
            if offset > MAX_OFFSET:
                match = 0
                break

            length = 2
            while (length <= MAX_LEN) and (i >= length):
                if length > best_length:
                    best_length = length  # fix: was best_len (dead write)
                    bits = opt_bits[i - length] + count_bits(offset, length)
                    if opt_bits[i] > bits:
                        opt_bits[i]   = bits
                        opt_offset[i] = offset
                        opt_len[i]    = length
                elif (max_arr[offset] != 0) and (i + 1 == max_arr[offset] + length):  # fix: was (...) + length
                    length = i - min_arr[offset]
                    if length > best_length:
                        length = best_length

                if (i < offset + length) or (input_data[i - length] != input_data[i - length - offset]):
                    break
                length += 1

            min_arr[offset] = i + 1 - length
            max_arr[offset] = i

            match = match_slots[match]

        match_slots[i] = matches[match_index]
        matches[match_index] = i

    return opt_bits, opt_offset, opt_len



def compress(data):
    """Return a forward ZX7 stream (including its end marker)."""
    if not data:
        raise ValueError("ZX7 requires at least one input byte")
    return bytes(compress_data(data, optimize(data)))
