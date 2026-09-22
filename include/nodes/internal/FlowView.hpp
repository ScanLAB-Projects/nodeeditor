#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtCore/QElapsedTimer>
#include <set>

#include "Export.hpp"
  
class QTimer;

namespace QtNodes
{

class FlowScene;
class NodeGraphicsObject;

class NODE_EDITOR_PUBLIC FlowView
  : public QGraphicsView
{
  Q_OBJECT
public:

  FlowView(QWidget *parent = Q_NULLPTR);
  FlowView(FlowScene *scene, QWidget *parent = Q_NULLPTR);

  FlowView(const FlowView&) = delete;
  FlowView operator=(const FlowView&) = delete;

  QAction* clearSelectionAction() const;

  QAction* deleteSelectionAction() const;

  void setScene(FlowScene *scene);

  QJsonObject selectionToJson(bool includePartialConnections=false);
  void jsonToScene(QJsonObject object);
  void jsonToSceneMousePos(QJsonObject object);
  void deleteJsonElements(const QJsonObject &object);

  void goToNode(NodeGraphicsObject *node);
  void goToNodeID(QUuid ID);

  /// Navigation style for every FlowView (set from the app's preferences).
  /// blender: middle-drag / Alt+left-drag pans, Ctrl+(that) zooms, left-drag on empty space box-selects,
  /// Home frames all nodes. trackpadScroll: two-finger scroll pans and Ctrl+scroll (pinch) zooms;
  /// otherwise the wheel zooms at the cursor. blender=false keeps the original behaviour.
  /// trackpadSpeed scales trackpad pan; zoomSpeed scales zoom (wheel, Ctrl+scroll, pinch) and is capped per step.
  static void setNavigation(bool blender, bool trackpadScroll, double trackpadSpeed = 4.0, double zoomSpeed = 1.0);

  /// Pan and zoom so every node is visible.
  void frameAll();

  /// Drawing: 0 = auto (software over Remote Desktop, OpenGL otherwise), 1 = OpenGL, 2 = software.
  static void setRenderMode(int mode);
  /// Overlay the last frame's draw time (ms) in the corner, to measure optimisations.
  static void setShowFrameTime(bool show);
  /// Below this zoom, embedded widgets are hidden (plain node boxes).
  static constexpr double lowDetailZoom = 0.4;


public slots:

  void scaleUp();

  void scaleDown();
  
  void deleteSelectedNodes();
  
  void duplicateSelectedNode();

  void copySelectedNodes();

  void pasteSelectedNodes();

protected:

  void contextMenuEvent(QContextMenuEvent *event) override;

  void wheelEvent(QWheelEvent *event) override;

  bool event(QEvent *event) override;

  void keyPressEvent(QKeyEvent *event) override;

  void keyReleaseEvent(QKeyEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;

  void mouseMoveEvent(QMouseEvent *event) override;

  void mouseReleaseEvent(QMouseEvent *event) override;

  void drawBackground(QPainter* painter, const QRectF& r) override;

  void drawForeground(QPainter* painter, const QRectF& r) override;

  void paintEvent(QPaintEvent *event) override;

  void showEvent(QShowEvent *event) override;

  void addAnchor(int index);
  void goToAnchor(int index);

signals: 
  void nodeNotFound(const QString &str);

protected:

  FlowScene * scene();

private:

  QAction* _clearSelectionAction;
  QAction* _deleteSelectionAction;
  QAction* _duplicateSelectionAction;
  QAction* _copymultiplenodes;
  QAction* _pastemultiplenodes;
  QAction* _undoAction;
  QAction* _redoAction;

  std::vector<QAction*> anchorActions;

  QPointF _clickPos;

  enum class NavDrag { None, Pan, Zoom };
  NavDrag _navDrag = NavDrag::None;
  QPoint _navLastPos;
  void panByView(QPointF viewDelta);
  void zoomBy(double factor);

  void applyRenderMode();
  void updateLevelOfDetail();
  void beginInteraction();          // anti-aliasing off while panning/zooming, back on when idle
  QTimer *_interactionTimer = nullptr;
  bool _lowDetail = false;
  double _lastFrameMs = 0;
  static std::set<FlowView*> s_views;
  static int s_renderMode;
  static bool s_showFrameTime;

  static bool s_blender;
  static bool s_trackpadScroll;
  static double s_trackpadSpeed;
  static double s_zoomSpeed;
  QString _lastWheel;           // raw last wheel event, shown with the frame-time overlay

  FlowScene* _scene;
};
}
