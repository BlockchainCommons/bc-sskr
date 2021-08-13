//
//  encoding.c
//
//  Copyright © 2020 by Blockchain Commons, LLC
//  Licensed under the "BSD-2-Clause Plus Patent License"
//

#include "encoding.h"
#include "shard.h"
#include "sskr-errors.h"

#if defined(ARDUINO) || defined(__EMSCRIPTEN__)
#include "bc-shamir.h"
#else
#include <bc-shamir/bc-shamir.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static size_t check_secret_length(size_t len) {
    if(len < MIN_STRENGTH_BYTES) {
        return SSKR_ERROR_SECRET_TOO_SHORT;
    }
    if(len > MAX_STRENGTH_BYTES) {
        return SSKR_ERROR_SECRET_TOO_LONG;
    }
    if(len & 1) {
        return SSKR_ERROR_SECRET_LENGTH_NOT_EVEN;
    }
    return 0;
}

static size_t serialize_shard(
    const sskr_shard *shard,
    uint8_t *destination,
    size_t destination_len) {

    if(destination_len < METADATA_LENGTH_BYTES) {
        return SSKR_ERROR_INSUFFICIENT_SPACE;
    }

    // pack the id, group and member data into 5 bytes:
    // 76543210        76543210        76543210
    //         76543210        76543210
    // ----------------====----====----====----
    // identifier: 16
    //                 group-threshold: 4
    //                     group-count: 4
    //                         group-index: 4
    //                             member-threshold: 4
    //                                 reserved (MUST be zero): 4
    //                                     member-index: 4

    uint32_t id = shard->identifier & 0xffff;
    uint32_t gt = (shard->group_threshold - 1) & 0xf;
    uint32_t gc = (shard->group_count - 1) & 0xf;
    uint32_t gi = shard->group_index & 0xf;
    uint32_t mt = (shard->member_threshold - 1) & 0xf;
    uint32_t mi = shard->member_index & 0xf;

    uint32_t id1 = id >> 8;
    uint32_t id2 = id & 0xff;

    destination[0] = id1;
    destination[1] = id2;
    destination[2] = (gt << 4) | gc;
    destination[3] = (gi << 4) | mt;
    destination[4] = mi;

    memcpy(destination + METADATA_LENGTH_BYTES, shard->value, shard->value_len);

    return shard->value_len + METADATA_LENGTH_BYTES;
}

static int deserialize_shard(
    const uint8_t *source,
    size_t source_len,
    sskr_shard *shard
) {
    if(source_len < MIN_SERIALIZED_LENGTH_BYTES) {
        return SSKR_ERROR_NOT_ENOUGH_SERILIZED_BYTES;
    }

    size_t group_threshold = (source[2] >> 4) + 1;
    size_t group_count = (source[2] & 0xf) + 1;

    if(group_threshold > group_count) {
        return SSKR_ERROR_INVALID_GROUP_THRESHOLD;
    }

    shard->identifier = ((uint16_t)source[0]) << 8 | source[1];
    shard->group_threshold = group_threshold;
    shard->group_count = group_count;
    shard->group_index = source[3] >> 4;
    shard->member_threshold = (source[3] & 0xf) + 1;
    size_t reserved = source[4] >> 4;
    if(reserved != 0) {
        return SSKR_ERROR_INVALID_RESERVED_BITS;
    }
    shard->member_index = source[4] & 0xf;
    shard->value_len = source_len - METADATA_LENGTH_BYTES;
    memcpy(shard->value, source + METADATA_LENGTH_BYTES, shard->value_len);

    size_t err = check_secret_length(shard->value_len);
    if(err) {
        return err;
    }
    return shard->value_len;
}

int sskr_count_shards(
    size_t group_threshold,
    const sskr_group_descriptor *groups,
    size_t groups_len
) {
    size_t shard_count = 0;

    if(group_threshold > groups_len) {
        return SSKR_ERROR_INVALID_GROUP_THRESHOLD;
    }

    for(int i = 0; i < groups_len; ++i) {
        shard_count += groups[i].count;
        if( groups[i].threshold > groups[i].count ) {
            return SSKR_ERROR_INVALID_MEMBER_THRESHOLD;
        }
        if( groups[i].threshold == 1 && groups[i].count > 1) {
            return SSKR_ERROR_INVALID_SINGLETON_MEMBER;
        }
    }

    return shard_count;
}

