#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include "../src/bc-sskr.h"
#include "test-utils.h"

static void test1() {
    // size_t group_threshold = 1;
    // sskr_group_descriptor groups[] = {{1, 1}};
    // size_t groups_len = 1;
    // size_t input_indexes[] = {0};
    // size_t input_indexes_len = 1;

    // size_t group_threshold = 2;
    // sskr_group_descriptor groups[] = {{2, 3}, {3, 5}};
    // size_t groups_len = 2;
    // size_t input_indexes[] = {0, 2, 4, 5, 7};
    // size_t input_indexes_len = 5;

    size_t group_threshold = 2;
    sskr_group_descriptor groups[] = {{2, 3}, {3, 5}};
    size_t groups_len = 2;
    size_t input_indexes[] = {1, 2, 3, 5, 6};
    size_t input_indexes_len = 5;
    size_t expected_shard_count = sskr_count_shards(group_threshold, groups, groups_len);

    char* master_secret_str = "7daa851251002874e1a1995f0897e6b1";
    uint8_t* master_secret;
    size_t master_secret_len = hex_to_data(master_secret_str, &master_secret);
    size_t shard_len = 0;
    size_t expected_shard_len = master_secret_len + METADATA_LENGTH_BYTES;
    size_t buffer_len = 20 * expected_shard_len;
    uint8_t output[buffer_len];
    int shard_count = sskr_generate(group_threshold, groups, groups_len, master_secret, master_secret_len, &shard_len, output, buffer_len, fake_random);
    assert(shard_count == expected_shard_count);
    assert(shard_len == expected_shard_len);

    // printf("%d\n", shard_count);
    // printf("%ld\n", shard_len);
    // for(int i = 0; i < shard_count; i++) {
    //     char* s = data_to_hex(&output[i * shard_len], shard_len);
    //     printf("%s\n", s);
    //     free(s);
    // }

    const uint8_t* input[input_indexes_len];
    for(int i = 0; i < input_indexes_len; i++) {
        input[i] = &output[input_indexes[i] * shard_len];
    }
    size_t result_master_secret_max_len = 100;
    uint8_t result_master_secret[result_master_secret_max_len];
    int result_master_secret_len = sskr_combine(&input[0], shard_len, input_indexes_len, result_master_secret, result_master_secret_max_len);
    assert(result_master_secret_len == master_secret_len);
    assert(equal_uint8_buffers(master_secret, master_secret_len, result_master_secret, result_master_secret_len));

    // printf("%d\n", result_master_secret_len);
    // printf("%s\n", data_to_hex(result_master_secret, result_master_secret_len));
}

int main() {
    test1();
}
