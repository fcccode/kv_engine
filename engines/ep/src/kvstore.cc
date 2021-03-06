/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010 Couchbase, Inc
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

#include <fcntl.h>
#include <folly/lang/Assume.h>
#include <map>
#include <string>

#include "common.h"
#include "couch-kvstore/couch-kvstore.h"
#include "item.h"
#include "vbucket_state.h"
#ifdef EP_USE_MAGMA
#include "magma-kvstore/magma-kvstore.h"
#include "magma-kvstore/magma-kvstore_config.h"
#endif /* EP_USE_MAGMA */
#ifdef EP_USE_ROCKSDB
#include "rocksdb-kvstore/rocksdb-kvstore.h"
#include "rocksdb-kvstore/rocksdb-kvstore_config.h"
#endif
#include "bucket_logger.h"
#include "kvstore.h"
#include "kvstore_config.h"
#include "persistence_callback.h"
#include "statwriter.h"
#include "vbucket.h"
#include "vbucket_state.h"

#include <platform/dirutils.h>
#include <sys/types.h>
#include <sys/stat.h>

ScanContext::ScanContext(
        std::shared_ptr<StatusCallback<GetValue>> cb,
        std::shared_ptr<StatusCallback<CacheLookup>> cl,
        Vbid vb,
        size_t id,
        int64_t start,
        int64_t end,
        uint64_t purgeSeqno,
        DocumentFilter _docFilter,
        ValueFilter _valFilter,
        uint64_t _documentCount,
        const vbucket_state& vbucketState,
        const KVStoreConfig& _config,
        const std::vector<Collections::KVStore::DroppedCollection>&
                droppedCollections)
    : callback(cb),
      lookup(cl),
      lastReadSeqno(0),
      startSeqno(start),
      maxSeqno(end),
      purgeSeqno(purgeSeqno),
      scanId(id),
      vbid(vb),
      docFilter(_docFilter),
      valFilter(_valFilter),
      documentCount(_documentCount),
      maxVisibleSeqno(vbucketState.maxVisibleSeqno),
      persistedCompletedSeqno(vbucketState.persistedCompletedSeqno),
      logger(globalBucketLogger.get()),
      config(_config),
      collectionsContext(droppedCollections) {
}

void FileStats::reset() {
    readTimeHisto.reset();
    readSeekHisto.reset();
    readSizeHisto.reset();
    writeTimeHisto.reset();
    writeSizeHisto.reset();
    syncTimeHisto.reset();
    readCountHisto.reset();
    writeCountHisto.reset();
    totalBytesRead = 0;
    totalBytesWritten = 0;
}

size_t FileStats::getMemFootPrint() const {
    return readTimeHisto.getMemFootPrint() + readSeekHisto.getMemFootPrint() +
           readSizeHisto.getMemFootPrint() + writeTimeHisto.getMemFootPrint() +
           writeSizeHisto.getMemFootPrint() + syncTimeHisto.getMemFootPrint() +
           readCountHisto.getMemFootPrint() + writeCountHisto.getMemFootPrint();
}

KVStoreStats::KVStoreStats() = default;

void KVStoreStats::reset() {
    docsCommitted = 0;
    numOpen = 0;
    numClose = 0;
    numLoadedVb = 0;

    numCompactionFailure = 0;
    numGetFailure = 0;
    numSetFailure = 0;
    numDelFailure = 0;
    numOpenFailure = 0;
    numVbSetFailure = 0;

    io_bg_fetch_docs_read = 0;
    io_num_write = 0;
    io_bgfetch_doc_bytes = 0;
    io_document_write_bytes = 0;

    readTimeHisto.reset();
    readSizeHisto.reset();
    writeTimeHisto.reset();
    writeSizeHisto.reset();
    delTimeHisto.reset();
    commitHisto.reset();
    compactHisto.reset();
    saveDocsHisto.reset();
    batchSize.reset();
    snapshotHisto.reset();

    getMultiFsReadCount.reset();
    getMultiFsReadHisto.reset();
    getMultiFsReadPerDocHisto.reset();
    flusherWriteAmplificationHisto.reset();

    fsStats.reset();
    fsStatsCompaction.reset();
}