//////////////////////////////////////////////////
// generate shards
//
static int generate_shards(
    uint8_t group_threshold,
    const sskr_group_descriptor *groups,
    uint8_t groups_len,
    const uint8_t *master_secret,
    size_t master_secret_len,
    sskr_shard *shards,
    uint16_t shards_size,
    void* ctx,
    void (*random_generator)(uint8_t *, size_t, void*)
) {
    size_t err = check_secret_length(master_secret_len);
    if(err) {
        return err;
    }

    // Figure out how many shards we are dealing with
    int total_shards = sskr_count_shards(group_threshold, groups, groups_len);
    if(total_shards < 0) {
        return total_shards;
    }

    // assign a random identifier
    uint16_t identifier = 0;
    random_generator((uint8_t *)(&identifier), 2, ctx);

    if(shards_size < total_shards) {
        return SSKR_ERROR_INSUFFICIENT_SPACE;
    }

    if(group_threshold > groups_len) {
        return SSKR_ERROR_INVALID_GROUP_THRESHOLD;
    }

    uint8_t group_shares[master_secret_len * groups_len];

    split_secret(group_threshold, groups_len, master_secret, master_secret_len, group_shares, ctx, random_generator);

    uint8_t *group_share = group_shares;

    unsigned int shards_count = 0;
    sskr_shard *shard;

    for(uint8_t i=0; i<groups_len; ++i, group_share += master_secret_len) {
        uint8_t member_shares[master_secret_len *groups[i].count];
        split_secret(groups[i].threshold, groups[i].count, group_share, master_secret_len, member_shares, ctx, random_generator);

        uint8_t *value = member_shares;
        for(uint8_t j=0; j< groups[i].count; ++j, value += master_secret_len) {
            shard = &shards[shards_count];

            shard->identifier = identifier;
            shard->group_threshold = group_threshold;
            shard->group_count = groups_len;
            shard->value_len = master_secret_len;
            shard->group_index = i;
            shard->member_threshold = groups[i].threshold;
            shard->member_index = j;
            memset(shard->value, 0, 32);
            memcpy(shard->value, value, master_secret_len);

            shards_count++;
        }

        // clean up
        memset(member_shares, 0, sizeof(member_shares));
    }

    // clean up stack
    memset(group_shares, 0, sizeof(group_shares));

    // return the number of shards generated
    return shards_count;
}

//////////////////////////////////////////////////
// generate mnemonics
//
int sskr_generate(
    size_t group_threshold,
    const sskr_group_descriptor *groups,
    size_t groups_len,
    const uint8_t *master_secret,
    size_t master_secret_len,
    size_t *shard_len,
    uint8_t *output,
    size_t buffer_size,
    void* ctx,
    void (*random_generator)(uint8_t *, size_t, void*)
) {
    size_t err = check_secret_length(master_secret_len);
    if(err) {
        return err;
    }

    // Figure out how many shards we are dealing with
    int total_shards = sskr_count_shards(group_threshold, groups, groups_len);
    if(total_shards < 0) {
        return total_shards;
    }

    // figure out how much space we need to store all of the mnemonics
    // and make sure that we were provided with sufficient resources
    size_t shard_length = METADATA_LENGTH_BYTES + master_secret_len;
    if(buffer_size < shard_length * total_shards) {
        return SSKR_ERROR_INSUFFICIENT_SPACE;
    }

    int error = 0;

    // allocate space for shard representations
    sskr_shard shards[total_shards];

    // generate shards
    total_shards = generate_shards(group_threshold, groups, groups_len, master_secret, master_secret_len, shards, total_shards, ctx, random_generator);

    if(total_shards < 0) {
        error = total_shards;
    }

    uint8_t *cur_output = output;
    unsigned int remaining_buffer = buffer_size;
    unsigned int byte_count = 0;

    for(size_t i = 0; !error && i < total_shards; ++i) {
        int bytes = serialize_shard(&shards[i], cur_output, remaining_buffer);
        if(bytes < 0) {
            error = bytes;
            break;
        }
        byte_count = bytes;
        remaining_buffer -= byte_count;
        cur_output += byte_count;
    }

    memset(shards, 0, sizeof(shards));
    if(error) {
        memset(output, 0, buffer_size);
        return 0;
    }

    *shard_len = byte_count;
    return total_shards;
}

typedef struct sskr_group_struct {
    size_t group_index;
    size_t member_threshold;
    size_t count;
    uint8_t member_index[16];
    const uint8_t *value[16];
} sskr_group;

/**
 * This version of combine shards potentially modifies the shard structures
 * in place, so it is for internal use only, however it provides the implementation
 * for both combine_shards and sskr_combine.
 */
