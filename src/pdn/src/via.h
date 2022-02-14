///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2022, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <array>
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <map>
#include <memory>

#include "odb/dbTypes.h"
#include "odb/geom.h"

namespace odb {
class dbBlock;
class dbNet;
class dbTech;
class dbTechLayer;
class dbTechLayerCutClassRule;
class dbViaVia;
class dbTechVia;
class dbTechViaGenerateRule;
class dbTechViaLayerRule;
class dbSBox;
class dbSWire;
class dbVia;
class dbViaParams;
class dbTechLayerCutEnclosureRule;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace pdn {

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

class Connect;
class Shape;
class Via;

using Point = bg::model::d2::point_xy<int, bg::cs::cartesian>;
using Box = bg::model::box<Point>;
using ShapePtr = std::shared_ptr<Shape>;
using ViaPtr = std::shared_ptr<Via>;
using ShapeValue = std::pair<Box, ShapePtr>;
using ViaValue = std::pair<Box, ViaPtr>;
using ShapeTree = bgi::rtree<ShapeValue, bgi::quadratic<16>>;
using ViaTree = bgi::rtree<ViaValue, bgi::quadratic<16>>;
using ShapeTreeMap = std::map<odb::dbTechLayer*, ShapeTree>;

class Grid;
class TechLayer;

// Wrapper class to handle building actual DB Vias
class DbVia
{
 public:
  struct ViaLayerShape
  {
    std::set<odb::Rect> bottom;
    std::set<odb::Rect> top;
  };

  DbVia() = default;
  virtual ~DbVia() {}

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType type,
                                 int x,
                                 int y) const = 0;

  virtual bool requiresPatch() const { return false; }

 protected:
  ViaLayerShape getLayerShapes(odb::dbSBox* box) const;
  void combineLayerShapes(const ViaLayerShape& other,
                          ViaLayerShape& shapes) const;
};

class DbBaseVia : public DbVia
{
 public:
  virtual const odb::Rect getViaRect(bool include_enclosure = true) const = 0;
};

class DbTechVia : public DbBaseVia
{
 public:
  DbTechVia(odb::dbTechVia* via,
            int rows,
            int row_pitch,
            int cols,
            int col_pitch);
  virtual ~DbTechVia() {}

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType type,
                                 int x,
                                 int y) const override;

  virtual bool requiresPatch() const override { return rows_ > 1 || cols_ > 1; }

  virtual const odb::Rect getViaRect(bool include_enclosure
                                     = true) const override;

 private:
  odb::dbTechVia* via_;
  int rows_;
  int row_pitch_;
  int cols_;
  int col_pitch_;

  odb::Rect via_rect_;
  odb::Rect enc_rect_;
};

class DbGenerateVia : public DbBaseVia
{
 public:
  DbGenerateVia(const odb::Rect& rect,
                odb::dbTechViaGenerateRule* rule,
                int rows,
                int columns,
                int cut_pitch_x,
                int cut_pitch_y,
                int bottom_enclosure_x,
                int bottom_enclosure_y,
                int top_enclosure_x,
                int top_enclosure_y,
                odb::dbTechLayer* bottom,
                odb::dbTechLayer* cut,
                odb::dbTechLayer* top);
  virtual ~DbGenerateVia() {}

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType type,
                                 int x,
                                 int y) const override;

  virtual const odb::Rect getViaRect(bool include_enclosure
                                     = true) const override;

 private:
  odb::Rect rect_;
  odb::Rect cut_rect_;

  odb::dbTechViaGenerateRule* rule_;
  int rows_;
  int columns_;

  int cut_pitch_x_;
  int cut_pitch_y_;

  int bottom_enclosure_x_;
  int bottom_enclosure_y_;
  int top_enclosure_x_;
  int top_enclosure_y_;

  odb::dbTechLayer* bottom_;
  odb::dbTechLayer* cut_;
  odb::dbTechLayer* top_;

  const std::string getName() const;
};

