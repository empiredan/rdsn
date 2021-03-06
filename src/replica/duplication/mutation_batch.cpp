// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <dsn/dist/fmt_logging.h>
#include <dsn/cpp/message_utils.h>

#include "replica_duplicator.h"
#include "mutation_batch.h"
#include "replica/prepare_list.h"

namespace dsn {
namespace replication {

/*static*/ constexpr int64_t mutation_batch::PREPARE_LIST_NUM_ENTRIES;

error_s mutation_batch::add(mutation_ptr mu)
{
    if (mu->get_decree() <= _mutation_buffer->last_committed_decree()) {
        // ignore
        return error_s::ok();
    }

    auto old = _mutation_buffer->get_mutation_by_decree(mu->get_decree());
    if (old != nullptr && old->data.header.ballot >= mu->data.header.ballot) {
        // ignore
        return error_s::ok();
    }

    error_code ec = _mutation_buffer->prepare(mu, partition_status::PS_INACTIVE);
    if (ec != ERR_OK) {
        return FMT_ERR(
            ERR_INVALID_DATA,
            "failed to add mutation [err:{}, logged:{}, decree:{}, committed:{}, start_decree:{}]",
            ec.to_string(),
            mu->is_logged(),
            mu->get_decree(),
            mu->data.header.last_committed_decree,
            _start_decree);
    }

    return error_s::ok();
}

decree mutation_batch::last_decree() const { return _mutation_buffer->last_committed_decree(); }

void mutation_batch::set_start_decree(decree d) { _start_decree = d; }

mutation_tuple_set mutation_batch::move_all_mutations()
{
    // free the internal space
    _mutation_buffer->truncate(last_decree());
    return std::move(_loaded_mutations);
}

mutation_batch::mutation_batch(replica_duplicator *r) : replica_base(r)
{
    // Prepend a special tag identifying this is a mutation_batch,
    // so `dxxx_replica` logging in prepare_list will print along with its real caller.
    // This helps for debugging.
    replica_base base(
        r->get_gpid(), std::string("mutation_batch@") + r->replica_name(), r->app_name());
    _mutation_buffer =
        make_unique<prepare_list>(&base, 0, PREPARE_LIST_NUM_ENTRIES, [this](mutation_ptr &mu) {
            // committer
            add_mutation_if_valid(mu, _loaded_mutations, _start_decree);
        });

    // start duplication from confirmed_decree
    _mutation_buffer->reset(r->progress().confirmed_decree);
}

/*extern*/ void
add_mutation_if_valid(mutation_ptr &mu, mutation_tuple_set &mutations, decree start_decree)
{
    if (mu->get_decree() < start_decree) {
        // ignore
        return;
    }
    for (mutation_update &update : mu->data.updates) {
        // ignore WRITE_EMPTY
        if (update.code == RPC_REPLICATION_WRITE_EMPTY) {
            continue;
        }
        // Ignore non-idempotent writes.
        // Normally a duplicating replica will reply non-idempotent writes with
        // ERR_OPERATION_DISABLED, but there could still be a mutation written
        // before the duplication was added.
        // To ignore means this write will be lost, which is acceptable under this rare case.
        if (!task_spec::get(update.code)->rpc_request_is_write_idempotent) {
            continue;
        }
        blob bb;
        if (update.data.buffer() != nullptr) {
            bb = std::move(update.data);
        } else {
            bb = blob::create_from_bytes(update.data.data(), update.data.length());
        }

        mutations.emplace(std::make_tuple(mu->data.header.timestamp, update.code, std::move(bb)));
    }
}

} // namespace replication
} // namespace dsn
