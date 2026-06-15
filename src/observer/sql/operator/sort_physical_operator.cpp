/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/sort_physical_operator.h"
#include "common/log/log.h"

#include <algorithm>
#include <strings.h>

using namespace std;

SortPhysicalOperator::SortPhysicalOperator(vector<unique_ptr<Expression>> &&order_by_exprs)
    : order_by_exprs_(std::move(order_by_exprs))
{}

RC SortPhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::INVALID_ARGUMENT;
  }

  PhysicalOperator *child = children_[0].get();
  RC                rc    = child->open(trx);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to open child operator. rc=%s", strrc(rc));
    return rc;
  }

  size_t sequence = 0;
  while (RC::SUCCESS == (rc = child->next())) {
    Tuple *child_tuple = child->current_tuple();
    if (nullptr == child_tuple) {
      return RC::INTERNAL;
    }

    auto value_tuple = make_unique<ValueListTuple>();
    rc = ValueListTuple::make(*child_tuple, *value_tuple);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to materialize tuple. rc=%s", strrc(rc));
      return rc;
    }

    SortItem item;
    item.tuple    = std::move(value_tuple);
    item.sequence = sequence++;
    item.keys.reserve(order_by_exprs_.size());
    for (const unique_ptr<Expression> &expr : order_by_exprs_) {
      Value key;
      rc = expr->get_value(*item.tuple, key);
      if (OB_FAIL(rc)) {
        LOG_WARN("failed to get sort key. rc=%s", strrc(rc));
        return rc;
      }
      item.keys.emplace_back(std::move(key));
    }
    items_.emplace_back(std::move(item));
  }

  if (rc != RC::RECORD_EOF) {
    LOG_WARN("failed to iterate child operator. rc=%s", strrc(rc));
    return rc;
  }

  stable_sort(items_.begin(), items_.end(), [](const SortItem &left, const SortItem &right) {
    const size_t key_num = min(left.keys.size(), right.keys.size());
    for (size_t i = 0; i < key_num; i++) {
      const int cmp = left.keys[i].compare(right.keys[i]);
      if (cmp != 0) {
        return cmp < 0;
      }
    }
    return left.sequence < right.sequence;
  });

  next_index_    = 0;
  current_tuple_ = nullptr;
  return RC::SUCCESS;
}

RC SortPhysicalOperator::next()
{
  if (next_index_ >= items_.size()) {
    current_tuple_ = nullptr;
    return RC::RECORD_EOF;
  }

  current_tuple_ = items_[next_index_].tuple.get();
  next_index_++;
  return RC::SUCCESS;
}

RC SortPhysicalOperator::close()
{
  RC rc = RC::SUCCESS;
  if (!children_.empty()) {
    rc = children_[0]->close();
  }
  items_.clear();
  next_index_    = 0;
  current_tuple_ = nullptr;
  return rc;
}

RC SortPhysicalOperator::tuple_schema(TupleSchema &schema) const
{
  if (children_.empty()) {
    return RC::INVALID_ARGUMENT;
  }
  return children_[0]->tuple_schema(schema);
}
