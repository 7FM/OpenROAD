//////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2019, OpenROAD
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

#include <QApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QToolTip>
#include <iostream>

#include "db.h"
#include "dbTransform.h"
#include "gui/gui.h"
#include "layoutViewer.h"
#include "mainWindow.h"
#include "search.h"
#include <vector>
#include <tuple>

// Qt's coordinate system is defined with the origin at the UPPER-left
// and y values increase as you move DOWN the screen.  All EDA tools
// and formats use the origin at the LOWER-left with y increasing
// as you move UP the screen.  This mismatch is painful.
//
// To workaround it the painter is setup with shifted and flipped
// coordinates to better match EDA style.  However that also
// flips the text which has to be reversed again to account for this.
// In short, yuck!
//
// The pixelsPerDBU_ field stores pixels per DBU.  This adds additional
// trickiness to the coordinates.

namespace gui {

using namespace odb;

// This class wraps the QPainter in the abstract Painter API for
// Renderer instances to use.
class GuiPainter : public Painter
{
 public:
  GuiPainter(QPainter* painter, Options* options)
      : painter_(painter), options_(options)
  {
  }

  void setPen(odb::dbTechLayer* layer, bool cosmetic) override
  {
    QPen pen(options_->color(layer));
    pen.setCosmetic(cosmetic);
    painter_->setPen(pen);
  }

  void setPen(const Color& color, bool cosmetic) override
  {
    QPen pen(QColor(color.r, color.g, color.b, color.a));
    pen.setCosmetic(cosmetic);
    painter_->setPen(pen);
  }

  void setBrush(odb::dbTechLayer* layer) override
  {
    painter_->setBrush(options_->color(layer));
  }

  void setBrush(const Color& color) override
  {
    painter_->setBrush(QColor(color.r, color.g, color.b, color.a));
  }
  void drawGeomShape(const odb::GeomShape* shape) override
  {
    std::vector<Point> points = shape->getPoints();
    QPoint* qpoints = new QPoint[points.size()];
    for(int i = 0;i<points.size();i++)
      qpoints[i] = QPoint(points[i].getX(),points[i].getY());
    painter_->drawPolygon(qpoints,points.size());
  }
  void drawRect(const odb::Rect& rect) override
  {
    painter_->drawRect(QRect(QPoint(rect.xMin(), rect.yMin()),
                             QPoint(rect.xMax(), rect.yMax())));
  }
  void drawLine(const odb::Point& p1, const odb::Point& p2) override
  {
    painter_->drawLine(p1.x(), p1.y(), p2.x(), p2.y());
  }

