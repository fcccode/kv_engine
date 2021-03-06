/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2017 Couchbase, Inc.
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

#include "cookie.h"

#include "buckets.h"
#include "connection.h"
#include "cookie_trace_context.h"
#include "mcaudit.h"
#include "mcbp.h"
#include "mcbp_executors.h"
#include "memcached.h"
#include "opentracing.h"
#include "protocol/mcbp/engine_errc_2_mcbp.h"
#include "sendbuffer.h"
#include "settings.h"

#include <logger/logger.h>
#include <mcbp/mcbp.h>
#include <mcbp/protocol/framebuilder.h>
#include <nlohmann/json.hpp>
#include <phosphor/phosphor.h>
#include <platform/checked_snprintf.h>
#include <platform/compress.h>
#include <platform/string_hex.h>
#include <platform/timeutils.h>
#include <platform/uuid.h>
#include <utilities/logtags.h>
#include <chrono>

nlohmann::json Cookie::toJSON() const {
    nlohmann::json ret;

    if (packet == nullptr) {
        ret["packet"] = nlohmann::json();
    } else {
        const auto& header = getHeader();
        ret["packet"] = header.toJSON(validated);
    }

    if (!event_id.empty()) {
        ret["event_id"] = event_id;
    }

    if (!error_context.empty()) {
        ret["error_context"] = error_context;
    }

    if (cas != 0) {
        ret["cas"] = std::to_string(cas);
    }

    ret["connection"] = connection.getDescription();
    ret["ewouldblock"] = ewouldblock;
    ret["aiostat"] = to_string(cb::engine_errc(aiostat));
    ret["refcount"] = uint32_t(refcount);
    ret["engine_storage"] = cb::to_hex(uint64_t(engine_storage));
    return ret;
}

const std::string& Cookie::getEventId() const {
    if (event_id.empty()) {
        event_id = to_string(cb::uuid::random());
    }

    return event_id;
}

void Cookie::setErrorJsonExtras(const nlohmann::json& json) {
    if (json.find("error") != json.end()) {
        throw std::invalid_argument(
                "Cookie::setErrorJsonExtras: cannot use \"error\" as a key, "
                "json:" +
                json.dump());
    }

    error_extra_json = json;
}

const std::string& Cookie::getErrorJson() {
    json_message.clear();
    if (error_context.empty() && event_id.empty() && error_extra_json.empty()) {
        return json_message;
    }

    nlohmann::json error;
    if (!error_context.empty()) {
        error["context"] = error_context;
    }
    if (!event_id.empty()) {
        error["ref"] = event_id;
    }

    nlohmann::json root;

    if (!error.empty()) {
        root["error"] = error;
    }

    if (!error_extra_json.empty()) {
        root.update(error_extra_json);
    }
    json_message = root.dump();
    return json_message;
}

bool Cookie::execute() {
    if (!validated) {
        throw std::runtime_error("Cookie::execute: validate() not called");
    }

    // Reset ewouldblock state!
    setEwouldblock(false);
    const auto& header = getHeader();
    if (header.isResponse()) {
        execute_response_packet(*this, header.getResponse());
    } else {
        // We've already verified that the packet is a legal packet
        // so it must be a request
        execute_request_packet(*this, header.getRequest());
    }

    if (isEwouldblock()) {
        return false;
    }

    collectTimings();
    return true;
}

void Cookie::setPacket(const cb::mcbp::Header& header, bool copy) {
    if (copy) {
        auto frame = header.getFrame();
        frame_copy = std::make_unique<uint8_t[]>(frame.size());
        std::copy(frame.begin(), frame.end(), frame_copy.get());
        packet = reinterpret_cast<const cb::mcbp::Header*>(frame_copy.get());
    } else {
        packet = &header;
    }
}

cb::const_byte_buffer Cookie::getPacket() const {
    if (packet == nullptr) {
        return {};
    }

    return packet->getFrame();
}

const cb::mcbp::Header& Cookie::getHeader() const {
    if (packet == nullptr) {
        throw std::logic_error("Cookie::getHeader(): packet not available");
    }

    return *packet;
}

