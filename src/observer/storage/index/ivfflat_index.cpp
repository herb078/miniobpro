/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "storage/index/ivfflat_index.h"

#include <algorithm>
#include <cmath>
#include <strings.h>

#include "common/log/log.h"
#include "storage/record/record_scanner.h"
#include "storage/table/table.h"

using namespace std;

RC IvfflatIndex::create(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)
{
  if (inited_) {
    return RC::RECORD_OPENNED;
  }
  if (table == nullptr || !index_meta.is_vector() || field_meta.type() != AttrType::VECTORS) {
    return RC::INVALID_ARGUMENT;
  }

  Index::init(index_meta, field_meta);
  table_         = table;
  lists_         = max(1, index_meta.lists());
  probes_        = max(1, index_meta.probes());
  dimension_     = field_meta.len() / static_cast<int>(sizeof(float));
  distance_type_ = index_meta.distance_type();
  inited_        = true;
  return RC::SUCCESS;
}

RC IvfflatIndex::open(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)
{
  RC rc = create(table, file_name, index_meta, field_meta);
  if (OB_FAIL(rc)) {
    return rc;
  }

  RecordScanner *scanner = nullptr;
  rc = table_->get_record_scanner(scanner, nullptr, ReadWriteMode::READ_ONLY);
  if (OB_FAIL(rc)) {
    return rc;
  }

  Record record;
  while (OB_SUCC(rc = scanner->next(record))) {
    rc = insert_entry(record.data(), &record.rid());
    if (OB_FAIL(rc)) {
      scanner->close_scan();
      delete scanner;
      return rc;
    }
  }
  scanner->close_scan();
  delete scanner;

  if (rc == RC::RECORD_EOF) {
    return rebuild();
  }
  return rc;
}

RC IvfflatIndex::close()
{
  entries_.clear();
  centroids_.clear();
  inverted_lists_.clear();
  inited_ = false;
  table_  = nullptr;
  return RC::SUCCESS;
}

vector<float> IvfflatIndex::read_vector(const char *record) const
{
  vector<float> values(dimension_);
  const float  *data = reinterpret_cast<const float *>(record + field_meta_.offset());
  for (int i = 0; i < dimension_; i++) {
    values[i] = data[i];
  }
  return values;
}

RC IvfflatIndex::insert_entry(const char *record, const RID *rid)
{
  if (!inited_ || record == nullptr || rid == nullptr) {
    return RC::INVALID_ARGUMENT;
  }

  Entry entry;
  entry.rid    = *rid;
  entry.values = read_vector(record);
  if (!centroids_.empty()) {
    entry.list_id = nearest_centroid(entry.values);
  }
  entries_.emplace_back(std::move(entry));

  if (centroids_.empty() || static_cast<int>(entries_.size()) <= lists_) {
    return rebuild();
  }

  if (entries_.back().list_id >= 0 && entries_.back().list_id < static_cast<int>(inverted_lists_.size())) {
    inverted_lists_[entries_.back().list_id].push_back(static_cast<int>(entries_.size()) - 1);
  }
  return RC::SUCCESS;
}

RC IvfflatIndex::delete_entry(const char *record, const RID *rid)
{
  if (rid == nullptr) {
    return RC::INVALID_ARGUMENT;
  }

  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->rid == *rid) {
      entries_.erase(it);
      return rebuild();
    }
  }
  return RC::RECORD_INVALID_KEY;
}

float IvfflatIndex::distance(const vector<float> &left, const vector<float> &right) const
{
  if (0 == strcasecmp(distance_type_.c_str(), "COSINE") || 0 == strcasecmp(distance_type_.c_str(), "COSINE_DISTANCE")) {
    float dot = 0.0F;
    float left_norm = 0.0F;
    float right_norm = 0.0F;
    for (int i = 0; i < dimension_; i++) {
      dot += left[i] * right[i];
      left_norm += left[i] * left[i];
      right_norm += right[i] * right[i];
    }
    if (left_norm <= 0.0F || right_norm <= 0.0F) {
      return left_norm <= 0.0F && right_norm <= 0.0F ? 0.0F : 1.0F;
    }
    return 1.0F - dot / (sqrtf(left_norm) * sqrtf(right_norm));
  }

  if (0 == strcasecmp(distance_type_.c_str(), "DOT") || 0 == strcasecmp(distance_type_.c_str(), "INNER_PRODUCT")) {
    float dot = 0.0F;
    for (int i = 0; i < dimension_; i++) {
      dot += left[i] * right[i];
    }
    return dot;
  }

  float sum = 0.0F;
  for (int i = 0; i < dimension_; i++) {
    const float diff = left[i] - right[i];
    sum += diff * diff;
  }
  return sqrtf(sum);
}

