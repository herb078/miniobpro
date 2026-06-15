/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "sql/operator/physical_operator.h"

class SortPhysicalOperator : public PhysicalOperator
{
public:
  SortPhysicalOperator(vector<unique_ptr<Expression>> &&order_by_exprs);
  virtual ~SortPhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::SORT; }
  OpType               get_op_type() const override { return OpType::ORDERBY; }
  string               name() const override { return "SORT"; }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override { return current_tuple_; }
  RC     tuple_schema(TupleSchema &schema) const override;

private:
  struct SortItem
  {
    unique_ptr<ValueListTuple> tuple;
    vector<Value>             keys;
    size_t                    sequence = 0;
  };

private:
  vector<unique_ptr<Expression>> order_by_exprs_;
  vector<SortItem>               items_;
  size_t                         next_index_    = 0;
  Tuple                         *current_tuple_ = nullptr;
};
