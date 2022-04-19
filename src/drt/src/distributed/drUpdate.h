/* Authors: Osama */
/*
 * Copyright (c) 2022, The Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include "db/obj/frMarker.h"
#include "db/obj/frShape.h"
#include "db/obj/frVia.h"
#include <fstream>
namespace fr {
class frNet;
class drUpdate
{
 public:
  enum UpdateType
  {
    ADD_SHAPE,
    ADD_GUIDE,
    REMOVE_FROM_NET,
    REMOVE_FROM_BLOCK
  };
  drUpdate(UpdateType type = ADD_SHAPE)
      : net_(nullptr),
        order_in_owner_(0),
        type_(type),
        layer_(0),
        bottomConnected_(false),
        topConnected_(false),
        tapered_(false),
        viaDef_(nullptr),
        obj_type_(frcBlock)
  {
  }
  void setNet(frNet* net) { net_ = net; }
  void setOrderInOwner(int value) { order_in_owner_ = value; }
  void setUpdateType(UpdateType value) { type_ = value; }
  void setPathSeg(const frPathSeg& seg);
  void setPatchWire(const frPatchWire& pwire);
  void setVia(const frVia& via);
  void setMarker(const frMarker& marker);
  frPathSeg getPathSeg() const;
  frPatchWire getPatchWire() const;
  frVia getVia() const;
  UpdateType getType() const { return type_; }
  int getOrderInOwner() const { return order_in_owner_; }
  frNet* getNet() const { return net_; }
  frBlockObjectEnum getObjTypeId() const { return obj_type_; }
  frMarker getMarker() const { return marker_; }
  Point getBegin() const { return begin_; }
  Point getEnd() const { return end_; }
  frSegStyle getStyle() const { return style_; }
  Rect getOffsetBox() const { return offsetBox_; }
  frLayerNum getLayerNum() const { return layer_; }
  bool isBottomConnected() const { return bottomConnected_; }
  bool isTopConnected() const { return topConnected_; }
  bool isTapered() const { return tapered_; }
  frViaDef* getViaDef() const { return viaDef_; }
  frBlockObjectEnum getObjType() const { return obj_type_; }
  void setBegin(Point begin) { begin_ = begin; }
  void setEnd(Point end) { end_ = end; }
  void setStyle(frSegStyle style) { style_ = style; }
  void setOffsetBox(Rect rect) { offsetBox_ = rect; }
  void setBottomConnected(bool value) { bottomConnected_ = value; }
  void setTopConnected(bool value) { topConnected_ = value; }
  void setTapered(bool value) { tapered_ = value; }
  void setViaDef(frViaDef* value) { viaDef_ = value; }
  void setObjType(frBlockObjectEnum value) { obj_type_ = value; }
  void setLayerNum(frLayerNum value) { layer_ = value; }
 private:
  frNet* net_;
  int order_in_owner_;
  UpdateType type_;
  Point begin_;
  Point end_;
  frSegStyle style_;
  Rect offsetBox_;
  frLayerNum layer_;
  bool bottomConnected_ : 1;
  bool topConnected_ : 1;
  bool tapered_ : 1;
  frViaDef* viaDef_;
  frBlockObjectEnum obj_type_;
  frMarker marker_;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version);

  friend class boost::serialization::access;
};
}  // namespace fr
std::ofstream& operator<<(std::ofstream& stream, const fr::drUpdate& update);