 private:
  QPainter* painter_;
  Options* options_;
};

LayoutViewer::LayoutViewer(Options* options,
                           const SelectionSet& selected,
                           QWidget* parent)
    : QWidget(parent),
      db_(nullptr),
      options_(options),
      selected_(selected),
      scroller_(nullptr),
      pixelsPerDBU_(1.0),
      min_depth_(0),
      max_depth_(99),
      search_init_(false),
      rubber_band_showing_(false)
{
  setMouseTracking(true);
  resize(100, 100);  // just a placeholder until we load the design
}

void LayoutViewer::setDb(dbDatabase* db)
{
  if (db_ != db) {
    update();
  }
  db_ = db;
}

dbBlock* LayoutViewer::getBlock()
{
  if (!db_) {
    return nullptr;
  }

  dbChip* chip = db_->getChip();
  if (!chip) {
    return nullptr;
  }

  dbBlock* block = chip->getBlock();
  return block;
}

void LayoutViewer::setPixelsPerDBU(qreal pixelsPerDBU)
{
  pixelsPerDBU_ = pixelsPerDBU;
  dbBlock* block = getBlock();
  if (!block) {
    return;
  }

  dbBox* bbox = block->getBBox();
  QSize size(ceil(bbox->getWidth(0) * pixelsPerDBU),
             ceil(bbox->getLength(0) * pixelsPerDBU));
  resize(size);
  setMinimumSize(size);  // needed by scroll area
  update();
}

void LayoutViewer::zoomIn()
{
  setPixelsPerDBU(pixelsPerDBU_ * 1.2);
}

void LayoutViewer::zoomOut()
{
  setPixelsPerDBU(pixelsPerDBU_ / 1.2);
}

void LayoutViewer::zoomTo(const Rect& rect_dbu)
{
  QSize viewport = scroller_->maximumViewportSize();
  qreal pixelsPerDBU = std::min(viewport.width() / (double) rect_dbu.dx(),
                                viewport.height() / (double) rect_dbu.dy());
  setPixelsPerDBU(pixelsPerDBU);

  QRectF screen_rect = DBUToScreen(rect_dbu);

  // Center the region
  int w = (scroller_->width() - screen_rect.width()) / 2;
  int h = (scroller_->height() - screen_rect.height()) / 2;

  scroller_->horizontalScrollBar()->setValue(screen_rect.left() - w);
  scroller_->verticalScrollBar()->setValue(screen_rect.top() - h);
}

void LayoutViewer::updateRubberBandRegion()
{
  QRect rect = rubber_band_.normalized();
  int unit = ceil(2 / pixelsPerDBU_);
  update(rect.left(), rect.top() - unit / 2, rect.width(), unit);
  update(rect.left() - unit / 2, rect.top(), unit, rect.height());
  update(rect.left(), rect.bottom() - unit / 2, rect.width(), unit);
  update(rect.right() - unit / 2, rect.top(), unit, rect.height());
}

Selected LayoutViewer::selectAtPoint(odb::Point pt_dbu)
{
  // Look for the selected object in reverse layer order
  auto& renderers = Gui::get()->renderers();
  dbTech* tech = getBlock()->getDataBase()->getTech();
  // dbSet doesn't provide a reverse iterator so we have to copy it.
  std::deque<dbTechLayer*> rev_layers;
  for (auto layer : tech->getLayers()) {
    rev_layers.push_front(layer);
  }

  for (auto layer : rev_layers) {
    if (!options_->isVisible(layer) || !options_->isSelectable(layer)) {
      continue;
    }

    for (auto* renderer : renderers) {
      Selected selected = renderer->select(layer, pt_dbu);
      if (selected) {
        return selected;
      }
    }

    auto shapes = search_.search_shapes(
        layer, pt_dbu.x(), pt_dbu.y(), pt_dbu.x(), pt_dbu.y());

    // Just return the first one
    for (auto iter : shapes) {
      if (options_->isNetVisible(std::get<2>(iter))) {
        return Selected(std::get<2>(iter));
      }
    }
  }

  // Check for objects not in a layer
  for (auto* renderer : renderers) {
    Selected selected = renderer->select(nullptr, pt_dbu);
    if (selected) {
      return selected;
    }
  }

  // Look for an instance since no shape was found
  auto insts
      = search_.search_insts(pt_dbu.x(), pt_dbu.y(), pt_dbu.x(), pt_dbu.y());

  // Just return the first one

  if (insts.begin() != insts.end()) {
    return Selected(std::get<2>(*insts.begin()));
  }
  return Selected();
}

void LayoutViewer::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton) {
    if (getBlock()) {
      Point pt_dbu = screenToDBU(event->pos());
      if (qGuiApp->keyboardModifiers() & Qt::ShiftModifier) {
        emit addSelected(selectAtPoint(pt_dbu));
      } else {
        emit selected(selectAtPoint(pt_dbu));
      }
    }
  } else if (event->button() == Qt::RightButton) {
    rubber_band_showing_ = true;
    rubber_band_.setTopLeft(event->pos());
    rubber_band_.setBottomRight(event->pos());
    updateRubberBandRegion();
    setCursor(Qt::CrossCursor);
  }
}

void LayoutViewer::mouseMoveEvent(QMouseEvent* event)
{
  dbBlock* block = getBlock();
  if (!block) {
    return;
  }

  // emit location in microns
  Point pt_dbu = screenToDBU(event->pos());
  qreal toMicrons = block->getDbUnitsPerMicron();
  emit location(pt_dbu.x() / toMicrons, pt_dbu.y() / toMicrons);

  if (rubber_band_showing_) {
    updateRubberBandRegion();
    rubber_band_.setBottomRight(event->pos());
    updateRubberBandRegion();
  }
}