int IvfflatIndex::nearest_centroid(const vector<float> &values) const
{
  int   best_id       = -1;
  float best_distance = 0.0F;
  for (int i = 0; i < static_cast<int>(centroids_.size()); i++) {
    const float current_distance = distance(values, centroids_[i]);
    if (best_id < 0 || current_distance < best_distance) {
      best_id       = i;
      best_distance = current_distance;
    }
  }
  return best_id;
}

RC IvfflatIndex::rebuild()
{
  centroids_.clear();
  inverted_lists_.clear();
  if (entries_.empty()) {
    return RC::SUCCESS;
  }

  const int cluster_count = min(lists_, static_cast<int>(entries_.size()));
  centroids_.reserve(cluster_count);
  for (int i = 0; i < cluster_count; i++) {
    centroids_.push_back(entries_[i].values);
  }

  vector<int> assignments(entries_.size(), 0);
  for (int iter = 0; iter < 20; iter++) {
    bool changed = false;
    for (int i = 0; i < static_cast<int>(entries_.size()); i++) {
      int list_id = nearest_centroid(entries_[i].values);
      if (list_id < 0) {
        list_id = 0;
      }
      if (assignments[i] != list_id) {
        assignments[i] = list_id;
        changed        = true;
      }
    }

    vector<vector<float>> new_centroids(cluster_count, vector<float>(dimension_, 0.0F));
    vector<int>           counts(cluster_count, 0);
    for (int i = 0; i < static_cast<int>(entries_.size()); i++) {
      const int list_id = assignments[i];
      counts[list_id]++;
      for (int d = 0; d < dimension_; d++) {
        new_centroids[list_id][d] += entries_[i].values[d];
      }
    }
    for (int i = 0; i < cluster_count; i++) {
      if (counts[i] == 0) {
        new_centroids[i] = entries_[i % entries_.size()].values;
      } else {
        for (int d = 0; d < dimension_; d++) {
          new_centroids[i][d] /= counts[i];
        }
      }
    }
    centroids_.swap(new_centroids);
    if (!changed) {
      break;
    }
  }

  for (Entry &entry : entries_) {
    entry.list_id = nearest_centroid(entry.values);
  }
  rebuild_inverted_lists();
  return RC::SUCCESS;
}

void IvfflatIndex::rebuild_inverted_lists()
{
  inverted_lists_.assign(centroids_.size(), vector<int>());
  for (int i = 0; i < static_cast<int>(entries_.size()); i++) {
    int list_id = entries_[i].list_id;
    if (list_id < 0 || list_id >= static_cast<int>(inverted_lists_.size())) {
      list_id = 0;
    }
    inverted_lists_[list_id].push_back(i);
  }
}

vector<RID> IvfflatIndex::ann_search(const vector<float> &base_vector, size_t limit)
{
  vector<RID> result;
  if (base_vector.size() != static_cast<size_t>(dimension_) || entries_.empty()) {
    return result;
  }
  if (centroids_.empty()) {
    rebuild();
  }

  vector<pair<float, int>> centroid_distances;
  centroid_distances.reserve(centroids_.size());
  for (int i = 0; i < static_cast<int>(centroids_.size()); i++) {
    centroid_distances.emplace_back(distance(base_vector, centroids_[i]), i);
  }
  sort(centroid_distances.begin(), centroid_distances.end());

  vector<pair<float, RID>> candidates;
  const int probe_count = min(probes_, static_cast<int>(centroid_distances.size()));
  for (int i = 0; i < probe_count; i++) {
    const int list_id = centroid_distances[i].second;
    for (int entry_id : inverted_lists_[list_id]) {
      const Entry &entry = entries_[entry_id];
      candidates.emplace_back(distance(base_vector, entry.values), entry.rid);
    }
  }

  sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
    if (left.first != right.first) {
      return left.first < right.first;
    }
    return RID::compare(&left.second, &right.second) < 0;
  });

  const size_t output_count = limit == 0 ? candidates.size() : min(limit, candidates.size());
  result.reserve(output_count);
  for (size_t i = 0; i < output_count; i++) {
    result.emplace_back(candidates[i].second);
  }
  return result;
}
