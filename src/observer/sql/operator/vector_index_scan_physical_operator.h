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

#include "common/types.h"
#include "sql/operator/physical_operator.h"
#include "storage/record/record_manager.h"

class IvfflatIndex;
class Table;

class VectorIndexScanPhysicalOperator : public PhysicalOperator
{
public:
  VectorIndexScanPhysicalOperator(Table *table, IvfflatIndex *index, vector<float> &&query_vector, size_t limit,
      ReadWriteMode mode);
  virtual ~VectorIndexScanPhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::VECTOR_INDEX_SCAN; }
  OpType               get_op_type() const override { return OpType::SEQSCAN; }
  string               param() const override;

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override;

  void set_predicates(vector<unique_ptr<Expression>> &&exprs);

private:
  RC filter(RowTuple &tuple, bool &result);

private:
  Table                         *table_ = nullptr;
  IvfflatIndex                  *index_ = nullptr;
  vector<float>                  query_vector_;
  size_t                         limit_ = 0;
  ReadWriteMode                  mode_  = ReadWriteMode::READ_ONLY;
  Trx                           *trx_   = nullptr;
  vector<RID>                    rids_;
  size_t                         next_index_ = 0;
  Record                         current_record_;
  RowTuple                       tuple_;
  vector<unique_ptr<Expression>> predicates_;
};