KVStoreRWRO KVStoreFactory::create(KVStoreConfig& config) {
    std::string backend = config.getBackend();
    if (backend == "couchdb") {
        auto rw = std::make_unique<CouchKVStore>(config);
        auto ro = rw->makeReadOnlyStore();
        return {rw.release(), ro.release()};
    }
#ifdef EP_USE_MAGMA
    else if (backend == "magma") {
        auto rw = std::make_unique<MagmaKVStore>(
                dynamic_cast<MagmaKVStoreConfig&>(config));
        return {rw.release(), nullptr};
    }
#endif
#ifdef EP_USE_ROCKSDB
    else if (backend == "rocksdb") {
        auto rw = std::make_unique<RocksDBKVStore>(
                dynamic_cast<RocksDBKVStoreConfig&>(config));
        return {rw.release(), nullptr};
    }
#endif
    else {
        throw std::invalid_argument("KVStoreFactory::create unknown backend:" +
                                    config.getBackend());
    }

    return {};
}

void KVFileHandleDeleter::operator()(KVFileHandle* kvFileHandle) {
    kvFileHandle->kvs.freeFileHandle(kvFileHandle);
}

void KVStore::createDataDir(const std::string& dbname) {
    try {
        cb::io::mkdirp(dbname);
    } catch (const std::system_error& error) {
        std::stringstream ss;
        ss << "Failed to create data directory ["
           << dbname << "]: " << error.code().message();
        throw std::runtime_error(ss.str());
    }
}

bool KVStore::updateCachedVBState(Vbid vbid, const vbucket_state& newState) {
    vbucket_state* vbState = getVBucketState(vbid);

    bool state_change_detected = true;
    if (vbState != nullptr) {
        //Check if there's a need for persistence
        if (vbState->needsToBePersisted(newState)) {
            vbState->transition.state = newState.transition.state;
            vbState->transition.failovers = newState.transition.failovers;
            vbState->transition.replicationTopology =
                    newState.transition.replicationTopology;
            vbState->persistedCompletedSeqno = newState.persistedCompletedSeqno;
            vbState->persistedPreparedSeqno = newState.persistedPreparedSeqno;
            vbState->highPreparedSeqno =
                    newState.highPreparedSeqno;
            vbState->maxVisibleSeqno = newState.maxVisibleSeqno;
            vbState->onDiskPrepares = newState.onDiskPrepares;
        } else {
            state_change_detected = false;
        }

        if (newState.maxDeletedSeqno > 0 &&
                vbState->maxDeletedSeqno < newState.maxDeletedSeqno) {
            vbState->maxDeletedSeqno = newState.maxDeletedSeqno;
        }

        vbState->highSeqno = newState.highSeqno;
        vbState->lastSnapStart = newState.lastSnapStart;
        vbState->lastSnapEnd = newState.lastSnapEnd;
        vbState->maxCas = std::max(vbState->maxCas, newState.maxCas);
        vbState->hlcCasEpochSeqno = newState.hlcCasEpochSeqno;
        vbState->mightContainXattrs = newState.mightContainXattrs;
        vbState->checkpointType = newState.checkpointType;
    } else {
        cachedVBStates[vbid.get()] = std::make_unique<vbucket_state>(newState);
        if (cachedVBStates[vbid.get()]->transition.state !=
            vbucket_state_dead) {
            cachedValidVBCount++;
        }
    }

    return state_change_detected;
}

