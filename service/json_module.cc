// SPDX-License-Identifier: MIT
#include "px/service/json_module.h"

#include <algorithm>

namespace px {

void JsonModuleRegistry::Add(std::unique_ptr<JsonModule> module) {
  // 同名覆盖：找到已有同名模块直接替换。
  auto it = std::find_if(modules_.begin(), modules_.end(), [&](const auto& m) {
    return std::strcmp(m->Name(), module->Name()) == 0;
  });
  if (it != modules_.end()) {
    *it = std::move(module);
    return;
  }
  modules_.push_back(std::move(module));
}

void JsonModuleRegistry::Compose(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const std::vector<std::string>& names, const JsonContext& ctx) const {
  for (const auto& name : names) {
    // 按 names 顺序查找同名模块。
    auto it =
        std::find_if(modules_.begin(), modules_.end(), [&](const auto& m) {
          return std::strcmp(m->Name(), name.c_str()) == 0;
        });
    if (it == modules_.end()) continue;

    if (!(*it)->WriteFields(writer, ctx)) continue;
  }
}

}  // namespace px
