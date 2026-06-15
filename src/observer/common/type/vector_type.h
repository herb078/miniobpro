/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "common/type/data_type.h"
#include "common/value.h"

#include <cstring>
#include <sstream>

/**
 * @brief 向量类型
 * @ingroup DataType
 */
class VectorType : public DataType
{
public:
  VectorType() : DataType(AttrType::VECTORS) {}
  virtual ~VectorType() {}

  int compare(const Value &left, const Value &right) const override
  {
    if (left.attr_type() != AttrType::VECTORS || right.attr_type() != AttrType::VECTORS) {
      return INT32_MAX;
    }
    if (left.length() != right.length()) {
      return left.length() < right.length() ? -1 : 1;
    }
    int ret = memcmp(left.data(), right.data(), left.length());
    return ret == 0 ? 0 : (ret < 0 ? -1 : 1);
  }

  RC add(const Value &left, const Value &right, Value &result) const override { return RC::UNIMPLEMENTED; }
  RC subtract(const Value &left, const Value &right, Value &result) const override { return RC::UNIMPLEMENTED; }
  RC multiply(const Value &left, const Value &right, Value &result) const override { return RC::UNIMPLEMENTED; }

  RC to_string(const Value &val, string &result) const override
  {
    if (val.attr_type() != AttrType::VECTORS || val.length() % static_cast<int>(sizeof(float)) != 0) {
      return RC::INVALID_ARGUMENT;
    }

    std::stringstream ss;
    ss << "[";
    const int    dimension = val.length() / static_cast<int>(sizeof(float));
    const float *data      = reinterpret_cast<const float *>(val.data());
    for (int i = 0; i < dimension; i++) {
      if (i > 0) {
        ss << ",";
      }
      ss << data[i];
    }
    ss << "]";
    result = ss.str();
    return RC::SUCCESS;
  }
};
