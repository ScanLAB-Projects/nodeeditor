#pragma once

#include <QtWidgets/QGraphicsView>

#include "Export.hpp"
  
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
  static void setNavigation(bool blender, bool trackpadScroll);

  /// Pan and zoom so every node is visible.
  void frameAll();


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

  void keyPressEvent(QKeyEvent *event) override;

  void keyReleaseEvent(QKeyEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;

  void mouseMoveEvent(QMouseEvent *event) override;

  void mouseReleaseEvent(QMouseEvent *event) override;

  void drawBackground(QPainter* painter, const QRectF& r) override;

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

  static bool s_blender;
  static bool s_trackpadScroll;

  FlowScene* _scene;
};
}
