#include "Node.hpp"

#include <QtCore/QObject>

#include <iostream>

#include "FlowScene.hpp"

#include "NodeGraphicsObject.hpp"
#include "NodeDataModel.hpp"

#include "ConnectionGraphicsObject.hpp"
#include "ConnectionState.hpp"
#include "Connection.hpp"
#include <QtWidgets/QGraphicsView>

using QtNodes::Node;
using QtNodes::NodeGeometry;
using QtNodes::NodeState;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeGraphicsObject;
using QtNodes::PortIndex;
using QtNodes::PortType;
using QtNodes::Connection;


Node::
Node(std::unique_ptr<NodeDataModel> && dataModel)
  : _id(QUuid::createUuid())
  , _nodeDataModel(std::move(dataModel))
  , _nodeState(_nodeDataModel)
  , _nodeGeometry(_nodeDataModel)
  , _nodeGraphicsObject(nullptr)
{
  _nodeGeometry.recalculateSize();

  // propagate data: model => node
  connect(_nodeDataModel.get(), &NodeDataModel::dataUpdated,
          this, &Node::onDataUpdated);

  this->inputSelected.resize(_nodeDataModel->nPorts(PortType::In));
		
}

void 
Node::
setInputSelected(int inx, bool selected)
{
  this->inputSelected[inx] = selected;
}

Node::
~Node() {}

QJsonObject
Node::
save() const
{
  QJsonObject nodeJson;

  nodeJson["id"] = _id.toString();

  nodeJson["model"] = _nodeDataModel->save();

  QJsonObject obj;
  obj["x"] = _nodeGraphicsObject->scenePos().x();
  obj["y"] = _nodeGraphicsObject->scenePos().y();
  nodeJson["position"] = obj;

  return nodeJson;
}

QJsonObject Node::copyWithNewID(QUuid newId) const
{
  QJsonObject nodeJson;

  nodeJson["id"] = newId.toString();
  nodeJson["model"] = _nodeDataModel->save();

  QJsonObject obj;
  obj["x"] = _nodeGraphicsObject->pos().x();
  obj["y"] = _nodeGraphicsObject->pos().y();
  nodeJson["position"] = obj;

  return nodeJson;
}


void 
Node::
updateView()
{
  nodeGeometry().recalculateInOut();
  nodeState().updateEntries();

  nodeGraphicsObject().update();
  // QGraphicsView *view = nodeGraphicsObject().scene()->views().first();
  // view->viewport()->repaint();
}


void
Node::
eraseInputAtIndex(int portIndex)
{
	std::unordered_map<QUuid, Connection*> connections = nodeState().connections(PortType::In, portIndex);
	for (auto& connection : connections) {
		nodeGraphicsObject().flowScene().deleteConnection(connection.second);
	}
}


void
Node::
restore(QJsonObject const& json)
{
  _id = QUuid(json["id"].toString());

  QJsonObject positionJson = json["position"].toObject();
  QPointF     point(positionJson["x"].toDouble(),
                    positionJson["y"].toDouble());
  _nodeGraphicsObject->setPos(point);

  _nodeDataModel->restore(json["model"].toObject());
}

void
Node::
paste(QJsonObject const& json, QUuid ID)
{
  _id = ID;

  QJsonObject positionJson = json["position"].toObject();
  QPointF     point(positionJson["x"].toDouble(),
                    positionJson["y"].toDouble());
  _nodeGraphicsObject->setPos(point);

  _nodeDataModel->restore(json["model"].toObject());
  // this->updateView();
}


QUuid
Node::
id() const
{
  return _id;
}

void Node::setId(QUuid id) {
  this->_id = id;
}



void
Node::
reactToPossibleConnection(PortType reactingPortType,

                          NodeDataType reactingDataType,
                          QPointF const &scenePoint)
{
  QTransform const t = _nodeGraphicsObject->sceneTransform();

  QPointF p = t.inverted().map(scenePoint);

  _nodeGeometry.setDraggingPosition(p);

  _nodeGraphicsObject->update();

  _nodeState.setReaction(NodeState::REACTING,
                         reactingPortType,
                         reactingDataType);
}


void
Node::
resetReactionToConnection()
{
  _nodeState.setReaction(NodeState::NOT_REACTING);
  _nodeGraphicsObject->update();
}


NodeGraphicsObject const &
Node::
nodeGraphicsObject() const
{
  return *_nodeGraphicsObject.get();
}


NodeGraphicsObject &
Node::
nodeGraphicsObject()
{
  return *_nodeGraphicsObject.get();
}


void
Node::
setGraphicsObject(std::unique_ptr<NodeGraphicsObject>&& graphics)
{
  _nodeGraphicsObject = std::move(graphics);

  _nodeGeometry.recalculateSize();
  
  nodeGraphicsObject().setToolTip(_nodeDataModel->toolTipText());
}


NodeGeometry&
Node::
nodeGeometry()
{
  return _nodeGeometry;
}


NodeGeometry const&
Node::
nodeGeometry() const
{
  return _nodeGeometry;
}


NodeState const &
Node::
nodeState() const
{
  return _nodeState;
}


NodeState &
Node::
nodeState()
{
  return _nodeState;
}


NodeDataModel*
Node::
nodeDataModel() const
{
  return _nodeDataModel.get();
}


void
Node::
propagateData(std::shared_ptr<NodeData> nodeData,
              PortIndex inPortIndex) const
{
  _nodeDataModel->setInData(nodeData, inPortIndex);

  //Recalculate the nodes visuals. A data change can result in the node taking more space than before, so this forces a recalculate+repaint on the affected node
  _nodeGraphicsObject->setGeometryChanged();
  _nodeGeometry.recalculateSize();
  _nodeGraphicsObject->update();
  _nodeGraphicsObject->moveConnections();
}


void
Node::
onDataUpdated(PortIndex index)
{
  auto nodeData = _nodeDataModel->outData(index);

  if (_nodeState.getEntries(PortType::Out).size() > 0)
  {
	  auto connections =
		_nodeState.connections(PortType::Out, index);

	  for (auto const & c : connections)
		c.second->propagateData(nodeData);
  }
}

void
Node::
onDataUpdatedConnection(PortIndex index, Connection* connection)
{
  auto nodeData = _nodeDataModel->outData(index);
  connection->propagateData(nodeData);
}