void LayoutViewer::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::RightButton && rubber_band_showing_) {
    rubber_band_showing_ = false;
    updateRubberBandRegion();
    unsetCursor();

    QRect rect = rubber_band_.normalized();
    if (rect.width() < 4 || rect.height() < 4) {
      return;  // ignore clicks not intended to be drags
    }

    Rect rubber_band_dbu = screenToDBU(rect);

    // Clip to the block bounds
    dbBlock* block = getBlock();
    dbBox* bbox = block->getBBox();

    rubber_band_dbu.set_xlo(qMax(rubber_band_dbu.xMin(), bbox->xMin()));
    rubber_band_dbu.set_ylo(qMax(rubber_band_dbu.yMin(), bbox->yMin()));
    rubber_band_dbu.set_xhi(qMin(rubber_band_dbu.xMax(), bbox->xMax()));
    rubber_band_dbu.set_yhi(qMin(rubber_band_dbu.yMax(), bbox->yMax()));

    zoomTo(rubber_band_dbu);
  }
}

void LayoutViewer::resizeEvent(QResizeEvent* event)
{
  dbBlock* block = getBlock();
  if (block) {
    dbBox* bbox = block->getBBox();
    pixelsPerDBU_
        = std::min(event->size().width() / (double) bbox->getWidth(0),
                   event->size().height() / (double) bbox->getLength(0));
  }
}

QColor LayoutViewer::getColor(dbTechLayer* layer)
{
  return options_->color(layer);
}

void LayoutViewer::addInstTransform(QTransform& xfm,
                                    const dbTransform& inst_xfm)
{
  xfm.translate(inst_xfm.getOffset().getX(), inst_xfm.getOffset().getY());

  switch (inst_xfm.getOrient()) {
    case dbOrientType::R0:
      break;
    case dbOrientType::R90:
      xfm.rotate(90);
      break;
    case dbOrientType::R180:
      xfm.rotate(180);
      break;
    case dbOrientType::R270:
      xfm.rotate(270);
      break;
    case dbOrientType::MY:
      xfm.scale(-1, 1);
      break;
    case dbOrientType::MYR90:
      xfm.scale(-1, 1);
      xfm.rotate(90);
      break;
    case dbOrientType::MX:
      xfm.scale(1, -1);
      break;
    case dbOrientType::MXR90:
      xfm.scale(1, -1);
      xfm.rotate(90);
      break;
    default:
      break;  // error
  }
}

// Cache the boxes for shapes in obs/mterm by layer per master for
// drawing performance
void LayoutViewer::boxesByLayer(dbMaster* master, LayerBoxes& boxes)
{
  // store obstructions
  for (dbBox* box : master->getObstructions()) {
    dbTechLayer* layer = box->getTechLayer();
    dbTechLayerType type = layer->getType();
    if (type != dbTechLayerType::ROUTING && type != dbTechLayerType::CUT) {
      continue;
    }
    boxes[layer].obs.emplace_back(QRect(QPoint(box->xMin(), box->yMin()),
                                        QPoint(box->xMax(), box->yMax())));
  }

  // store mterms
  for (dbMTerm* mterm : master->getMTerms()) {
    for (dbMPin* mpin : mterm->getMPins()) {
      for (dbBox* box : mpin->getGeometry()) {
        dbTechLayer* layer = box->getTechLayer();
        dbTechLayerType type = layer->getType();
        if (type != dbTechLayerType::ROUTING && type != dbTechLayerType::CUT) {
          continue;
        }
        boxes[layer].mterms.emplace_back(
            QRect(QPoint(box->xMin(), box->yMin()),
                  QPoint(box->xMax(), box->yMax())));
      }
    }
  }
}

// Get the boxes for the given layer & master from the cache,
// populating the cache if necessary
const LayoutViewer::Boxes* LayoutViewer::boxesByLayer(dbMaster* master,
                                                      dbTechLayer* layer)
{
  auto it = cell_boxes_.find(master);
  if (it == cell_boxes_.end()) {
    LayerBoxes& boxes = cell_boxes_[master];
    boxesByLayer(master, boxes);
  }
  it = cell_boxes_.find(master);
  LayerBoxes& boxes = it->second;

  auto layer_it = boxes.find(layer);
  if (layer_it != boxes.end()) {
    return &layer_it->second;
  }
  return nullptr;
}