class DbSplitCutVia : public DbVia
{
 public:
  DbSplitCutVia(DbBaseVia* via,
                int rows,
                int row_pitch,
                int cols,
                int col_pitch,
                odb::dbBlock* block,
                odb::dbTechLayer* bottom,
                bool snap_bottom,
                odb::dbTechLayer* top,
                bool snap_top);
  virtual ~DbSplitCutVia();

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType type,
                                 int x,
                                 int y) const override;

 private:
  std::unique_ptr<TechLayer> bottom_;
  std::unique_ptr<TechLayer> top_;

  std::unique_ptr<DbBaseVia> via_;
  int rows_;
  int row_pitch_;
  int cols_;
  int col_pitch_;
};

class DbArrayVia : public DbVia
{
 public:
  DbArrayVia(DbBaseVia* core_via,
             DbBaseVia* end_of_row,
             DbBaseVia* end_of_column,
             DbBaseVia* end_of_row_column,
             int core_rows,
             int core_cols,
             int array_spacing_x,
             int array_spacing_y);
  virtual ~DbArrayVia() {}

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType type,
                                 int x,
                                 int y) const override;

  virtual bool requiresPatch() const override { return true; }

 private:
  std::unique_ptr<DbBaseVia> core_via_;
  std::unique_ptr<DbBaseVia> end_of_row_;
  std::unique_ptr<DbBaseVia> end_of_column_;
  std::unique_ptr<DbBaseVia> end_of_row_column_;
  int rows_;
  int columns_;

  int array_spacing_x_;
  int array_spacing_y_;

  int array_start_x_;
  int array_start_y_;
};

class DbGenerateStackedVia : public DbVia
{
 public:
  DbGenerateStackedVia(const std::vector<DbVia*>& vias,
                       odb::dbTechLayer* bottom,
                       odb::dbBlock* block,
                       const std::set<odb::dbTechLayer*>& ongrid);
  virtual ~DbGenerateStackedVia();

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType type,
                                 int x,
                                 int y) const override;

 private:
  std::vector<std::unique_ptr<DbVia>> vias_;
  std::vector<std::unique_ptr<TechLayer>> layers_;
};

class DbGenerateDummyVia : public DbVia
{
 public:
  DbGenerateDummyVia(utl::Logger* logger,
                     const odb::Rect& shape,
                     odb::dbTechLayer* bottom,
                     odb::dbTechLayer* top);
  virtual ~DbGenerateDummyVia() {}

  virtual ViaLayerShape generate(odb::dbBlock* block,
                                 odb::dbSWire* wire,
                                 odb::dbWireShapeType /* type */,
                                 int x,
                                 int y) const override;

 private:
  utl::Logger* logger_;
  const odb::Rect shape_;
  odb::dbTechLayer* bottom_;
  odb::dbTechLayer* top_;
};

// Class to build a generate via, either as a single group or as an array
class ViaGenerator
{
 public:
  ViaGenerator(utl::Logger* logger, const odb::Rect& lower_rect, const odb::Rect& upper_rect);
  virtual ~ViaGenerator() = default;

  virtual odb::dbTechLayer* getBottomLayer() const = 0;
  virtual odb::dbTechLayer* getTopLayer() const = 0;
  virtual odb::dbTechLayer* getCutLayer() const = 0;

  const odb::Rect& getCut() const { return cut_; }
  virtual int getCutArea() const;

  void setCutPitchX(int pitch) { cut_pitch_x_ = pitch; }
  int getCutPitchX() const { return cut_pitch_x_; }
  void setCutPitchY(int pitch) { cut_pitch_y_ = pitch; }
  int getCutPitchY() const { return cut_pitch_y_; }

  void setMaxRows(int rows) { max_rows_ = rows; }
  void setMaxColumns(int columns) { max_cols_ = columns; }

  odb::dbTechLayerCutClassRule* getCutClass() const { return cutclass_; }
  bool hasCutClass() const { return cutclass_ != nullptr; }

  virtual bool isSetupValid(odb::dbTechLayer* lower,
                            odb::dbTechLayer* upper) const;
  virtual bool checkConstraints() const;

  // determine the shape of the vias
  virtual void determineRowsAndColumns(bool use_bottom_min_enclosure,
                                       bool use_top_min_enclosure);
  virtual int getRows() const;
  virtual int getColumns() const;
  int getTotalCuts() const;

