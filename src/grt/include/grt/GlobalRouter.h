/////////////////////////////////////////////////////////////////////////////
//
// BSD 3-Clause License
//
// Copyright (c) 2019, The Regents of the University of California
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
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GRoute.h"
#include "odb/db.h"
#include "odb/dbBlockCallBackObj.h"
#include "sta/Liberty.hh"

namespace ord {
class OpenRoad;
}

namespace gui {
class Gui;
}

namespace utl {
class Logger;
}

namespace odb {
class dbDatabase;
class dbTech;
class dbBlock;
class dbDatabase;
class dbTechLayer;
}  // namespace odb

namespace stt {
class SteinerTreeBuilder;
}

namespace sta {
class dbSta;
class dbNetwork;
}  // namespace sta

namespace grt {

class FastRouteCore;
class AntennaRepair;
class Grid;
class Pin;
class Net;
class Netlist;
class RoutingTracks;
class SteinerTree;
class RoutePt;
class GrouteRenderer;
class GlobalRouter;
class RoutingCongestionDataSource;

struct RegionAdjustment
{
  odb::Rect region;
  int layer;
  float adjustment;

  RegionAdjustment(int min_x,
                   int min_y,
                   int max_x,
                   int max_y,
                   int l,
                   float adjst);
  odb::Rect getRegion() { return region; }
  int getLayer() { return layer; }
  float getAdjustment() { return adjustment; }
};

enum class NetType
{
  Clock,
  Signal,
  Antenna,
  All
};

class RoutePt
{
 public:
  RoutePt() = default;
  RoutePt(int x, int y, int layer);
  int x() { return _x; };
  int y() { return _y; };
  int layer() { return _layer; };

  friend bool operator<(const RoutePt& p1, const RoutePt& p2);

 private:
  int _x;
  int _y;
  int _layer;
};

bool operator<(const RoutePt& p1, const RoutePt& p2);

class GlobalRouter
{
 public:
  GlobalRouter();
  ~GlobalRouter();
  void init(ord::OpenRoad* openroad);
  void clear();

  void setAdjustment(const float adjustment);
  void setMinRoutingLayer(const int min_layer);
  void setMaxRoutingLayer(const int max_layer);
  void setMinLayerForClock(const int min_layer);
  void setMaxLayerForClock(const int max_layer);
  unsigned getDbId();
  void addLayerAdjustment(int layer, float reduction_percentage);
  void addRegionAdjustment(int min_x,
                           int min_y,
                           int max_x,
                           int max_y,
                           int layer,
                           float reduction_percentage);
  void setVerbose(const bool v);
  void setOverflowIterations(int iterations);
  void setGridOrigin(int x, int y);
  void setAllowCongestion(bool allow_congestion);
  void setMacroExtension(int macro_extension);
  void printGrid();

  // flow functions
  void readGuides(const char* file_name);  // just for display
  void writeGuides(const char* file_name);
  std::vector<Net*> initFastRoute(int min_routing_layer, int max_routing_layer);
  void initFastRouteIncr(std::vector<Net*>& nets);
  void estimateRC();
  void estimateRC(odb::dbNet* db_net);
  void globalRoute();
  NetRouteMap& getRoutes() { return routes_; }
  bool haveRoutes() const { return !routes_.empty(); }
  Net* getNet(odb::dbNet* db_net);
  int getTileSize() const;

  // repair antenna public functions
  void repairAntennas(sta::LibertyPort* diode_port, int iterations);

  // Incremental global routing functions.
  // See class IncrementalGRoute.
  void addDirtyNet(odb::dbNet* net);
  void removeDirtyNet(odb::dbNet* net);
  std::set<odb::dbNet*> getDirtyNets() { return dirty_nets_; }

  double dbuToMicrons(int64_t dbu);

  // route clock nets public functions
  void routeClockNets();

  // functions for random grt
  void setSeed(int seed) { seed_ = seed; }
  void setCapacitiesPerturbationPercentage(float percentage);
  void setPerturbationAmount(int perturbation)
  {
    perturbation_amount_ = perturbation;
  };
  void perturbCapacities();

  void initDebugFastRoute();
  void setDebugNet(const odb::dbNet* net);
  void setDebugSteinerTree(bool steinerTree);
  void setDebugRectilinearSTree(bool rectilinearSTree);
  void setDebugTree2D(bool tree2D);
  void setDebugTree3D(bool tree3D);

  // Highlight route in the gui.
  void highlightRoute(const odb::dbNet* net);

