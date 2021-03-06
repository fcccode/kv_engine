/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2018 Couchbase, Inc.
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

#include <string>

namespace cb {
namespace sasl {

/**
 * An abstract context class to allow a common base class for the
 * client and server context.
 */
class Context {
public:
    virtual ~Context() = default;

    /**
     * Get the UUID used for errors by this connection. If none
     * is created a new one is generated.
     */
    std::string getUuid();

    /**
     * Do this context contain a UUID?
     */
    bool containsUuid() const {
        return !uuid.empty();
    }

protected:
    std::string uuid;
};

} // namespace sasl
} // namespace cb