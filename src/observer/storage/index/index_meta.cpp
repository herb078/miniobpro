/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai.wyl on 2021/5/18.
//

#include "storage/index/index_meta.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "storage/field/field_meta.h"
#include "storage/table/table_meta.h"
#include "json/json.h"

const static Json::StaticString FIELD_NAME("name");
const static Json::StaticString FIELD_FIELD_NAME("field_name");
const static Json::StaticString FIELD_IS_VECTOR("is_vector");
const static Json::StaticString FIELD_INDEX_TYPE("index_type");
const static Json::StaticString FIELD_DISTANCE_TYPE("distance_type");
const static Json::StaticString FIELD_LISTS("lists");
const static Json::StaticString FIELD_PROBES("probes");

RC IndexMeta::init(const char *name, const FieldMeta &field)
{
  if (common::is_blank(name)) {
    LOG_ERROR("Failed to init index, name is empty.");
    return RC::INVALID_ARGUMENT;
  }

  name_  = name;
  field_ = field.name();
  return RC::SUCCESS;
}

RC IndexMeta::init_vector(
    const char *name, const FieldMeta &field, const char *index_type, const char *distance_type, int lists, int probes)
{
  RC rc = init(name, field);
  if (OB_FAIL(rc)) {
    return rc;
  }
  if (field.type() != AttrType::VECTORS || common::is_blank(index_type) || common::is_blank(distance_type) ||
      lists <= 0 || probes <= 0) {
    LOG_ERROR("Failed to init vector index meta. name=%s, field=%s, type=%s, distance=%s, lists=%d, probes=%d",
        name, field.name(), index_type, distance_type, lists, probes);
    return RC::INVALID_ARGUMENT;
  }

  is_vector_     = true;
  index_type_    = index_type;
  distance_type_ = distance_type;
  lists_         = lists;
  probes_        = probes;
  return RC::SUCCESS;
}

void IndexMeta::to_json(Json::Value &json_value) const
{
  json_value[FIELD_NAME]       = name_;
  json_value[FIELD_FIELD_NAME] = field_;
  json_value[FIELD_IS_VECTOR]  = is_vector_;
  if (is_vector_) {
    json_value[FIELD_INDEX_TYPE]    = index_type_;
    json_value[FIELD_DISTANCE_TYPE] = distance_type_;
    json_value[FIELD_LISTS]         = lists_;
    json_value[FIELD_PROBES]        = probes_;
  }
}

RC IndexMeta::from_json(const TableMeta &table, const Json::Value &json_value, IndexMeta &index)
{
  const Json::Value &name_value  = json_value[FIELD_NAME];
  const Json::Value &field_value = json_value[FIELD_FIELD_NAME];
  if (!name_value.isString()) {
    LOG_ERROR("Index name is not a string. json value=%s", name_value.toStyledString().c_str());
    return RC::INTERNAL;
  }

  if (!field_value.isString()) {
    LOG_ERROR("Field name of index [%s] is not a string. json value=%s",
        name_value.asCString(), field_value.toStyledString().c_str());
    return RC::INTERNAL;
  }

  const FieldMeta *field = table.field(field_value.asCString());
  if (nullptr == field) {
    LOG_ERROR("Deserialize index [%s]: no such field: %s", name_value.asCString(), field_value.asCString());
    return RC::SCHEMA_FIELD_MISSING;
  }

  const Json::Value &is_vector_value = json_value[FIELD_IS_VECTOR];
  const bool         is_vector       = is_vector_value.isBool() && is_vector_value.asBool();
  if (!is_vector) {
    return index.init(name_value.asCString(), *field);
  }

  const Json::Value &index_type_value    = json_value[FIELD_INDEX_TYPE];
  const Json::Value &distance_type_value = json_value[FIELD_DISTANCE_TYPE];
  const Json::Value &lists_value         = json_value[FIELD_LISTS];
  const Json::Value &probes_value        = json_value[FIELD_PROBES];
  if (!index_type_value.isString() || !distance_type_value.isString() || !lists_value.isInt() ||
      !probes_value.isInt()) {
    LOG_ERROR("Invalid vector index meta. json value=%s", json_value.toStyledString().c_str());
    return RC::INTERNAL;
  }

  return index.init_vector(name_value.asCString(),
      *field,
      index_type_value.asCString(),
      distance_type_value.asCString(),
      lists_value.asInt(),
      probes_value.asInt());
}

const char *IndexMeta::name() const { return name_.c_str(); }

const char *IndexMeta::field() const { return field_.c_str(); }

void IndexMeta::desc(ostream &os) const
{
  os << "index name=" << name_ << ", field=" << field_;
  if (is_vector_) {
    os << ", type=" << index_type_ << ", distance=" << distance_type_ << ", lists=" << lists_
       << ", probes=" << probes_;
  }
}