const cb::mcbp::Request& Cookie::getRequest() const {
    if (packet == nullptr) {
        throw std::logic_error("Cookie::getRequest(): packet not available");
    }

    return packet->getRequest();
}

ENGINE_ERROR_CODE Cookie::swapAiostat(ENGINE_ERROR_CODE value) {
    auto ret = getAiostat();
    setAiostat(value);
    return ret;
}

ENGINE_ERROR_CODE Cookie::getAiostat() const {
    return aiostat;
}

void Cookie::setAiostat(ENGINE_ERROR_CODE aiostat) {
    Cookie::aiostat = aiostat;
}

void Cookie::setEwouldblock(bool ewouldblock) {
    if (ewouldblock && !connection.isDCP()) {
        setAiostat(ENGINE_EWOULDBLOCK);
    }

    Cookie::ewouldblock = ewouldblock;
}

void Cookie::sendNotMyVBucket() {
    auto pair = connection.getBucket().clusterConfiguration.getConfiguration();
    if (pair.first == -1 || (pair.first == connection.getClustermapRevno() &&
                             Settings::instance().isDedupeNmvbMaps())) {
        // We don't have a vbucket map, or we've already sent it to the
        // client
        connection.sendResponse(*this,
                                cb::mcbp::Status::NotMyVbucket,
                                {},
                                {},
                                {},
                                PROTOCOL_BINARY_RAW_BYTES,
                                {});
        return;
    }

    // Send the new payload
    connection.sendResponse(*this,
                            cb::mcbp::Status::NotMyVbucket,
                            {},
                            {},
                            {pair.second->data(), pair.second->size()},
                            PROTOCOL_BINARY_DATATYPE_JSON,
                            {});
    connection.setClustermapRevno(pair.first);
}

void Cookie::sendResponse(cb::mcbp::Status status) {
    if (status == cb::mcbp::Status::Success) {
        const auto& request = getHeader().getRequest();
        const auto quiet = request.isQuiet();
        if (quiet) {
            // The responseCounter is updated here as this is non-responding
            // code hence mcbp_add_header will not be called (which is what
            // normally updates the responseCounters).
            auto& bucket = connection.getBucket();
            ++bucket.responseCounters[int(cb::mcbp::Status::Success)];
            return;
        }

        connection.sendResponse(
                *this, status, {}, {}, {}, PROTOCOL_BINARY_RAW_BYTES, {});
        return;
    }

    if (status == cb::mcbp::Status::NotMyVbucket) {
        sendNotMyVBucket();
        return;
    }

    // fall back sending the error message (and include the JSON payload etc)
    sendResponse(status, {}, {}, {}, cb::mcbp::Datatype::Raw, cas);
}

void Cookie::sendResponse(cb::engine_errc code) {
    sendResponse(cb::mcbp::to_status(code));
}

void Cookie::sendResponse(cb::mcbp::Status status,
                          cb::const_char_buffer extras,
                          cb::const_char_buffer key,
                          cb::const_char_buffer value,
                          cb::mcbp::Datatype datatype,
                          uint64_t cas) {
    if (status == cb::mcbp::Status::NotMyVbucket) {
        sendNotMyVBucket();
        return;
    }

    const auto& error_json = getErrorJson();

    if (cb::mcbp::isStatusSuccess(status)) {
        setCas(cas);
    } else {
        // This is an error message.. Inject the error JSON!
        extras = {};
        key = {};
        value = {error_json.data(), error_json.size()};
        datatype = value.empty() ? cb::mcbp::Datatype::Raw
                                 : cb::mcbp::Datatype::JSON;
    }

    connection.sendResponse(*this,
                            status,
                            extras,
                            key,
                            value,
                            connection.getEnabledDatatypes(
                                    protocol_binary_datatype_t(datatype)),
                            {});
}

const DocKey Cookie::getRequestKey() const {
    return connection.makeDocKey(getRequest().getKey());
}

std::string Cookie::getPrintableRequestKey() const {
    const auto key = getRequest().getKey();

    std::string buffer{reinterpret_cast<const char*>(key.data()), key.size()};
    for (auto& ii : buffer) {
        if (!std::isgraph(ii)) {
            ii = '.';
        }
    }

    return cb::tagUserData(buffer);
}

