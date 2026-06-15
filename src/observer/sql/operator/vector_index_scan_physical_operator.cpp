/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/vector_index_scan_physical_operator.h"

#include "storage/index/ivfflat_index.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

using namespace std;

VectorIndexScanPhysicalOperator::VectorIndexScanPhysicalOperator(Table *table, IvfflatIndex *index,
    vector<float> &&query_vector, size_t limit, ReadWriteMode mode)
    : table_(table), index_(index), query_vector_(std::move(query_vector)), limit_(limit), mode_(mode)
{}

RC VectorIndexScanPhysicalOperator::open(Trx *trx)
{
  if (table_ == nullptr || index_ == nullptr) {
    return RC::INVALID_ARGUMENT;
  }
  rids_       = index_->ann_search(query_vector_, limit_);
  next_index_ = 0;
  tuple_.set_schema(table_, table_->table_meta().field_metas());
  trx_ = trx;
  return RC::SUCCESS;
}

RC VectorIndexScanPhysicalOperator::next()
{
  RC rc = RC::SUCCESS;
  while (next_index_ < rids_.size()) {
    const RID rid = rids_[next_index_++];
    rc = table_->get_record(rid, current_record_);
    if (OB_FAIL(rc)) {
      return rc;
    }

    tuple_.set_record(&current_record_);
    bool filter_result = false;
    rc = filter(tuple_, filter_result);
    if (OB_FAIL(rc)) {
      return rc;
    }
    if (!filter_result) {
      continue;
    }

    if (trx_ != nullptr) {
      rc = trx_->visit_record(table_, current_record_, mode_);
      if (rc == RC::RECORD_INVISIBLE) {
        continue;
      }
    }
    return rc;
  }
  return RC::RECORD_EOF;
}

RC VectorIndexScanPhysicalOperator::close()
{
  rids_.clear();
  next_index_ = 0;
  return RC::SUCCESS;
}

Tuple *VectorIndexScanPhysicalOperator::current_tuple()
{
  tuple_.set_record(&current_record_);
  return &tuple_;
}

void VectorIndexScanPhysicalOperator::set_predicates(vector<unique_ptr<Expression>> &&exprs)
{
  predicates_ = std::move(exprs);
}

RC VectorIndexScanPhysicalOperator::filter(RowTuple &tuple, bool &result)
{
  RC    rc = RC::SUCCESS;
  Value value;
  for (unique_ptr<Expression> &expr : predicates_) {
    rc = expr->get_value(tuple, value);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    if (!value.get_boolean()) {
      result = false;
      return rc;
    }
  }
  result = true;
  return rc;
}

string VectorIndexScanPhysicalOperator::param() const
{
  return string(index_->index_meta().name()) + " ON " + table_->name();
}
