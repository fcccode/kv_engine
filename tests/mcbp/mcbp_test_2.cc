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

#include "config.h"
#include "mcbp_test.h"

#include <daemon/cookie.h>
#include <daemon/settings.h>
#include <event2/event.h>
#include <mcbp/protocol/header.h>
#include <memcached/protocol_binary.h>
#include <gsl/gsl>
#include <memory>

/**
 * Test all of the command validators we've got to ensure that they
 * catch broken packets. There is still a high number of commands we
 * don't have any command validators for...
 */
namespace mcbp {
namespace test {

class DropPrivilegeValidatorTest : public ::testing::WithParamInterface<bool>,
                                   public ValidatorTest {
public:
    DropPrivilegeValidatorTest()
        : ValidatorTest(GetParam()), req(request.message.header.request) {
    }

    void SetUp() override {
        ValidatorTest::SetUp();
        req.setKeylen(10);
        req.setBodylen(10);
    }

protected:
    cb::mcbp::Request& req;
    cb::mcbp::Status validate() {
        return ValidatorTest::validate(cb::mcbp::ClientOpcode::DropPrivilege,
                                       static_cast<void*>(&request));
    }
};

TEST_P(DropPrivilegeValidatorTest, CorrectMessage) {
    EXPECT_EQ(cb::mcbp::Status::Success, validate());
}

TEST_P(DropPrivilegeValidatorTest, InvalidMagic) {
    req.magic = 0;
    EXPECT_EQ(cb::mcbp::Status::Einval, validate());
}

TEST_P(DropPrivilegeValidatorTest, InvalidExtlen) {
    req.setExtlen(2);
    req.setBodylen(req.getBodylen() + 2);
    EXPECT_EQ(cb::mcbp::Status::Einval, validate());
}

TEST_P(DropPrivilegeValidatorTest, InvalidDatatype) {
    req.setDatatype(cb::mcbp::Datatype::JSON);
    EXPECT_EQ(cb::mcbp::Status::Einval, validate());
}

TEST_P(DropPrivilegeValidatorTest, IvalidCas) {
    req.setCas(0xff);
    EXPECT_EQ(cb::mcbp::Status::Einval, validate());
}

TEST_P(DropPrivilegeValidatorTest, InvalidKey) {
    req.setKeylen(0);
    req.setBodylen(0);
    EXPECT_EQ(cb::mcbp::Status::Einval, validate());
}

TEST_P(DropPrivilegeValidatorTest, InvalidBodylen) {
    req.setBodylen(req.getKeylen() + 10);
    EXPECT_EQ(cb::mcbp::Status::Einval, validate());
}

INSTANTIATE_TEST_CASE_P(CollectionsOnOff,
                        DropPrivilegeValidatorTest,
                        ::testing::Bool(),
                        ::testing::PrintToStringParamName());

} // namespace test
} // namespace mcbp