  DbVia* generate(odb::dbBlock* block) const;
  virtual DbBaseVia* makeBaseVia(int rows,
                                 int row_pitch,
                                 int cols,
                                 int col_pitch) const = 0;

  const odb::Rect& getLowerRect() const { return lower_rect_; }
  const odb::Rect& getUpperRect() const { return upper_rect_; }
  const odb::Rect& getIntersectionRect() const { return intersection_rect_; }

  void setSplitCutArray(bool split_cuts_bot, bool split_cuts_top);
  bool isSplitCutArray() const { return split_cuts_top_ || split_cuts_bottom_; }
  bool isCutArray() const
  {
    return !isSplitCutArray() && (array_core_x_ != 1 || array_core_y_ != 1);
  }

  int getBottomEnclosureX() const { return bottom_x_enclosure_; }
  int getBottomEnclosureY() const { return bottom_y_enclosure_; }
  int getTopEnclosureX() const { return top_x_enclosure_; }
  int getTopEnclosureY() const { return top_y_enclosure_; }

 protected:
  int getMaxRows() const { return max_rows_; }
  int getMaxColumns() const { return max_cols_; }

  bool isCutClass(odb::dbTechLayerCutClassRule* cutclass) const;
  void setCut(const odb::Rect& cut);

  int getCuts(int width,
              int cut,
              int bot_enc,
              int top_enc,
              int pitch,
              int max_cuts) const;

  int getCutsWidth(int cuts, int cut_width, int spacing, int enc) const;
  virtual void getMinimumEnclosure(odb::dbTechLayer* layer,
                                   int width,
                                   int& dx,
                                   int& dy) const;
  bool getCutMinimumEnclosure(int width,
                              odb::dbTechLayer* layer,
                              int& overhang1,
                              int& overhang2) const;

  int getViaCoreRows() const { return core_row_; }
  int getViaCoreColumns() const { return core_col_; }
  int getViaLastRows() const { return end_row_; }
  bool hasViaLastRows() const { return end_row_ != 0; }
  int getViaLastColumns() const { return end_col_; }
  bool hasViaLastColumns() const { return end_col_ != 0; }

  int getArraySpacingX() const { return array_spacing_x_; }
  int getArraySpacingY() const { return array_spacing_y_; }

  int getArrayCoresX() const { return array_core_x_; }
  int getArrayCoresY() const { return array_core_y_; }

  utl::Logger* getLogger() const { return logger_; }
  odb::dbTech* getTech() const;

 protected:
  int getLowerWidth(bool only_real = true) const;
  int getUpperWidth(bool only_real = true) const;

  void determineCutSpacing();

 private:
  utl::Logger* logger_;

  odb::Rect lower_rect_;
  odb::Rect upper_rect_;
  odb::Rect intersection_rect_;

  odb::Rect cut_;

  odb::dbTechLayerCutClassRule* cutclass_;

  int cut_pitch_x_;
  int cut_pitch_y_;

  int max_rows_;
  int max_cols_;

  int core_row_;
  int core_col_;
  int end_row_;
  int end_col_;

  bool split_cuts_bottom_;
  bool split_cuts_top_;

  int array_spacing_x_;
  int array_spacing_y_;
  int array_core_x_;
  int array_core_y_;

  int bottom_x_enclosure_;
  int bottom_y_enclosure_;
  int top_x_enclosure_;
  int top_y_enclosure_;

  void determineCutClass();
  bool checkMinCuts() const;
  bool checkMinCuts(odb::dbTechLayer* layer, int width) const;
  bool appliesToLayers(odb::dbTechLayer* lower, odb::dbTechLayer* upper) const;

  bool checkMinEnclosure() const;

  std::vector<odb::dbTechLayerCutEnclosureRule*> getCutMinimumEnclosureRules(int width, bool above) const;
};

// Class to build a generate via, either as a single group or as an array
class GenerateViaGenerator : public ViaGenerator
{
 public:
  GenerateViaGenerator(utl::Logger* logger,
                       odb::dbTechViaGenerateRule* rule,
                       const odb::Rect& lower_rect,
                       const odb::Rect& upper_rect);