void Cookie::logCommand() const {
    if (Settings::instance().getVerbose() == 0) {
        // Info is not enabled.. we don't want to try to format
        // output
        return;
    }

    const auto opcode = getRequest().getClientOpcode();
    LOG_DEBUG("{}> {} {}",
              connection.getId(),
              to_string(opcode),
              getPrintableRequestKey());
}

void Cookie::logResponse(const char* reason) const {
    const auto opcode = getRequest().getClientOpcode();
    LOG_DEBUG("{}< {} {} - {}",
              connection.getId(),
              to_string(opcode),
              getPrintableRequestKey(),
              reason);
}

void Cookie::logResponse(ENGINE_ERROR_CODE code) const {
    if (Settings::instance().getVerbose() == 0) {
        // Info is not enabled.. we don't want to try to format
        // output
        return;
    }

    if (code == ENGINE_EWOULDBLOCK) {
        // This is a temporary state
        return;
    }

    logResponse(cb::to_string(cb::engine_errc(code)).c_str());
}

void Cookie::setCommandContext(CommandContext* ctx) {
    commandContext.reset(ctx);
}

void Cookie::maybeLogSlowCommand(
        std::chrono::steady_clock::duration elapsed) const {
    const auto opcode = getRequest().getClientOpcode();
    const auto limit = cb::mcbp::sla::getSlowOpThreshold(opcode);

    if (elapsed > limit) {
        const auto& header = getHeader();
        std::chrono::nanoseconds timings(elapsed);
        std::string command;
        try {
            command = to_string(opcode);
        } catch (const std::exception&) {
            char opcode_s[16];
            checked_snprintf(
                    opcode_s, sizeof(opcode_s), "0x%X", header.getOpcode());
            command.assign(opcode_s);
        }

        auto& c = getConnection();

        TRACE_COMPLETE2("memcached/slow",
                        "Slow cmd",
                        start,
                        start + elapsed,
                        "opcode",
                        getHeader().getOpcode(),
                        "connection_id",
                        c.getId());

        const std::string traceData = to_string(tracer);
        LOG_WARNING(
                R"({}: Slow operation. {{"cid":"{}/{:x}","duration":"{}","trace":"{}","command":"{}","peer":"{}","bucket":"{}","packet":{}}})",
                c.getId(),
                c.getConnectionId().data(),
                ntohl(getHeader().getOpaque()),
                cb::time2text(timings),
                traceData,
                command,
                c.getPeername(),
                c.getBucket().name,
                getHeader().toJSON(validated));
    }
}

Cookie::Cookie(Connection& conn) : connection(conn) {
}

void Cookie::initialize(const cb::mcbp::Header& header, bool tracing_enabled) {
    reset();
    setTracingEnabled(tracing_enabled ||
                      Settings::instance().alwaysCollectTraceInfo());
    setPacket(header);
    start = std::chrono::steady_clock::now();
    tracer.begin(cb::tracing::Code::Request, start);

    if (Settings::instance().getVerbose() > 1) {
        try {
            LOG_TRACE(">{} Read command {}",
                      connection.getId(),
                      header.toJSON(false).dump());
        } catch (const std::exception&) {
            // Failed to decode the header.. do a raw dump instead
            LOG_TRACE(">{} Read command {}",
                      connection.getId(),
                      cb::to_hex({reinterpret_cast<const uint8_t*>(packet),
                                  sizeof(*packet)}));
        }
    }
}