bool KVStore::snapshotStats(const std::map<std::string,
                            std::string> &stats) {
    if (isReadOnly()) {
        throw std::logic_error("KVStore::snapshotStats: Cannot perform "
                        "on a read-only instance.");
    }

    size_t count = 0;
    size_t size = stats.size();
    std::stringstream stats_buf;
    stats_buf << "{";
    std::map<std::string, std::string>::const_iterator it = stats.begin();
    for (; it != stats.end(); ++it) {
        stats_buf << "\"" << it->first << "\": \"" << it->second << "\"";
        ++count;
        if (count < size) {
            stats_buf << ", ";
        }
    }
    stats_buf << "}";
    std::string dbname = configuration.getDBName();
    std::string next_fname = dbname + "/stats.json.new";

    FILE *new_stats = fopen(next_fname.c_str(), "w");
    if (new_stats == nullptr) {
        EP_LOG_INFO(
                "Failed to open the engine stats "
                "file \"{}\" due to an error \"{}\"; Not critical because new "
                "stats will be dumped later, please ignore.",
                next_fname.c_str(),
                strerror(errno));
        return false;
    }

    bool rv = true;
    if (fprintf(new_stats, "%s\n", stats_buf.str().c_str()) < 0) {
        EP_LOG_INFO(
                "Failed to write the engine stats to "
                "file \"{}\" due to an error \"{}\"; Not critical because new "
                "stats will be dumped later, please ignore.",
                next_fname.c_str(),
                strerror(errno));
        rv = false;
    }
    fclose(new_stats);

    if (rv) {
        std::string old_fname = dbname + "/stats.json.old";
        std::string stats_fname = dbname + "/stats.json";
        if (cb::io::isFile(old_fname) && remove(old_fname.c_str()) != 0) {
            EP_LOG_WARN(
                    "Failed to remove '{}': {}", old_fname, strerror(errno));
            remove(next_fname.c_str());
            rv = false;
        } else if (cb::io::isFile(stats_fname) &&
                   rename(stats_fname.c_str(), old_fname.c_str()) != 0) {
            EP_LOG_WARN("Failed to rename '{}' to '{}': {}",
                        stats_fname,
                        old_fname,
                        strerror(errno));
            remove(next_fname.c_str());
            rv = false;
        } else if (rename(next_fname.c_str(), stats_fname.c_str()) != 0) {
            EP_LOG_WARN("Failed to rename '{}' to '{}': {}",
                        next_fname,
                        stats_fname,
                        strerror(errno));
            remove(next_fname.c_str());
            rv = false;
        }
    }

    return rv;
}

KVStore::KVStore(KVStoreConfig& config, bool read_only)
    : configuration(config), readOnly(read_only) {
}

KVStore::~KVStore() = default;

std::string KVStore::getStatsPrefix() const {
    const auto shardId = configuration.getShardId();
    if (isReadOnly()) {
        return "ro_" + std::to_string(shardId);
    }
    return "rw_" + std::to_string(shardId);
}

void KVStore::addStats(const AddStatFn& add_stat,
                       const void* c,
                       const std::string& args) {
    const char* backend = configuration.getBackend().c_str();
    const auto prefix = getStatsPrefix();

    /* stats for both read-only and read-write threads */
    add_prefixed_stat(prefix, "backend_type", backend, add_stat, c);
    add_prefixed_stat(prefix, "open", st.numOpen, add_stat, c);
    add_prefixed_stat(prefix, "close", st.numClose, add_stat, c);
    add_prefixed_stat(prefix, "numLoadedVb", st.numLoadedVb, add_stat, c);

    // failure stats
    add_prefixed_stat(
            prefix, "failure_compaction", st.numCompactionFailure, add_stat, c);
    add_prefixed_stat(prefix, "failure_open", st.numOpenFailure, add_stat, c);
    add_prefixed_stat(prefix, "failure_get", st.numGetFailure, add_stat, c);

    if (!isReadOnly()) {
        add_prefixed_stat(prefix, "failure_set", st.numSetFailure, add_stat, c);
        add_prefixed_stat(prefix, "failure_del", st.numDelFailure, add_stat, c);
        add_prefixed_stat(
                prefix, "failure_vbset", st.numVbSetFailure, add_stat, c);
        add_prefixed_stat(
                prefix, "lastCommDocs", st.docsCommitted, add_stat, c);
    }

    add_prefixed_stat(prefix,
                      "io_bg_fetch_docs_read",
                      st.io_bg_fetch_docs_read,
                      add_stat,
                      c);
    add_prefixed_stat(prefix, "io_num_write", st.io_num_write, add_stat, c);
    add_prefixed_stat(prefix,
                      "io_bg_fetch_doc_bytes",
                      st.io_bgfetch_doc_bytes,
                      add_stat,
                      c);
    add_prefixed_stat(prefix,
                      "io_document_write_bytes",
                      st.io_document_write_bytes,
                      add_stat,
                      c);

    const size_t read = st.fsStats.totalBytesRead.load() +
                        st.fsStatsCompaction.totalBytesRead.load();
    add_prefixed_stat(prefix, "io_total_read_bytes", read, add_stat, c);

    const size_t written = st.fsStats.totalBytesWritten.load() +
                           st.fsStatsCompaction.totalBytesWritten.load();
    add_prefixed_stat(prefix, "io_total_write_bytes", written, add_stat, c);

    if (!isReadOnly()) {
        // Flusher Write Amplification - ratio of bytes written to disk by
        // flusher to "useful" user data written - i.e. doesn't include bytes
        // written later by compaction (after initial flush). Used to measure
        // the impact of KVstore on persistTo times.
        const double flusherWriteAmp =
                double(st.fsStats.totalBytesWritten.load()) /
                st.io_document_write_bytes;
        add_prefixed_stat(prefix,
                          "io_flusher_write_amplification",
                          flusherWriteAmp,
                          add_stat,
                          c);

        // Total Write Amplification - ratio of total bytes written to disk
        // to "useful" user data written over entire disk lifecycle. Includes
        // bytes during initial item flush to disk  and compaction.
        // Used to measure the overall write amplification.
        const double totalWriteAmp =
                double(written) / st.io_document_write_bytes;
        add_prefixed_stat(prefix,
                          "io_total_write_amplification",
                          totalWriteAmp,
                          add_stat,
                          c);
    }

    add_prefixed_stat(prefix,
                      "io_compaction_read_bytes",
                      st.fsStatsCompaction.totalBytesRead,
                      add_stat,
                      c);
    add_prefixed_stat(prefix,
                      "io_compaction_write_bytes",
                      st.fsStatsCompaction.totalBytesWritten,
                      add_stat,
                      c);
}