void LayoutViewer::drawTracks(dbTechLayer* layer,
                              dbBlock* block,
                              QPainter* painter,
                              const Rect& bounds)
{
  if (!options_->arePrefTracksVisible()
      && !options_->areNonPrefTracksVisible()) {
    return;
  }

  dbTrackGrid* grid = block->findTrackGrid(layer);
  if (!grid) {
    return;
  }

  bool isHorizontal = layer->getDirection() == dbTechLayerDir::HORIZONTAL;
  std::vector<int> grids;
  if ((!isHorizontal && options_->arePrefTracksVisible())
      || (isHorizontal && options_->areNonPrefTracksVisible())) {
    grid->getGridX(grids);
    for (int x : grids) {
      if (x < bounds.xMin()) {
        continue;
      }
      if (x > bounds.xMax()) {
        break;
      }
      painter->drawLine(x, bounds.yMin(), x, bounds.yMax());
    }
  }

  if ((isHorizontal && options_->arePrefTracksVisible())
      || (!isHorizontal && options_->areNonPrefTracksVisible())) {
    grid->getGridY(grids);
    for (int y : grids) {
      if (y < bounds.yMin()) {
        continue;
      }
      if (y > bounds.yMax()) {
        break;
      }
      painter->drawLine(bounds.xMin(), y, bounds.xMax(), y);
    }
  }
}

void LayoutViewer::drawRows(dbBlock* block,
                            QPainter* painter,
                            const Rect& bounds)
{
  if (!options_->areRowsVisible()) {
    return;
  }
  painter->setPen(Qt::white);
  painter->setBrush(QColor(0, 0xff, 0, 0x70));
  for (dbRow* row : block->getRows()) {
    int x;
    int y;
    row->getOrigin(x, y);

    dbSite* site = row->getSite();
    int spacing = row->getSpacing();
    int w = site->getWidth();
    int h = site->getHeight();
    switch (row->getOrient()) {
      case dbOrientType::R0:
      case dbOrientType::R180:
      case dbOrientType::MY:
      case dbOrientType::MX:
        /* do nothing */
        break;

      case dbOrientType::R90:
      case dbOrientType::R270:
      case dbOrientType::MYR90:
      case dbOrientType::MXR90:
        std::swap(w, h);
    }

    dbRowDir dir = row->getDirection();
    int count = row->getSiteCount();
    for (int i = 0; i < count; ++i) {
      painter->drawRect(QRect(QPoint(x, y), QPoint(x + w, y + h)));
      if (dir == dbRowDir::HORIZONTAL) {
        x += spacing;
      } else {
        y += spacing;
      }
    }
  }
}

void LayoutViewer::drawSelected(Painter& painter)
{
  for (auto& selected : selected_) {
    selected.highlight(painter);
  }
}

