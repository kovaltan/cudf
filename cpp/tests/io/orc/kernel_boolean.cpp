﻿/*
 * Copyright (c) 2018, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// unit_test.cpp : Defines the entry point for the console application.
//

#include "tests_common.h"
#include "io/orc/kernel_orc.cuh"
#include "io/orc/orc_util.hpp"

#define UNREFERENCED_VALUE 255

bool Do_test_cuda_boolean_RLE_depends(orc_byte* expected, int count, orc_byte* raw, int raw_count,
    const orc_bitmap* present = NULL, size_t present_size = 0, OrcBufferArray* array = NULL)
{
    OrcKernelParameterHelper<orc_byte> helper;
    helper.setUnrefValue(UNREFERENCED_VALUE);
    KernelParamCommon* param = helper.create(expected, count, raw, raw_count, present, present_size, array);

    cudaDecodeBooleanRLE(param);

    return helper.validate();
}

bool Do_test_cuda_boolean_RLE(orc_byte* expected, int count, orc_byte* raw, int raw_count)
{
    bool succeeded = true;

    succeeded = Do_test_cuda_boolean_RLE_depends(expected, count, raw, raw_count);


    size_t present_length;
    const orc_bitmap* present = gen_present(present_length, count);
    size_t expected_sparse_length = present_length * 8;
    orc_byte* expected_sparse = new orc_byte[present_length * 8];

    int the_length = present_encode<orc_byte>(expected_sparse, expected, count, present, present_length, UNREFERENCED_VALUE);
    assert(the_length <= expected_sparse_length);

    succeeded &= Do_test_cuda_boolean_RLE_depends(expected_sparse, the_length, raw, raw_count, present, present_length);

    delete[] expected_sparse;
    free((void*)present);
    return succeeded;
}

// --------------------------------------------------------------------------------------------

void encode_boolean_run(orc_byte* expected, orc_byte* raw, int& expected_count, int& raw_count,
    orc_bitmap value, int length)
{
    assert(length < 128 + 3 && length > 2);

    // write header
    raw[raw_count] = (length - 3);
    raw_count++;

    // wirte run data
    raw[raw_count] = value;
    raw_count++;

    orc_byte values[8];
    for (int k = 0; k < 8; k++) {
        values[k] = ((value >> (7-k)) & 0x01 );
    }

    // fill expected table
    for (int i = 0; i < length; i++) {
        for (int k = 0; k < 8; k++) {
            expected[expected_count + i*8 +k] = values[k];
        }
    }
    expected_count += length *8;
}


TEST(boolean_RLE, boolean_run)
{
    const int count = 50;
    const int max_expected_count = 1500;

    orc_byte raw[count];
    orc_byte* expected = new orc_byte[max_expected_count];

    int expected_count = 0;
    int raw_count = 0;

    encode_boolean_run(expected, raw, expected_count, raw_count, 0xcf, 3);
    encode_boolean_run(expected, raw, expected_count, raw_count, 0xa7, 127 + 3);
    encode_boolean_run(expected, raw, expected_count, raw_count, 0x59, 20);
    encode_boolean_run(expected, raw, expected_count, raw_count, 0x35, 9);

    assert(max_expected_count > expected_count);
    assert(count > raw_count);

    Do_test_cuda_boolean_RLE(expected, expected_count, raw, raw_count);

    delete expected;
}

#if 1
void encode_bitmap_literals(orc_bitmap* expected, orc_byte* raw, int& expected_count, int& raw_count, int length)
{
    assert(length <= 128 && length > 0);

    // generate random source
    orc_bitmap* random_src = new orc_bitmap[length];
    set_random(random_src, length, (orc_bitmap)0x00, (orc_bitmap)0xff);    // expected is filled by random numbers

    // write header
    raw[raw_count] = orc_bitmap(-length);
    raw_count++;

    // wirte literal data
    for (int i = 0; i < length; i++) {
        for (int k = 0; k < 8; k++) {
            expected[expected_count + k] = isValidPresent(random_src + i, k) ? 0x01 : 0x00;
        }

        raw[raw_count] = getBitOrderFlip(random_src[i]);
        raw_count++;
        expected_count += 8;
    }

    delete random_src;
}

TEST(boolean_RLE, boolean_literal)
{
    const int count = 500;

    orc_bitmap* expected = new orc_bitmap[count * 8 ];
    orc_byte raw[count + 10];

    int expected_count = 0;
    int raw_count = 0;
    int start_offset = 0;

    // do encode
    encode_bitmap_literals(expected, raw, expected_count, raw_count, 1);
    encode_bitmap_literals(expected, raw, expected_count, raw_count, 20);
    encode_bitmap_literals(expected, raw, expected_count, raw_count, 128);
    encode_bitmap_literals(expected, raw, expected_count, raw_count, 32);


    assert(count * 8 > expected_count);
    assert(count > raw_count);

    Do_test_cuda_boolean_RLE(expected, expected_count, raw, raw_count);

    delete expected;
}

#endif