void KVStore::addTimingStats(const AddStatFn& add_stat, const void* c) {
    const auto prefix = getStatsPrefix();

    add_prefixed_stat(prefix, "commit", st.commitHisto, add_stat, c);
    add_prefixed_stat(prefix, "compact", st.compactHisto, add_stat, c);
    add_prefixed_stat(prefix, "snapshot", st.snapshotHisto, add_stat, c);
    add_prefixed_stat(prefix, "delete", st.delTimeHisto, add_stat, c);
    add_prefixed_stat(prefix, "save_documents", st.saveDocsHisto, add_stat, c);
    add_prefixed_stat(prefix, "readTime", st.readTimeHisto, add_stat, c);
    add_prefixed_stat(prefix, "readSize", st.readSizeHisto, add_stat, c);
    add_prefixed_stat(prefix, "writeTime", st.writeTimeHisto, add_stat, c);
    add_prefixed_stat(prefix, "writeSize", st.writeSizeHisto, add_stat, c);
    add_prefixed_stat(prefix, "saveDocCount", st.batchSize, add_stat, c);

    add_prefixed_stat(
            prefix, "getMultiFsReadCount", st.getMultiFsReadHisto, add_stat, c);
    add_prefixed_stat(prefix,
                      "getMultiFsReadPerDocCount",
                      st.getMultiFsReadPerDocHisto,
                      add_stat,
                      c);
    add_prefixed_stat(prefix,
                      "flusherWriteAmplificationRatio",
                      st.flusherWriteAmplificationHisto,
                      add_stat,
                      c);

    //file ops stats
    add_prefixed_stat(
            prefix, "fsReadTime", st.fsStats.readTimeHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsWriteTime", st.fsStats.writeTimeHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsSyncTime", st.fsStats.syncTimeHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsReadSize", st.fsStats.readSizeHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsWriteSize", st.fsStats.writeSizeHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsReadSeek", st.fsStats.readSeekHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsReadCount", st.fsStats.readCountHisto, add_stat, c);
    add_prefixed_stat(
            prefix, "fsWriteCount", st.fsStats.writeCountHisto, add_stat, c);
}

void KVStore::optimizeWrites(std::vector<queued_item>& items) {
    if (isReadOnly()) {
        throw std::logic_error(
                "KVStore::optimizeWrites: Not valid on a "
                "read-only object");
    }
    if (items.empty()) {
        return;
    }

    OrderItemsForDeDuplication cq;
    std::sort(items.begin(), items.end(), cq);
}

uint64_t KVStore::getLastPersistedSeqno(Vbid vbid) {
    vbucket_state* state = getVBucketState(vbid);
    if (state) {
        return state->highSeqno;
    }
    return 0;
}