// Draw the region of the block.  Depth is not yet used but
// is there for hierarchical design support.
void LayoutViewer::drawBlock(QPainter* painter,
                             const Rect& bounds,
                             dbBlock* block,
                             int depth)
{
  int pixel = 1 / pixelsPerDBU_;  // 1 pixel in DBU
  LayerBoxes boxes;
  QTransform initial_xfm = painter->transform();

  auto& renderers = Gui::get()->renderers();
  GuiPainter gui_painter(painter, options_);

  auto inst_range = search_.search_insts(
      bounds.xMin(), bounds.yMin(), bounds.xMax(), bounds.yMax(), 1 * pixel);

  // Cache the search results as we will iterate over the instances
  // for each layer.
  std::vector<dbInst*> insts;
  insts.reserve(10000);
  for (auto& [box, poly, inst] : inst_range) {
    insts.push_back(inst);
  }

  // Draw the instances bounds
  for (auto inst : insts) {
    dbMaster* master = inst->getMaster();
    // setup the instance's transform
    QTransform xfm = painter->transform();
    dbTransform inst_xfm;
    inst->getTransform(inst_xfm);
    addInstTransform(xfm, inst_xfm);
    painter->setTransform(xfm);

    // draw bbox
    painter->setPen(QPen(Qt::gray, 0));
    painter->setBrush(QBrush());
    int master_w = master->getWidth();
    int master_h = master->getHeight();
    painter->drawRect(QRect(QPoint(0, 0), QPoint(master_w, master_h)));

    // Draw an orientation tag in corner if useful in size
    if (master->getHeight() >= 5 * pixel) {
      qreal tag_size = 0.1 * master_h;
      painter->drawLine(QPointF(std::min(tag_size / 2, (double) master_w), 0.0),
                        QPointF(0.0, tag_size));
    }
    painter->setTransform(initial_xfm);
  }

  dbTech* tech = block->getDataBase()->getTech();
  for (dbTechLayer* layer : tech->getLayers()) {
    if (!options_->isVisible(layer)) {
      continue;
    }

    // Skip the cut layer if the cuts will be too small to see
    const bool is_cut = layer->getType() == dbTechLayerType::CUT;
    if (is_cut && layer->getWidth() < 1 * pixel) {
      continue;
    }

    // Draw the instances' shapes
    for (auto inst : insts) {
      dbMaster* master = inst->getMaster();
      if (master->getHeight() < 5 * pixel) {
        continue;
      }

      const Boxes* boxes = boxesByLayer(master, layer);

      if (boxes == nullptr) {
        continue;  // no shapes on this layer
      }

      // setup the instance's transform
      QTransform xfm = painter->transform();
      dbTransform inst_xfm;
      inst->getTransform(inst_xfm);
      addInstTransform(xfm, inst_xfm);
      painter->setTransform(xfm);

      // Only draw the pins/obs if they are big enough to be useful
      painter->setPen(Qt::NoPen);
      QColor color = getColor(layer);
      painter->setBrush(color);

      painter->setBrush(color.lighter());
      for (auto& box : boxes->obs) {
        painter->drawRect(box);
      }

      painter->setBrush(color);
      for (auto& box : boxes->mterms) {
        painter->drawRect(box);
      }

#if 0
        // TODO
        // draw text
        painter->setPen(QPen(Qt::yellow, 0));
        painter->setBrush(QBrush(Qt::yellow));
        auto name = master->getName();
        qreal font_scale = master_w
            / painter->fontMetrics().horizontalAdvance(name.c_str());

        font_scale = std::min(font_scale, 5000.0);
        QFont f = painter->font();
        f.setPointSizeF(f.pointSize() * pixelsPerDBU_);
        painter->setFont(f);

        painter->scale(1, -1);
        painter->drawText(0, -master_h, master_w, master_h,
                          Qt::AlignCenter, name.c_str());
#endif

      painter->setTransform(initial_xfm);
    }

    // Now draw the shapes
    QColor color = getColor(layer);
    painter->setBrush(color);
    painter->setPen(QPen(color, 0));
    auto iter = search_.search_shapes(layer,
                                      bounds.xMin(),
                                      bounds.yMin(),
                                      bounds.xMax(),
                                      bounds.yMax(),
                                      5 * pixel);

    for (auto& i : iter) {
      if (!options_->isNetVisible(std::get<2>(i))) {
        continue;
      }
      auto poly = std::get<1>(i);
      int size = poly.outer().size();
      QPoint* qpoints = new QPoint[size];
      for(int i = 0;i<size;i++)
        qpoints[i] = QPoint(poly.outer()[i].x(),poly.outer()[i].y());
      painter->drawPolygon(qpoints,size);
      // auto bbox = std::get<0>(i);
      // const auto& ll = bbox.min_corner();
      // const auto& ur = bbox.max_corner();
      // painter->drawRect(QRect(QPoint(ll.x(), ll.y()), QPoint(ur.x(), ur.y())));
    }

    // Now draw the fills
    if (options_->areFillsVisible()) {
      QColor color = getColor(layer).lighter();
      painter->setBrush(color);
      painter->setPen(QPen(color, 0));
      auto iter = search_.search_fills(layer,
                                       bounds.xMin(),
                                       bounds.yMin(),
                                       bounds.xMax(),
                                       bounds.yMax(),
                                       5 * pixel);

      for (auto& i : iter) {
        const auto& ll = i.first.min_corner();
        const auto& ur = i.first.max_corner();
        int w = ur.x() - ll.x();
        int h = ur.y() - ll.y();
        painter->drawRect(
            QRect(QPoint(ll.x(), ll.y()), QPoint(ur.x(), ur.y())));
      }
    }

    drawTracks(layer, block, painter, bounds);
    for (auto* renderer : renderers) {
      renderer->drawLayer(layer, gui_painter);
    }
  }

  drawRows(block, painter, bounds);
  for (auto* renderer : renderers) {
    renderer->drawObjects(gui_painter);
  }

  // Always last so on top
  drawSelected(gui_painter);
}

odb::Point LayoutViewer::screenToDBU(const QPoint& point)
{
  return Point(point.x() / pixelsPerDBU_,
               (height() - point.y()) / pixelsPerDBU_);
}

Rect LayoutViewer::screenToDBU(const QRect& screen_rect)
{
  int dbu_left = (int) floor(screen_rect.left() / pixelsPerDBU_);
  int dbu_right = (int) ceil(screen_rect.right() / pixelsPerDBU_);
  int dbu_top = (int) floor(screen_rect.top() / pixelsPerDBU_);
  int dbu_bottom = (int) ceil(screen_rect.bottom() / pixelsPerDBU_);

  // Flip the y-coordinate (see file level comments)
  dbBlock* block = getBlock();
  int dbu_height = block->getBBox()->getDY();
  dbu_top = dbu_height - dbu_top;
  dbu_bottom = dbu_height - dbu_bottom;

  return Rect(dbu_left, dbu_bottom, dbu_right, dbu_top);
}

