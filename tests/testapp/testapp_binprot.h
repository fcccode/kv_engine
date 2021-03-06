/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2015 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#pragma once

// Utility functions used to build up packets in the memcached binary protocol
#include <protocol/connection/client_connection.h>

#include <folly/portability/GTest.h>
#include <sys/types.h>
#include <algorithm>
#include <cstdlib>

#include <memcached/protocol_binary.h>

std::vector<uint8_t> mcbp_arithmetic_command(cb::mcbp::ClientOpcode cmd,
                                             cb::const_char_buffer key,
                                             uint64_t delta,
                                             uint64_t initial,
                                             uint32_t exp);

/**
 * Constructs a storage command using the give arguments into buf.
 *
 * @param cmd the command opcode to use
 * @param key the key to use
 * @param value the value for the key
 * @param flags the value to use for the flags
 * @param exp the expiry time
 */
std::vector<uint8_t> mcbp_storage_command(cb::mcbp::ClientOpcode cmd,
                                          cb::const_char_buffer key,
                                          cb::const_char_buffer value,
                                          uint32_t flags,
                                          uint32_t exp);

/* Validate the specified response header against the expected cmd and status.
 */
void mcbp_validate_response_header(protocol_binary_response_no_extras* response,
                                   cb::mcbp::ClientOpcode cmd,
                                   cb::mcbp::Status status);

void mcbp_validate_response_header(cb::mcbp::Response& response,
                                   cb::mcbp::ClientOpcode cmd,
                                   cb::mcbp::Status status);

::testing::AssertionResult mcbp_validate_response_header(
        const cb::mcbp::Response& response,
        cb::mcbp::ClientOpcode cmd,
        cb::mcbp::Status status,
        bool mutation_seqno_enabled);
