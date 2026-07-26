#include <memory>
#include <string>

#include "dfd1_loader.h"
#include "px/core/result.h"
#include "px/module/navdata/nav_data_loader.h"

namespace px {

Result<std::unique_ptr<NavDataLoader>> MakeLoader(
    const std::string& name) {
  if (name == "dfd1") {
    return Ok(std::unique_ptr<NavDataLoader>(new Dfd1Loader()));
  }
  return Err(
      Error(ErrorCode::kInvalidArgument, "unknown loader: " + name));
}

}  // namespace px
