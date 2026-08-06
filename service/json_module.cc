// SPDX-License-Identifier: MIT
#include "px/service/json_module.h"

#include <algorithm>
#include <cstring>

namespace px {

void JsonModuleRegistry::Add(std::unique_ptr<JsonModule> module) {
  // 同名覆盖：找到已有同名模块直接替换。
  auto it = std::find_if(modules_.begin(), modules_.end(), [&](const auto& m) {
    return std::strcmp(module->Name(), m->Name()) == 0;
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
  std::vector<std::string> emitted;  // 已输出名称——重复 key 会被解析器吞掉。
  for (const auto& name : names) {
    if (std::find(emitted.begin(), emitted.end(), name) != emitted.end()) {
      continue;
    }
    // 按 names 顺序查找同名模块。
    auto it = std::find_if(modules_.begin(), modules_.end(),
                           [&](const auto& m) { return name == m->Name(); });
    if (it == modules_.end()) continue;

    if (!(*it)->WriteFields(writer, ctx)) continue;
    emitted.push_back(name);
  }
}

}  // namespace px