static int combine_shards_internal(
    sskr_shard *shards,       // array of shard structures
    size_t shards_count,      // number of shards in array
    uint8_t *buffer,            // working space, and place to return secret
    size_t buffer_len      // total amount of working space
) {
    int error = 0;
    uint16_t identifier = 0;
    size_t group_threshold = 0;
    size_t group_count = 0;

    if(shards_count == 0) {
        return SSKR_ERROR_EMPTY_SHARD_SET;
    }

    size_t next_group = 0;
    sskr_group groups[16];
    size_t secret_len = 0;

    for(unsigned int i=0; i<shards_count; ++i) {
        sskr_shard *shard = &shards[i];

        if( i == 0) {
            // on the first one, establish expected values for common metadata
            identifier = shard->identifier;
            group_count = shard->group_count;
            group_threshold = shard->group_threshold;
            secret_len = shard->value_len;
        } else {
            // on subsequent shards, check that common metadata matches
            if( shard->identifier != identifier ||
                shard->group_threshold != group_threshold ||
                shard->group_count != group_count ||
                shard->value_len != secret_len
            ) {
                return SSKR_ERROR_INVALID_SHARD_SET;
            }
        }

        // sort shards into member groups
        bool group_found = false;
        for(int j = 0; j < next_group; ++j) {
            if(shard->group_index == groups[j].group_index) {
                group_found = true;
                if(shard->member_threshold != groups[j].member_threshold) {
                    return SSKR_ERROR_INVALID_MEMBER_THRESHOLD;
                }
                for(int k = 0; k < groups[j].count; ++k) {
                    if(shard->member_index == groups[j].member_index[k]) {
                        return SSKR_ERROR_DUPLICATE_MEMBER_INDEX;
                    }
                }
                groups[j].member_index[groups[j].count] = shard->member_index;
                groups[j].value[groups[j].count] = shard->value;
                groups[j].count++;
            }
        }

        if(!group_found) {
            sskr_group* g = &groups[next_group];
            g->group_index = shard->group_index;
            g->member_threshold = shard->member_threshold;
            g->count = 1;
            g->member_index[0] = shard->member_index;
            g->value[0] = shard->value;
            next_group++;
        }
    }

    if(buffer_len < secret_len) {
        error = SSKR_ERROR_INSUFFICIENT_SPACE;
    } else if(next_group < group_threshold) {
        error = SSKR_ERROR_NOT_ENOUGH_GROUPS;
    }

    // here, all of the shards are unpacked into member groups. Now we go through each
    // group and recover the group secret, and then use the result to recover the
    // master secret
    uint8_t gx[16];
    const uint8_t *gy[16];

    // allocate enough space for the group shards and the encrypted master secret
    uint8_t group_shares[secret_len * (group_threshold + 1)];
    uint8_t *group_share = group_shares;

    for(uint8_t i = 0; !error && i < next_group; ++i) {
        sskr_group* g = &groups[i];

        gx[i] = g->group_index;
        if(g->count < g->member_threshold) {
            error = SSKR_ERROR_NOT_ENOUGH_MEMBER_SHARDS;
            break;
        }

        int recovery = recover_secret(
            g->member_threshold, g->member_index,
            g->value, secret_len, group_share);

        if(recovery < 0) {
            error = recovery;
            break;
        }
        gy[i] = group_share;

        group_share += recovery;
    }

    int recovery = 0;
    if(!error) {
        recovery = recover_secret(group_threshold, gx, gy, secret_len, group_share);
    }

    if(recovery < 0) {
        error = recovery;
    }

    // copy the result to the beinning of the buffer supplied
    if(!error) {
        memcpy(buffer, group_share, secret_len);
    }

    // clean up stack
    memset(group_shares, 0, sizeof(group_shares));
    memset(gx, 0, sizeof(gx));
    memset(gy, 0, sizeof(gy));
    memset(groups, 0, sizeof(groups));

    if(error) {
        return error;
    }

    return secret_len;
}

static int combine_shards(
    const sskr_shard *shards, // array of shard structures
    uint16_t shards_count,      // number of shards in array
    uint8_t *buffer,            // working space, and place to return secret
    size_t buffer_len      // total amount of working space
) {
    if(shards_count == 0) {
        return SSKR_ERROR_EMPTY_SHARD_SET;
    }

    sskr_shard working_shards[shards_count];
    memcpy(working_shards, shards, sizeof(working_shards));

    int result = combine_shards_internal(working_shards, shards_count, buffer, buffer_len);

    memset(working_shards,0, sizeof(working_shards));

    return result;
}

/////////////////////////////////////////////////
// sskr_combine

int sskr_combine(
    const uint8_t **input_shards, // array of pointers to 10-bit words
    size_t shard_len,   // number of bytes in each serialized shard
    size_t shards_count,  // total number of shards
    uint8_t *buffer,            // working space, and place to return secret
    size_t buffer_len      // total amount of working space
) {
    int result = 0;

    if(shards_count == 0) {
        return SSKR_ERROR_EMPTY_SHARD_SET;
    }

    sskr_shard shards[shards_count];

    for(unsigned int i=0; !result && i < shards_count; ++i) {
        shards[i].value_len = 32;

        int bytes = deserialize_shard(input_shards[i], shard_len, &shards[i]);

        if(bytes < 0) {
            result = bytes;
        }
    }

    if(!result) {
        result = combine_shards_internal(shards, shards_count, buffer, buffer_len);
    }

    memset(shards,0,sizeof(shards));

    return result;
}
