/* -*- Mode: C++} tab-width: 4} c-basic-offset: 4} indent-tabs-mode: nil -*- */
/*
 *     Copyright 2015 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License")}
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
#include "config.h"
#include <memcached/protocol_binary.h>
#include <map>
#include <string>
#include <strings.h>
#include "protocol2text.h"

static const std::map<uint8_t, std::string> commandmap {
    {PROTOCOL_BINARY_CMD_GET,"GET"},
    {PROTOCOL_BINARY_CMD_SET,"SET"},
    {PROTOCOL_BINARY_CMD_ADD,"ADD"},
    {PROTOCOL_BINARY_CMD_REPLACE,"REPLACE"},
    {PROTOCOL_BINARY_CMD_DELETE,"DELETE"},
    {PROTOCOL_BINARY_CMD_INCREMENT,"INCREMENT"},
    {PROTOCOL_BINARY_CMD_DECREMENT,"DECREMENT"},
    {PROTOCOL_BINARY_CMD_QUIT,"QUIT"},
    {PROTOCOL_BINARY_CMD_FLUSH,"FLUSH"},
    {PROTOCOL_BINARY_CMD_GETQ,"GETQ"},
    {PROTOCOL_BINARY_CMD_NOOP,"NOOP"},
    {PROTOCOL_BINARY_CMD_VERSION,"VERSION"},
    {PROTOCOL_BINARY_CMD_GETK,"GETK"},
    {PROTOCOL_BINARY_CMD_GETKQ,"GETKQ"},
    {PROTOCOL_BINARY_CMD_APPEND,"APPEND"},
    {PROTOCOL_BINARY_CMD_PREPEND,"PREPEND"},
    {PROTOCOL_BINARY_CMD_STAT,"STAT"},
    {PROTOCOL_BINARY_CMD_SETQ,"SETQ"},
    {PROTOCOL_BINARY_CMD_ADDQ,"ADDQ"},
    {PROTOCOL_BINARY_CMD_REPLACEQ,"REPLACEQ"},
    {PROTOCOL_BINARY_CMD_DELETEQ,"DELETEQ"},
    {PROTOCOL_BINARY_CMD_INCREMENTQ,"INCREMENTQ"},
    {PROTOCOL_BINARY_CMD_DECREMENTQ,"DECREMENTQ"},
    {PROTOCOL_BINARY_CMD_QUITQ,"QUITQ"},
    {PROTOCOL_BINARY_CMD_FLUSHQ,"FLUSHQ"},
    {PROTOCOL_BINARY_CMD_APPENDQ,"APPENDQ"},
    {PROTOCOL_BINARY_CMD_PREPENDQ,"PREPENDQ"},
    {PROTOCOL_BINARY_CMD_VERBOSITY,"VERBOSITY"},
    {PROTOCOL_BINARY_CMD_TOUCH,"TOUCH"},
    {PROTOCOL_BINARY_CMD_GAT,"GAT"},
    {PROTOCOL_BINARY_CMD_GATQ,"GATQ"},
    {PROTOCOL_BINARY_CMD_HELLO,"HELLO"},
    {PROTOCOL_BINARY_CMD_SASL_LIST_MECHS,"SASL_LIST_MECHS"},
    {PROTOCOL_BINARY_CMD_SASL_AUTH,"SASL_AUTH"},
    {PROTOCOL_BINARY_CMD_SASL_STEP,"SASL_STEP"},
    {PROTOCOL_BINARY_CMD_IOCTL_GET,"IOCTL_GET"},
    {PROTOCOL_BINARY_CMD_IOCTL_SET,"IOCTL_SET"},
    {PROTOCOL_BINARY_CMD_CONFIG_VALIDATE,"CONFIG_VALIDATE"},
    {PROTOCOL_BINARY_CMD_CONFIG_RELOAD,"CONFIG_RELOAD"},
    {PROTOCOL_BINARY_CMD_SHUTDOWN,"SHUTDOWN"},
    {PROTOCOL_BINARY_CMD_AUDIT_PUT,"AUDIT_PUT"},
    {PROTOCOL_BINARY_CMD_AUDIT_CONFIG_RELOAD,"AUDIT_CONFIG_RELOAD"},
    {PROTOCOL_BINARY_CMD_RGET,"RGET"},
    {PROTOCOL_BINARY_CMD_RSET,"RSET"},
    {PROTOCOL_BINARY_CMD_RSETQ,"RSETQ"},
    {PROTOCOL_BINARY_CMD_RAPPEND,"RAPPEND"},
    {PROTOCOL_BINARY_CMD_RAPPENDQ,"RAPPENDQ"},
    {PROTOCOL_BINARY_CMD_RPREPEND,"RPREPEND"},
    {PROTOCOL_BINARY_CMD_RPREPENDQ,"RPREPENDQ"},
    {PROTOCOL_BINARY_CMD_RDELETE,"RDELETE"},
    {PROTOCOL_BINARY_CMD_RDELETEQ,"RDELETEQ"},
    {PROTOCOL_BINARY_CMD_RINCR,"RINCR"},
    {PROTOCOL_BINARY_CMD_RINCRQ,"RINCRQ"},
    {PROTOCOL_BINARY_CMD_RDECR,"RDECR"},
    {PROTOCOL_BINARY_CMD_RDECRQ,"RDECRQ"},
    {PROTOCOL_BINARY_CMD_SET_VBUCKET,"SET_VBUCKET"},
    {PROTOCOL_BINARY_CMD_GET_VBUCKET,"GET_VBUCKET"},
    {PROTOCOL_BINARY_CMD_DEL_VBUCKET,"DEL_VBUCKET"},
    {PROTOCOL_BINARY_CMD_TAP_CONNECT,"TAP_CONNECT"},
    {PROTOCOL_BINARY_CMD_TAP_MUTATION,"TAP_MUTATION"},
    {PROTOCOL_BINARY_CMD_TAP_DELETE,"TAP_DELETE"},
    {PROTOCOL_BINARY_CMD_TAP_FLUSH,"TAP_FLUSH"},
    {PROTOCOL_BINARY_CMD_TAP_OPAQUE,"TAP_OPAQUE"},
    {PROTOCOL_BINARY_CMD_TAP_VBUCKET_SET,"TAP_VBUCKET_SET"},
    {PROTOCOL_BINARY_CMD_TAP_CHECKPOINT_START,"TAP_CHECKPOINT_START"},
    {PROTOCOL_BINARY_CMD_TAP_CHECKPOINT_END,"TAP_CHECKPOINT_END"},
    {PROTOCOL_BINARY_CMD_GET_ALL_VB_SEQNOS,"GET_ALL_VB_SEQNOS"},
    {PROTOCOL_BINARY_CMD_DCP_OPEN,"DCP_OPEN"},
    {PROTOCOL_BINARY_CMD_DCP_ADD_STREAM,"DCP_ADD_STREAM"},
    {PROTOCOL_BINARY_CMD_DCP_CLOSE_STREAM,"DCP_CLOSE_STREAM"},
    {PROTOCOL_BINARY_CMD_DCP_STREAM_REQ,"DCP_STREAM_REQ"},
    {PROTOCOL_BINARY_CMD_DCP_GET_FAILOVER_LOG,"DCP_GET_FAILOVER_LOG"},
    {PROTOCOL_BINARY_CMD_DCP_STREAM_END,"DCP_STREAM_END"},
    {PROTOCOL_BINARY_CMD_DCP_SNAPSHOT_MARKER,"DCP_SNAPSHOT_MARKER"},
    {PROTOCOL_BINARY_CMD_DCP_MUTATION,"DCP_MUTATION"},
    {PROTOCOL_BINARY_CMD_DCP_DELETION,"DCP_DELETION"},
    {PROTOCOL_BINARY_CMD_DCP_EXPIRATION,"DCP_EXPIRATION"},
    {PROTOCOL_BINARY_CMD_DCP_FLUSH,"DCP_FLUSH"},
    {PROTOCOL_BINARY_CMD_DCP_SET_VBUCKET_STATE,"DCP_SET_VBUCKET_STATE"},
    {PROTOCOL_BINARY_CMD_DCP_NOOP,"DCP_NOOP"},
    {PROTOCOL_BINARY_CMD_DCP_BUFFER_ACKNOWLEDGEMENT,"DCP_BUFFER_ACKNOWLEDGEMENT"},
    {PROTOCOL_BINARY_CMD_DCP_CONTROL,"DCP_CONTROL"},
    {PROTOCOL_BINARY_CMD_DCP_SYSTEM_EVENT,"DCP_SYSTEM_EVENT"},
    {PROTOCOL_BINARY_CMD_STOP_PERSISTENCE,"STOP_PERSISTENCE"},
    {PROTOCOL_BINARY_CMD_START_PERSISTENCE,"START_PERSISTENCE"},
    {PROTOCOL_BINARY_CMD_SET_PARAM,"SET_PARAM"},
    {PROTOCOL_BINARY_CMD_GET_REPLICA,"GET_REPLICA"},
    {PROTOCOL_BINARY_CMD_CREATE_BUCKET,"CREATE_BUCKET"},
    {PROTOCOL_BINARY_CMD_DELETE_BUCKET,"DELETE_BUCKET"},
    {PROTOCOL_BINARY_CMD_LIST_BUCKETS,"LIST_BUCKETS"},
    {PROTOCOL_BINARY_CMD_SELECT_BUCKET,"SELECT_BUCKET"},
    {PROTOCOL_BINARY_CMD_OBSERVE,"OBSERVE"},
    {PROTOCOL_BINARY_CMD_OBSERVE_SEQNO,"OBSERVE_SEQNO"},
    {PROTOCOL_BINARY_CMD_EVICT_KEY,"EVICT_KEY"},
    {PROTOCOL_BINARY_CMD_GET_LOCKED,"GET_LOCKED"},
    {PROTOCOL_BINARY_CMD_UNLOCK_KEY,"UNLOCK_KEY"},
    {PROTOCOL_BINARY_CMD_LAST_CLOSED_CHECKPOINT,"LAST_CLOSED_CHECKPOINT"},
    {PROTOCOL_BINARY_CMD_GET_META,"GET_META"},
    {PROTOCOL_BINARY_CMD_GETQ_META,"GETQ_META"},
    {PROTOCOL_BINARY_CMD_SET_WITH_META,"SET_WITH_META"},
    {PROTOCOL_BINARY_CMD_SETQ_WITH_META,"SETQ_WITH_META"},
    {PROTOCOL_BINARY_CMD_ADD_WITH_META,"ADD_WITH_META"},
    {PROTOCOL_BINARY_CMD_ADDQ_WITH_META,"ADDQ_WITH_META"},
    {PROTOCOL_BINARY_CMD_SNAPSHOT_VB_STATES,"SNAPSHOT_VB_STATES"},
    {PROTOCOL_BINARY_CMD_VBUCKET_BATCH_COUNT,"VBUCKET_BATCH_COUNT"},
    {PROTOCOL_BINARY_CMD_DEL_WITH_META,"DEL_WITH_META"},
    {PROTOCOL_BINARY_CMD_DELQ_WITH_META,"DELQ_WITH_META"},
    {PROTOCOL_BINARY_CMD_CREATE_CHECKPOINT,"CREATE_CHECKPOINT"},
    {PROTOCOL_BINARY_CMD_NOTIFY_VBUCKET_UPDATE,"NOTIFY_VBUCKET_UPDATE"},
    {PROTOCOL_BINARY_CMD_ENABLE_TRAFFIC,"ENABLE_TRAFFIC"},
    {PROTOCOL_BINARY_CMD_DISABLE_TRAFFIC,"DISABLE_TRAFFIC"},
    {PROTOCOL_BINARY_CMD_CHECKPOINT_PERSISTENCE,"CHECKPOINT_PERSISTENCE"},
    {PROTOCOL_BINARY_CMD_RETURN_META,"RETURN_META"},
    {PROTOCOL_BINARY_CMD_COMPACT_DB,"COMPACT_DB"},
    {PROTOCOL_BINARY_CMD_SET_CLUSTER_CONFIG,"SET_CLUSTER_CONFIG"},
    {PROTOCOL_BINARY_CMD_GET_CLUSTER_CONFIG,"GET_CLUSTER_CONFIG"},
    {PROTOCOL_BINARY_CMD_GET_RANDOM_KEY,"GET_RANDOM_KEY"},
    {PROTOCOL_BINARY_CMD_SEQNO_PERSISTENCE,"SEQNO_PERSISTENCE"},
    {PROTOCOL_BINARY_CMD_GET_KEYS,"GET_KEYS"},
    {PROTOCOL_BINARY_CMD_GET_ADJUSTED_TIME,"GET_ADJUSTED_TIME"},
    {PROTOCOL_BINARY_CMD_SET_DRIFT_COUNTER_STATE,"SET_DRIFT_COUNTER_STATE"},
    {PROTOCOL_BINARY_CMD_SUBDOC_GET,"SUBDOC_GET"},
    {PROTOCOL_BINARY_CMD_SUBDOC_EXISTS,"SUBDOC_EXISTS"},
    {PROTOCOL_BINARY_CMD_SUBDOC_DICT_ADD,"SUBDOC_DICT_ADD"},
    {PROTOCOL_BINARY_CMD_SUBDOC_DICT_UPSERT,"SUBDOC_DICT_UPSERT"},
    {PROTOCOL_BINARY_CMD_SUBDOC_DELETE,"SUBDOC_DELETE"},
    {PROTOCOL_BINARY_CMD_SUBDOC_REPLACE,"SUBDOC_REPLACE"},
    {PROTOCOL_BINARY_CMD_SUBDOC_ARRAY_PUSH_LAST,"SUBDOC_ARRAY_PUSH_LAST"},
    {PROTOCOL_BINARY_CMD_SUBDOC_ARRAY_PUSH_FIRST,"SUBDOC_ARRAY_PUSH_FIRST"},
    {PROTOCOL_BINARY_CMD_SUBDOC_ARRAY_INSERT,"SUBDOC_ARRAY_INSERT"},
    {PROTOCOL_BINARY_CMD_SUBDOC_ARRAY_ADD_UNIQUE,"SUBDOC_ARRAY_ADD_UNIQUE"},
    {PROTOCOL_BINARY_CMD_SUBDOC_COUNTER,"SUBDOC_COUNTER"},
    {PROTOCOL_BINARY_CMD_SUBDOC_MULTI_LOOKUP,"SUBDOC_MULTI_LOOKUP"},
    {PROTOCOL_BINARY_CMD_SUBDOC_MULTI_MUTATION,"SUBDOC_MULTI_MUTATION"},
    {PROTOCOL_BINARY_CMD_SUBDOC_GET_COUNT, "SUBDOC_GET_COUNT"},
    {PROTOCOL_BINARY_CMD_SCRUB,"SCRUB"},
    {PROTOCOL_BINARY_CMD_ISASL_REFRESH,"ISASL_REFRESH"},
    {PROTOCOL_BINARY_CMD_SSL_CERTS_REFRESH,"SSL_CERTS_REFRESH"},
    {PROTOCOL_BINARY_CMD_GET_CMD_TIMER,"GET_CMD_TIMER"},
    {PROTOCOL_BINARY_CMD_SET_CTRL_TOKEN,"SET_CTRL_TOKEN"},
    {PROTOCOL_BINARY_CMD_GET_CTRL_TOKEN,"GET_CTRL_TOKEN"},
    {PROTOCOL_BINARY_CMD_RBAC_REFRESH,"RBAC_REFRESH"},
    {PROTOCOL_BINARY_CMD_EWOULDBLOCK_CTL,"EWB_CTL"},
    {PROTOCOL_BINARY_CMD_GET_ERROR_MAP, "GET_ERROR_MAP"},
    {PROTOCOL_BINARY_CMD_DROP_PRIVILEGE, "DROP_PRIVILEGES"},
    {PROTOCOL_BINARY_CMD_COLLECTIONS_SET_MANIFEST, "COLLECTIONS_SET_MANIFEST"}
};

const char *memcached_opcode_2_text(uint8_t opcode) {
    auto ii = commandmap.find(opcode);
    if (ii != commandmap.end()) {
        return ii->second.c_str();
    }

    return NULL;
}

uint8_t memcached_text_2_opcode(const char *cmd) {
    try {
        return static_cast<uint8_t>(std::stoi(cmd));
    } catch (...) {
        for (auto &element : commandmap ) {
            if (strcasecmp(element.second.c_str(), cmd) == 0) {
                return element.first;
            }
        }
    }

    return 0xff;
}

static const std::map<protocol_binary_response_status, std::string> statusmap {
    {PROTOCOL_BINARY_RESPONSE_SUCCESS,
        "Success"},
    {PROTOCOL_BINARY_RESPONSE_KEY_ENOENT,
        "Not found"},
    {PROTOCOL_BINARY_RESPONSE_KEY_EEXISTS,
        "Data exists for key"},
    {PROTOCOL_BINARY_RESPONSE_E2BIG,
        "Too large"},
    {PROTOCOL_BINARY_RESPONSE_EINVAL,
        "Invalid arguments"},
    {PROTOCOL_BINARY_RESPONSE_NOT_STORED,
        "Not stored"},
    {PROTOCOL_BINARY_RESPONSE_DELTA_BADVAL,
        "Non-numeric server-side value for incr or decr"},
    {PROTOCOL_BINARY_RESPONSE_NOT_MY_VBUCKET,
        "I'm not responsible for this vbucket"},
    {PROTOCOL_BINARY_RESPONSE_NO_BUCKET,
        "Not connected to a bucket"},
    {PROTOCOL_BINARY_RESPONSE_LOCKED,
        "Resource locked"},
    {PROTOCOL_BINARY_RESPONSE_AUTH_STALE,
        "Authentication stale. Please reauthenticate"},
    {PROTOCOL_BINARY_RESPONSE_AUTH_ERROR,
        "Auth failure"},
    {PROTOCOL_BINARY_RESPONSE_AUTH_CONTINUE,
        "Auth continue"},
    {PROTOCOL_BINARY_RESPONSE_ERANGE,
        "Outside range"},
    {PROTOCOL_BINARY_RESPONSE_ROLLBACK,
        "Rollback"},
    {PROTOCOL_BINARY_RESPONSE_EACCESS,
        "No access"},
    {PROTOCOL_BINARY_RESPONSE_NOT_INITIALIZED,
        "Node not initialized"},
    {PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND,
        "Unknown command"},
    {PROTOCOL_BINARY_RESPONSE_ENOMEM,
        "Out of memory"},
    {PROTOCOL_BINARY_RESPONSE_NOT_SUPPORTED,
        "Not supported"},
    {PROTOCOL_BINARY_RESPONSE_EINTERNAL,
        "Internal error"},
    {PROTOCOL_BINARY_RESPONSE_EBUSY,
        "Server too busy"},
    {PROTOCOL_BINARY_RESPONSE_ETMPFAIL,
        "Temporary failure"},
    {PROTOCOL_BINARY_RESPONSE_XATTR_EINVAL,
        "Invalid XATTR key"},

    /* Sub-document responses */
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_PATH_ENOENT,
        "Subdoc: Path not does not exist"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_PATH_MISMATCH,
        "Subdoc: Path mismatch"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_PATH_EINVAL,
        "Subdoc: Invalid path"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_PATH_E2BIG,
        "Subdoc: Path too large"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_DOC_E2DEEP,
        "Subdoc: Document too deep"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_VALUE_CANTINSERT,
        "Subdoc: Cannot insert specified value"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_DOC_NOTJSON,
        "Subdoc: Existing document not JSON"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_NUM_ERANGE,
        "Subdoc: Existing number outside valid arithmetic range"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_DELTA_EINVAL,
        "Subdoc: Delta is 0, not a number, or outside the valid range"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_PATH_EEXISTS,
        "Subdoc: Document path already exists"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_VALUE_ETOODEEP,
        "Subdoc: Inserting value would make document too deep"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_INVALID_COMBO,
        "Subdoc: Invalid combination for multi-path command"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_MULTI_PATH_FAILURE,
        "Subdoc: One or more paths in a multi-path command failed"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_SUCCESS_DELETED,
        "Subdoc: Operation completed successfully on a deleted document"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_XATTR_INVALID_FLAG_COMBO,
        "Subdoc: Invalid combination of xattr flags"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_XATTR_INVALID_KEY_COMBO,
        "Subdoc: Invalid combination of xattr keys"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_XATTR_UNKNOWN_MACRO,
        "Subdoc: Unknown xattr macro"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_XATTR_UNKNOWN_VATTR,
        "Subdoc: Unknown xattr virtual attribute"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_XATTR_CANT_MODIFY_VATTR,
        "Subdoc: Can't modify virtual attributes"},
    {PROTOCOL_BINARY_RESPONSE_SUBDOC_INVALID_XATTR_ORDER,
        "Subdoc: Invalid XATTR order (xattrs should come first)"}
};

const char *memcached_status_2_text(protocol_binary_response_status status) {
    auto ii = statusmap.find(status);
    if (ii != statusmap.end()) {
        return ii->second.c_str();
    }

    return "Unknown error code";
}
