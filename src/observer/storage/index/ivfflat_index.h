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

#include "common/lang/vector.h"
#include "storage/index/index.h"

/**
 * @brief ivfflat 向量索引
 * @ingroup Index
 */
class IvfflatIndex : public Index
{
public:
  IvfflatIndex(){};
  virtual ~IvfflatIndex() noexcept {};

  RC create(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta) override;
  RC open(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta) override;

  bool is_vector_index() override { return true; }

  vector<RID> ann_search(const vector<float> &base_vector, size_t limit);

  RC rebuild();
  RC close();

  RC insert_entry(const char *record, const RID *rid) override;
  RC delete_entry(const char *record, const RID *rid) override;

  IndexScanner *create_scanner(const char *left_key, int left_len, bool left_inclusive, const char *right_key,
      int right_len, bool right_inclusive) override
  {
    return nullptr;
  }

  RC sync() override { return RC::SUCCESS; };

private:
  struct Entry
  {
    RID           rid;
    vector<float> values;
    int           list_id = -1;
  };

private:
  vector<float> read_vector(const char *record) const;
  float         distance(const vector<float> &left, const vector<float> &right) const;
  int           nearest_centroid(const vector<float> &values) const;
  void          rebuild_inverted_lists();

private:
  bool                  inited_    = false;
  Table                *table_     = nullptr;
  int                   lists_     = 1;
  int                   probes_    = 1;
  int                   dimension_ = 0;
  string                distance_type_;
  vector<Entry>         entries_;
  vector<vector<float>> centroids_;
  vector<vector<int>>   inverted_lists_;
};