  const std::string getName() const;
  const std::string getRuleName() const;

  virtual odb::dbTechLayer* getBottomLayer() const override;
  odb::dbTechViaLayerRule* getBottomLayerRule() const;
  virtual odb::dbTechLayer* getTopLayer() const override;
  odb::dbTechViaLayerRule* getTopLayerRule() const;
  virtual odb::dbTechLayer* getCutLayer() const override;
  odb::dbTechViaLayerRule* getCutLayerRule() const;

  virtual bool isSetupValid(odb::dbTechLayer* lower,
                            odb::dbTechLayer* upper) const override;

  virtual DbBaseVia* makeBaseVia(int rows,
                                 int row_pitch,
                                 int cols,
                                 int col_pitch) const override;

 protected:
  virtual void getMinimumEnclosure(odb::dbTechLayer* layer,
                                   int width,
                                   int& dx,
                                   int& dy) const override;

 private:
  odb::dbTechViaGenerateRule* rule_;

  std::array<uint, 3> layers_;

  bool isLayerValidForWidth(odb::dbTechViaLayerRule*, int width) const;
  bool getLayerEnclosureRule(odb::dbTechViaLayerRule* rule,
                             int& dx,
                             int& dy) const;
  bool isBottomValidForWidth(int width) const;
  bool isTopValidForWidth(int width) const;
};

// Class to build a generate via, either as a single group or as an array
class TechViaGenerator : public ViaGenerator
{
 public:
  TechViaGenerator(utl::Logger* logger,
                   odb::dbTechVia* via,
                   const odb::Rect& lower_rect,
                   const odb::Rect& upper_rect);

  virtual odb::dbTechLayer* getBottomLayer() const override { return bottom_; }
  virtual odb::dbTechLayer* getTopLayer() const override { return top_; }
  virtual odb::dbTechLayer* getCutLayer() const override { return cut_; }

  int getCutArea() const override;

  virtual bool isSetupValid(odb::dbTechLayer* lower,
                            odb::dbTechLayer* upper) const override;

  odb::dbTechVia* getVia() const { return via_; }

  virtual DbBaseVia* makeBaseVia(int rows,
                                 int row_pitch,
                                 int cols,
                                 int col_pitch) const override;

 protected:
  virtual void getMinimumEnclosure(odb::dbTechLayer* layer,
                                   int width,
                                   int& dx,
                                   int& dy) const override;

 private:
  odb::dbTechVia* via_;

  int cuts_;

  odb::dbTechLayer* bottom_;
  odb::dbTechLayer* cut_;
  odb::dbTechLayer* top_;

  bool fitsShapes() const;
  bool mostlyContains(const odb::Rect& full_shape,
                      const odb::Rect& small_shape) const;
};

class Via
{
 public:
  Via(Connect* connect,
      odb::dbNet* net,
      const odb::Rect& area,
      const ShapePtr& lower,
      const ShapePtr& upper);

  odb::dbNet* getNet() const { return net_; }
  const odb::Rect& getArea() const { return area_; }
  const Box getBox() const;
  void setLowerShape(ShapePtr shape) { lower_ = shape; }
  const ShapePtr& getLowerShape() const { return lower_; }
  void setUpperShape(ShapePtr shape) { upper_ = shape; }
  const ShapePtr& getUpperShape() const { return upper_; }
  odb::dbTechLayer* getLowerLayer() const;
  odb::dbTechLayer* getUpperLayer() const;

  void removeShape(Shape* shape);

  bool isValid() const;

  bool containsIntermediateLayer(odb::dbTechLayer* layer) const;
  bool overlaps(const ViaPtr& via) const;
  bool startsBelow(const ViaPtr& via) const;

  Connect* getConnect() const { return connect_; }

  void writeToDb(odb::dbSWire* wire, odb::dbBlock* block) const;

  Grid* getGrid() const;

  const std::string getDisplayText() const;

  Via* copy() const;

 private:
  odb::dbNet* net_;
  odb::Rect area_;
  ShapePtr lower_;
  ShapePtr upper_;

  Connect* connect_;

  utl::Logger* getLogger() const;
};

}  // namespace pdn