  // Clear routes in the gui
  void clearRouteGui();
  // Report the wire length on each layer.
  void reportNetLayerWirelengths(odb::dbNet* db_net, std::ofstream& out);
  void reportLayerWireLengths();
  odb::Rect globalRoutingToBox(const GSegment& route);
  GSegment boxToGlobalRouting(const odb::Rect& route_bds, int layer);

  // Report wire length
  void reportNetWireLength(odb::dbNet* net,
                           bool global_route,
                           bool detailed_route,
                           bool verbose,
                           const char* file_name);
  void reportNetDetailedRouteWL(odb::dbWire* wire, std::ofstream& out);
  void createWLReportFile(const char* file_name, bool verbose);

 private:
  // Net functions
  int getNetCount() const;
  Net* addNet(odb::dbNet* db_net);
  void removeNet(odb::dbNet* db_net);
  int getMaxNetDegree();

  void applyAdjustments(int min_routing_layer, int max_routing_layer);
  // main functions
  void initCoreGrid(int max_routing_layer);
  void initRoutingLayers();
  std::vector<std::pair<int, int>> calcLayerPitches(int max_layer);
  void initRoutingTracks(int max_routing_layer);
  void setCapacities(int min_routing_layer, int max_routing_layer);
  void initNets(std::vector<Net*>& nets);
  void computeGridAdjustments(int min_routing_layer, int max_routing_layer);
  void computeTrackAdjustments(int min_routing_layer, int max_routing_layer);
  void computeUserGlobalAdjustments(int min_routing_layer,
                                    int max_routing_layer);
  void computeUserLayerAdjustments(int max_routing_layer);
  void computeRegionAdjustments(const odb::Rect& region,
                                int layer,
                                float reduction_percentage);
  void applyObstructionAdjustment(const odb::Rect& obstruction,
                                  odb::dbTechLayer* tech_layer);
  int computeNetWirelength(odb::dbNet* db_net);
  void computeWirelength();
  std::vector<Pin*> getAllPorts();
  int computeTrackConsumption(const Net* net,
                              std::vector<int>& edge_costs_per_layer);

  // aux functions
  std::vector<odb::Point> findOnGridPositions(const Pin& pin,
                                              bool& has_access_points,
                                              odb::Point& pos_on_grid);
  void findPins(Net* net);
  void findPins(Net* net, std::vector<RoutePt>& pins_on_grid, int& root_idx);
  odb::dbTechLayer* getRoutingLayerByIndex(int index);
  RoutingTracks getRoutingTracksByIndex(int layer);
  void addGuidesForLocalNets(odb::dbNet* db_net,
                             GRoute& route,
                             int min_routing_layer,
                             int max_routing_layer);
  void addGuidesForPinAccess(odb::dbNet* db_net, GRoute& route);
  void addRemainingGuides(NetRouteMap& routes,
                          std::vector<Net*>& nets,
                          int min_routing_layer,
                          int max_routing_layer);
  void connectPadPins(NetRouteMap& routes);
  void mergeBox(std::vector<odb::Rect>& guide_box);
  bool segmentsConnect(const GSegment& seg0,
                       const GSegment& seg1,
                       GSegment& new_seg,
                       const std::map<RoutePt, int>& segs_at_point);
  void mergeSegments(const std::vector<Pin>& pins, GRoute& route);
  bool pinOverlapsWithSingleTrack(const Pin& pin, odb::Point& track_position);
  GSegment createFakePin(Pin pin,
                         odb::Point& pin_position,
                         odb::dbTechLayer* layer);
  odb::Point findFakePinPosition(Pin& pin, odb::dbNet* db_net);
  void initAdjustments();
  odb::Point getRectMiddle(const odb::Rect& rect);
  NetRouteMap findRouting(std::vector<Net*>& nets,
                          int min_routing_layer,
                          int max_routing_layer);
  void print(GRoute& route);
  void reportLayerSettings(int min_routing_layer, int max_routing_layer);
  void reportResources();
  void reportCongestion();

  // check functions
  void checkPinPlacement();

  // antenna functions
  void addLocalConnections(NetRouteMap& routes);

  // incremental funcions
  void updateDirtyRoutes();
  Capacities getCapacities();
  void mergeResults(NetRouteMap& routes);
  void restoreCapacities(Capacities capacities,
                         int previous_min_layer,
                         int previous_max_layer);
  int getEdgeResource(int x1,
                      int y1,
                      int x2,
                      int y2,
                      odb::dbTechLayer* tech_layer,
                      odb::dbGCellGrid* gcell_grid);
  void removeDirtyNetsRouting();
  void updateDirtyNets();
  void updateDbCongestion();