uint64_t KVStore::prepareToDelete(Vbid vbid) {
    // MB-34380: We must clear the cached state
    resetCachedVBState(vbid);
    return prepareToDeleteImpl(vbid);
}


void KVStore::prepareToCreate(Vbid vbid) {
    resetCachedVBState(vbid);
    prepareToCreateImpl(vbid);
}

void KVStore::resetCachedVBState(Vbid vbid) {
    vbucket_state* state = getVBucketState(vbid);
    if (state) {
        state->reset();
    }
}

void KVStore::setSystemEvent(const Item& item, SetCallback cb) {
    switch (SystemEvent(item.getFlags())) {
    case SystemEvent::Collection: {
        auto createEvent = Collections::VB::Manifest::getCreateEventData(
                {item.getData(), item.getNBytes()});
        collectionsMeta.collections.push_back(
                {item.getBySeqno(), createEvent.metaData});
        collectionsMeta.setUid(createEvent.manifestUid);
        break;
    }
    case SystemEvent::Scope: {
        auto scopeEvent = Collections::VB::Manifest::getCreateScopeEventData(
                {item.getData(), item.getNBytes()});
        collectionsMeta.scopes.push_back(
                Collections::getScopeIDFromKey(item.getKey()));
        collectionsMeta.setUid(scopeEvent.manifestUid);
        break;
    }
    default:
        throw std::invalid_argument("KVStore::setSystemEvent: unknown event:" +
                                    std::to_string(item.getFlags()));
    }
    collectionsMeta.needsCommit = true;
    set(item, std::move(cb));
}

void KVStore::delSystemEvent(const Item& item, DeleteCallback cb) {
    switch (SystemEvent(item.getFlags())) {
    case SystemEvent::Collection: {
        auto dropEvent = Collections::VB::Manifest::getDropEventData(
                {item.getData(), item.getNBytes()});
        // The startSeqno is unknown, so here we set to zero. The Underlying
        // kvstore can discover the real startSeqno when processing the open
        // collection list against the dropped collection list. A kvstore which
        // can atomically drop a collection has no need for this, but one which
        // will background purge dropped collection should maintain the start.
        // Note: couch-kvstore will set the dropped start-seqno.
        collectionsMeta.droppedCollections.push_back(
                {0,
                 item.getBySeqno(),
                 Collections::getCollectionIDFromKey(item.getKey())});
        collectionsMeta.setUid(dropEvent.manifestUid);
        break;
    }
    case SystemEvent::Scope: {
        auto dropEvent = Collections::VB::Manifest::getDropScopeEventData(
                {item.getData(), item.getNBytes()});
        collectionsMeta.droppedScopes.push_back(
                Collections::getScopeIDFromKey(item.getKey()));
        collectionsMeta.setUid(dropEvent.manifestUid);
        break;
    }
    default:
        throw std::invalid_argument("KVStore::delSystemEvent: unknown event:" +
                                    std::to_string(item.getFlags()));
    }
    collectionsMeta.needsCommit = true;
    del(item, cb);
}

std::string to_string(KVStore::MutationStatus status) {
    switch (status) {
    case KVStore::MutationStatus::Success:
        return "MutationStatus::Success";
    case KVStore::MutationStatus::DocNotFound:
        return "MutationStatus::DocNotFound";
    case KVStore::MutationStatus::Failed:
        return "MutationStatus::Failed";
    }
    folly::assume_unreachable();
}

std::string to_string(KVStore::MutationSetResultState status) {
    switch (status) {
    case KVStore::MutationSetResultState::DocNotFound:
        return "MutationSetResultState::DocNotFound";
    case KVStore::MutationSetResultState::Failed:
        return "MutationSetResultState::Failed";
    case KVStore::MutationSetResultState::Insert:
        return "MutationSetResultState::Insert";
    case KVStore::MutationSetResultState::Update:
        return "MutationSetResultState::Update";
    }
    folly::assume_unreachable();
}

IORequest::IORequest(MutationRequestCallback cb, DiskDocKey itmKey)
    : callback(cb),
      key(std::move(itmKey)),
      start(std::chrono::steady_clock::now()) {
}

IORequest::~IORequest() = default;
