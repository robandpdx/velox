/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "velox/common/base/tests/GTestUtils.h"
#include "velox/type/parser/TypeParser.h"

namespace facebook::velox {
namespace {

class CustomType : public VarcharType {
 public:
  CustomType() = default;

  bool equivalent(const Type& other) const override {
    // Pointer comparison works since this type is a singleton.
    return this == &other;
  }
};

static const TypePtr& JSON() {
  static const TypePtr instance{new CustomType()};
  return instance;
}

static const TypePtr& TIMESTAMP_WITH_TIME_ZONE() {
  static const TypePtr instance{new CustomType()};
  return instance;
}

static const TypePtr& TIMESTAMP_WITHOUT_TIME_ZONE() {
  static const TypePtr instance{new CustomType()};
  return instance;
}

class TypeFactories : public CustomTypeFactories {
 public:
  TypeFactories(const TypePtr& type) : type_(type) {}

  TypePtr getType() const override {
    return type_;
  }

  exec::CastOperatorPtr getCastOperator() const override {
    return nullptr;
  }

 private:
  TypePtr type_;
};

class TestTypeSignature : public ::testing::Test {
 private:
  void SetUp() override {
    // Register custom types with and without spaces.
    // Does not need any parser support.
    registerCustomType("json", std::make_unique<const TypeFactories>(JSON()));
    // Needs and has parser support.
    registerCustomType(
        "timestamp with time zone",
        std::make_unique<const TypeFactories>(TIMESTAMP_WITH_TIME_ZONE()));
    // Needs and does not have parser support.
    registerCustomType(
        "timestamp without time zone",
        std::make_unique<const TypeFactories>(TIMESTAMP_WITHOUT_TIME_ZONE()));
  }
};

TEST_F(TestTypeSignature, booleanType) {
  ASSERT_EQ(*parseType("boolean"), *BOOLEAN());
}

TEST_F(TestTypeSignature, integerType) {
  ASSERT_EQ(*parseType("int"), *INTEGER());
  ASSERT_EQ(*parseType("integer"), *INTEGER());
}

TEST_F(TestTypeSignature, varcharType) {
  ASSERT_EQ(*parseType("varchar"), *VARCHAR());
  ASSERT_EQ(*parseType("varchar(4)"), *VARCHAR());
}

TEST_F(TestTypeSignature, varbinary) {
  ASSERT_EQ(*parseType("varbinary"), *VARBINARY());
}

TEST_F(TestTypeSignature, arrayType) {
  ASSERT_EQ(*parseType("array(bigint)"), *ARRAY(BIGINT()));

  ASSERT_EQ(*parseType("array(int)"), *ARRAY(INTEGER()));
  ASSERT_EQ(*parseType("array(integer)"), *ARRAY(INTEGER()));

  ASSERT_EQ(*parseType("array(array(bigint))"), *ARRAY(ARRAY(BIGINT())));

  ASSERT_EQ(*parseType("array(array(int))"), *ARRAY(ARRAY(INTEGER())));
}

TEST_F(TestTypeSignature, mapType) {
  ASSERT_EQ(*parseType("map(bigint,bigint)"), *MAP(BIGINT(), BIGINT()));

  ASSERT_EQ(
      *parseType("map(bigint,array(bigint))"), *MAP(BIGINT(), ARRAY(BIGINT())));

  ASSERT_EQ(
      *parseType("map(bigint,map(bigint,map(varchar,bigint)))"),
      *MAP(BIGINT(), MAP(BIGINT(), MAP(VARCHAR(), BIGINT()))));
}

TEST_F(TestTypeSignature, invalidType) {
  VELOX_ASSERT_THROW(parseType("blah()"), "Failed to parse type [blah()]");

  VELOX_ASSERT_THROW(parseType("array()"), "Failed to parse type [array()]");

  VELOX_ASSERT_THROW(parseType("map()"), "Failed to parse type [map()]");

  VELOX_ASSERT_THROW(parseType("x"), "Failed to parse type [x]");

  // Ensure this is not treated as a row type.
  VELOX_ASSERT_THROW(
      parseType("rowxxx(a)"), "Failed to parse type [rowxxx(a)]");
}

TEST_F(TestTypeSignature, rowType) {
  ASSERT_EQ(
      *parseType("row(a bigint,b varchar,c real)"),
      *ROW({"a", "b", "c"}, {BIGINT(), VARCHAR(), REAL()}));

  ASSERT_EQ(
      *parseType("row(a bigint,b array(bigint),c row(a bigint))"),
      *ROW(
          {"a", "b", "c"},
          {BIGINT(), ARRAY(BIGINT()), ROW({"a"}, {BIGINT()})}));

  ASSERT_EQ(
      *parseType("row(\"12 tb\" bigint,b bigint,c bigint)"),
      *ROW({"12 tb", "b", "c"}, {BIGINT(), BIGINT(), BIGINT()}));

  ASSERT_EQ(
      *parseType("row(a varchar(10),b row(a bigint))"),
      *ROW({"a", "b"}, {VARCHAR(), ROW({"a"}, {BIGINT()})}));

  ASSERT_EQ(
      *parseType("array(row(col0 bigint,col1 double))"),
      *ARRAY(ROW({"col0", "col1"}, {BIGINT(), DOUBLE()})));

  ASSERT_EQ(
      *parseType("row(col0 array(row(col0 bigint,col1 double)))"),
      *ROW({"col0"}, {ARRAY(ROW({"col0", "col1"}, {BIGINT(), DOUBLE()}))}));

  ASSERT_EQ(*parseType("row(bigint,varchar)"), *ROW({BIGINT(), VARCHAR()}));

  ASSERT_EQ(
      *parseType("row(bigint,array(bigint),row(a bigint))"),
      *ROW({BIGINT(), ARRAY(BIGINT()), ROW({"a"}, {BIGINT()})}));

  ASSERT_EQ(
      *parseType("row(varchar(10),b row(bigint))"),
      *ROW({"", "b"}, {VARCHAR(), ROW({BIGINT()})}));

  ASSERT_EQ(
      *parseType("array(row(col0 bigint,double))"),
      *ARRAY(ROW({"col0", ""}, {BIGINT(), DOUBLE()})));

  ASSERT_EQ(
      *parseType("row(col0 array(row(bigint,double)))"),
      *ROW({"col0"}, {ARRAY(ROW({BIGINT(), DOUBLE()}))}));

  ASSERT_EQ(
      *parseType("row(double double precision)"), *ROW({"double"}, {DOUBLE()}));

  ASSERT_EQ(*parseType("row(double precision)"), *ROW({DOUBLE()}));

  ASSERT_EQ(
      *parseType("RoW(a bigint,b varchar)"),
      *ROW({"a", "b"}, {BIGINT(), VARCHAR()}));

  ASSERT_EQ(*parseType("row(array(Json))"), *ROW({ARRAY(JSON())}));

  VELOX_ASSERT_THROW(
      *parseType("row(col0 row(array(HyperLogLog)))"),
      "Failed to parse type [HyperLogLog]. Type not registered.");

  // Field type canonicalization.
  ASSERT_EQ(*parseType("row(col iNt)"), *ROW({"col"}, {INTEGER()}));
}

TEST_F(TestTypeSignature, typesWithSpaces) {
  // Type is handled by the parser but is not registered.
  VELOX_ASSERT_THROW(
      parseType("row(time time with time zone)"),
      "Failed to parse type [time with time zone]. Type not registered.");

  // Type is not handled by the parser but is registered.
  VELOX_ASSERT_THROW(
      parseType("row(col0 timestamp without time zone)"),
      "Failed to parse type [row(col0 timestamp without time zone)]");

  ASSERT_EQ(
      *parseType("row(double double precision)"), *ROW({"double"}, {DOUBLE()}));

  VELOX_ASSERT_THROW(
      parseType("row(time with time zone)"),
      "Failed to parse type [time with time zone]");

  ASSERT_EQ(*parseType("row(double precision)"), *ROW({DOUBLE()}));

  ASSERT_EQ(
      *parseType("row(INTERval DAY TO SECOND)"), *ROW({INTERVAL_DAY_TIME()}));

  ASSERT_EQ(
      *parseType("row(INTERVAL YEAR TO month)"), *ROW({INTERVAL_YEAR_MONTH()}));

  // quoted field names
  ASSERT_EQ(
      *parseType(
          "row(\"timestamp with time zone\" timestamp with time zone,\"double\" double)"),
      *ROW(
          {"timestamp with time zone", "double"},
          {TIMESTAMP_WITH_TIME_ZONE(), DOUBLE()}));
}

TEST_F(TestTypeSignature, intervalYearToMonthType) {
  ASSERT_EQ(
      *parseType("row(interval interval year to month)"),
      *ROW({"interval"}, {INTERVAL_YEAR_MONTH()}));

  ASSERT_EQ(
      *parseType("row(interval year to month)"), *ROW({INTERVAL_YEAR_MONTH()}));
}

TEST_F(TestTypeSignature, functionType) {
  ASSERT_EQ(
      *parseType("function(bigint,bigint,bigint)"),
      *FUNCTION({BIGINT(), BIGINT()}, BIGINT()));
  ASSERT_EQ(
      *parseType("function(bigint,array(varchar),varchar)"),
      *FUNCTION({BIGINT(), ARRAY(VARCHAR())}, VARCHAR()));
}

TEST_F(TestTypeSignature, decimalType) {
  ASSERT_EQ(*parseType("decimal(10, 5)"), *DECIMAL(10, 5));
  ASSERT_EQ(*parseType("decimal(20,10)"), *DECIMAL(20, 10));

  VELOX_ASSERT_THROW(parseType("decimal"), "Failed to parse type [decimal]");
  VELOX_ASSERT_THROW(
      parseType("decimal()"), "Failed to parse type [decimal()]");
  VELOX_ASSERT_THROW(
      parseType("decimal(20)"), "Failed to parse type [decimal(20)]");
  VELOX_ASSERT_THROW(
      parseType("decimal(, 20)"), "Failed to parse type [decimal(, 20)]");
}

} // namespace
} // namespace facebook::velox
