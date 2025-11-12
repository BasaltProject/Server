#pragma once

#include <string_view>

namespace basalt::server::storage {

/// The whole schema, in the order the tables were written. Kept as one statement per table with its
/// indexes so a reader can see what a query is indexed on without opening another file.
///
/// There are no migrations on purpose: nobody is running Basalt yet, so a schema change deletes the
/// old file rather than growing a migration path that has never been exercised.
namespace schema {

[[nodiscard]] std::string_view ddl();

} // namespace schema

} // namespace basalt::server::storage