QRectF LayoutViewer::DBUToScreen(const Rect& dbu_rect)
{
  dbBlock* block = getBlock();
  int dbu_height = block->getBBox()->getDY();

  // Flip the y-coordinate (see file level comments)
  qreal screen_left = dbu_rect.xMin() * pixelsPerDBU_;
  qreal screen_right = dbu_rect.xMax() * pixelsPerDBU_;
  qreal screen_top = (dbu_height - dbu_rect.yMax()) * pixelsPerDBU_;
  qreal screen_bottom = (dbu_height - dbu_rect.yMin()) * pixelsPerDBU_;

  return QRectF(QPointF(screen_left, screen_top),
                QPointF(screen_right, screen_bottom));
}

void LayoutViewer::paintEvent(QPaintEvent* event)
{
  dbBlock* block = getBlock();
  if (!block) {
    return;
  }

  if (!search_init_) {
    search_.init(block);
    search_init_ = true;
  }

  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing);

  // Fill draw region with black
  painter.setPen(QPen(Qt::black, 0));
  painter.setBrush(Qt::black);
  painter.drawRect(event->rect());

  // Coordinate system setup (see file level comments)
  painter.save();
  painter.translate(0, height());
  painter.scale(pixelsPerDBU_, -pixelsPerDBU_);

  Rect dbu_bounds = screenToDBU(event->rect());
  drawBlock(&painter, dbu_bounds, block, 0);
  painter.restore();

  if (rubber_band_showing_) {
    painter.setPen(QPen(Qt::white, 0));
    painter.setBrush(QBrush());
    painter.drawRect(rubber_band_.normalized());
  }
}

void LayoutViewer::fit()
{
  dbBlock* block = getBlock();
  if (block == nullptr) {
    return;
  }
  dbBox* bbox = block->getBBox();

  QSize viewport = scroller_->maximumViewportSize();
  qreal pixelsPerDBU
      = std::min(viewport.width() / (double) bbox->getWidth(0),
                 viewport.height() / (double) bbox->getLength(0));
  setPixelsPerDBU(pixelsPerDBU);
}

void LayoutViewer::designLoaded(dbBlock* block)
{
  addOwner(block);  // register as a callback object
  fit();
}

void LayoutViewer::setScroller(LayoutScroll* scroller)
{
  scroller_ = scroller;
}

////// LayoutScroll ///////
LayoutScroll::LayoutScroll(LayoutViewer* viewer, QWidget* parent)
    : QScrollArea(parent), viewer_(viewer)
{
  setWidgetResizable(true);
  setWidget(viewer);
  viewer->setScroller(this);
}

// Handles zoom in/out on ctrl-wheel
void LayoutScroll::wheelEvent(QWheelEvent* event)
{
  if (!event->modifiers().testFlag(Qt::ControlModifier)) {
    QScrollArea::wheelEvent(event);
    return;
  }
  qreal old_pixelsPerDBU = viewer_->getPixelsPerDBU();

  int scrollbar_x = horizontalScrollBar()->value();
  int scrollbar_y = verticalScrollBar()->value();
  QPointF pos_in_widget = QPointF(event->pos()) - widget()->pos();

  if (event->delta() > 0) {
    viewer_->zoomIn();
  } else {
    viewer_->zoomOut();
  }

  qreal new_pixelsPerDBU = viewer_->getPixelsPerDBU();
  QPointF delta = (new_pixelsPerDBU / old_pixelsPerDBU - 1) * pos_in_widget;

  horizontalScrollBar()->setValue(scrollbar_x + delta.x());
  verticalScrollBar()->setValue(scrollbar_y + delta.y());
}

void LayoutViewer::inDbPostMoveInst(dbInst*)
{
  // This is not very smart - we just clear all the search structure
  // rather than try to surgically update it.  We need a pre & post
  // callback from OpenDB to do this right.
  // TODO:: Update this now with the new callbacks from OpenDB
  if (search_init_) {
    search_.clear();
    search_init_ = false;
  }
  update();
}

}  // namespace gui