  // db functions
  void initGrid(int max_layer);
  void initRoutingLayers(std::map<int, odb::dbTechLayer*>& routing_layers);
  void computeCapacities(int max_layer);
  void computeSpacingsAndMinWidth(int max_layer);
  std::vector<Net*> initNetlist();
  Net* getNet(odb::dbNet* db_net);
  void computeObstructionsAdjustments();
  void findLayerExtensions(std::vector<int>& layer_extensions);
  int findObstructions(odb::Rect& die_area);
  bool layerIsBlocked(int layer,
                    odb::dbTechLayerDir& direction,
                    const std::unordered_map<int, odb::Rect>& macro_obs_per_layer,
                    odb::Rect& extended_obs);
  void extendObstructions(std::unordered_map<int, odb::Rect>& macro_obs_per_layer,
                        int bottom_layer,
                        int top_layer);
  int findInstancesObstructions(odb::Rect& die_area,
                                const std::vector<int>& layer_extensions);
  void findNetsObstructions(odb::Rect& die_area);
  int computeMaxRoutingLayer();
  std::map<int, odb::dbTechVia*> getDefaultVias(int max_routing_layer);
  void makeItermPins(Net* net, odb::dbNet* db_net, const odb::Rect& die_area);
  void makeBtermPins(Net* net, odb::dbNet* db_net, const odb::Rect& die_area);
  void initClockNets();
  bool isClkTerm(odb::dbITerm* iterm, sta::dbNetwork* network);
  bool isNonLeafClock(odb::dbNet* db_net);
  int trackSpacing();

  ord::OpenRoad* openroad_;
  utl::Logger* logger_;
  gui::Gui* gui_;
  stt::SteinerTreeBuilder* stt_builder_;
  // Objects variables
  FastRouteCore* fastroute_;
  odb::Point grid_origin_;
  GrouteRenderer* groute_renderer_;
  NetRouteMap routes_;

  std::map<odb::dbNet*, Net*, cmpById> db_net_map_;
  Grid* grid_;
  std::map<int, odb::dbTechLayer*> routing_layers_;
  std::vector<RoutingTracks>* routing_tracks_;

  // Flow variables
  float adjustment_;
  int min_routing_layer_;
  int max_routing_layer_;
  int layer_for_guide_dimension_;
  const int gcells_offset_ = 2;
  int overflow_iterations_;
  bool allow_congestion_;
  std::vector<int> vertical_capacities_;
  std::vector<int> horizontal_capacities_;
  int macro_extension_;

  // Layer adjustment variables
  std::vector<float> adjustments_;

  // Region adjustment variables
  std::vector<RegionAdjustment> region_adjustments_;

  bool verbose_;
  int min_layer_for_clock_;
  int max_layer_for_clock_;

  // variables for random grt
  int seed_;
  float caps_perturbation_percentage_;
  int perturbation_amount_;

  // Variables for PADs obstructions handling
  std::map<odb::dbNet*, std::vector<GSegment>> pad_pins_connections_;

  // db variables
  sta::dbSta* sta_;
  odb::dbDatabase* db_;
  odb::dbBlock* block_;

  std::set<odb::dbNet*> dirty_nets_;

  std::unique_ptr<RoutingCongestionDataSource> heatmap_;

  friend class IncrementalGRoute;
  friend class GRouteDbCbk;
  friend class AntennaRepair;
};

std::string getITermName(odb::dbITerm* iterm);
std::string getLayerName(int layer_idx, odb::dbDatabase* db);

class GRouteDbCbk : public odb::dbBlockCallBackObj
{
 public:
  GRouteDbCbk(GlobalRouter* grouter);
  virtual void inDbPostMoveInst(odb::dbInst* inst);
  virtual void inDbInstSwapMasterAfter(odb::dbInst* inst);

  virtual void inDbNetDestroy(odb::dbNet* net);
  virtual void inDbNetCreate(odb::dbNet* net);

  virtual void inDbITermPreDisconnect(odb::dbITerm* iterm);
  virtual void inDbITermPostConnect(odb::dbITerm* iterm);

  virtual void inDbBTermPostConnect(odb::dbBTerm* bterm);
  virtual void inDbBTermPreDisconnect(odb::dbBTerm* bterm);

 private:
  void instItermsDirty(odb::dbInst* inst);

  GlobalRouter* grouter_;
};

// Class to save global router state and monitor db updates with callbacks
// to make incremental routing updates.
class IncrementalGRoute
{
 public:
  // Saves global router state and enables db callbacks.
  IncrementalGRoute(GlobalRouter* groute, odb::dbBlock* block);
  // Update global routes for dirty nets.
  void updateRoutes();
  // Disables db callbacks.
  ~IncrementalGRoute();

 private:
  GlobalRouter* groute_;
  GRouteDbCbk db_cbk_;
};

}  // namespace grt