cb::mcbp::Status Cookie::validate() {
    static McbpValidator packetValidator;

    const auto& header = getHeader();

    if (!header.isValid()) {
        audit_invalid_packet(connection, getPacket());
        throw std::runtime_error("Received an invalid packet");
    }

    if (header.isRequest()) {
        const auto& request = header.getRequest();
        if (cb::mcbp::is_client_magic(request.getMagic())) {
            auto opcode = request.getClientOpcode();
            if (!cb::mcbp::is_valid_opcode(opcode)) {
                // We don't know about this command so we can stop
                // processing it.
                return cb::mcbp::Status::UnknownCommand;
            }

            auto result = packetValidator.validate(opcode, *this);
            if (result != cb::mcbp::Status::Success) {
                LOG_WARNING(
                        "{}: Invalid format specified for \"{}\" - Status: "
                        "\"{}\" - Closing connection. Packet:[{}] "
                        "Reason:\"{}\"",
                        connection.getId(),
                        to_string(opcode),
                        to_string(result),
                        request.toJSON(false).dump(),
                        getErrorContext());
                audit_invalid_packet(getConnection(), getPacket());
                return result;
            }
        } else {
            // We should not be receiving a server command.
            // Audit the packet, and close the connection
            audit_invalid_packet(connection, getPacket());
            throw std::runtime_error("Received a server command");
        }

        // Add a barrier to the command if we don't support reordering it!
        if (reorder && !is_reorder_supported(request.getClientOpcode())) {
            setBarrier();
        }
    } // We don't currently have any validators for response packets

    validated = true;
    return cb::mcbp::Status::Success;
}

void Cookie::reset() {
    event_id.clear();
    error_context.clear();
    json_message.clear();
    packet = {};
    validated = false;
    cas = 0;
    commandContext.reset();
    tracer.clear();
    ewouldblock = false;
    openTracingContext.clear();
    authorized = false;
    reorder = connection.allowUnorderedExecution();
    inflated_input_payload.reset();
}

void Cookie::setOpenTracingContext(cb::const_byte_buffer context) {
    try {
        openTracingContext.assign(reinterpret_cast<const char*>(context.data()),
                                  context.size());
    } catch (const std::bad_alloc&) {
        // Drop tracing if we run out of memory
    }
}

CookieTraceContext Cookie::extractTraceContext() {
    if (openTracingContext.empty()) {
        throw std::logic_error(
                "Cookie::extractTraceContext should only be called if we have "
                "a context");
    }

    auto& header = getHeader();
    return CookieTraceContext{cb::mcbp::Magic(header.getMagic()),
                              header.getOpcode(),
                              header.getOpaque(),
                              header.getKey(),
                              std::move(openTracingContext),
                              std::move(tracer)};
}

void Cookie::collectTimings() {
    // The state machinery cause this method to be called for all kinds
    // of packets, but the header musts be a client request for the timings
    // to make sense (and not when we handled a ServerResponse message etc ;)
    if (!packet->isRequest() || connection.isDCP()) {
        return;
    }

    const auto opcode = packet->getRequest().getClientOpcode();
    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsed = endTime - start;
    getTracer().end(cb::tracing::Code::Request, endTime);

    // aggregated timing for all buckets
    all_buckets[0].timings.collect(opcode, elapsed);

    // timing for current bucket
    const auto bucketid = connection.getBucketIndex();
    /* bucketid will be zero initially before you run sasl auth
     * (unless there is a default bucket), or if someone tries
     * to delete the bucket you're associated with and your're idle.
     */
    if (bucketid != 0) {
        all_buckets[bucketid].timings.collect(opcode, elapsed);
    }

    // Log operations taking longer than the "slow" threshold for the opcode.
    maybeLogSlowCommand(elapsed);

    if (isOpenTracingEnabled()) {
        OpenTracing::pushTraceLog(extractTraceContext());
    }
}

cb::const_char_buffer Cookie::getInflatedInputPayload() const {
    if (!inflated_input_payload.empty()) {
        return inflated_input_payload;
    }

    const auto value = getHeader().getValue();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

bool Cookie::inflateInputPayload(const cb::mcbp::Header& header) {
    inflated_input_payload.reset();
    if (!mcbp::datatype::is_snappy(header.getDatatype())) {
        return true;
    }

    try {
        auto val = header.getValue();
        if (!cb::compression::inflate(
                    cb::compression::Algorithm::Snappy,
                    {reinterpret_cast<const char*>(val.data()), val.size()},
                    inflated_input_payload)) {
            setErrorContext("Failed to inflate payload");
            return false;
        }
    } catch (const std::bad_alloc&) {
        setErrorContext("Failed to allocate memory");
        return false;
    }

    return true;
}
