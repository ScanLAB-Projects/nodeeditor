#include "FlowScene.hpp"

#include <iostream>
#include <stdexcept>

#include <QtWidgets/QGraphicsSceneMoveEvent>
#include <QtWidgets/QFileDialog>
#include <QtCore/QByteArray>
#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QFile>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <QDebug>

#include "Node.hpp"
#include "Group.hpp"
#include "NodeGraphicsObject.hpp"

#include "NodeGraphicsObject.hpp"
#include "ConnectionGraphicsObject.hpp"

#include "Connection.hpp"

#include "FlowView.hpp"
#include "DataModelRegistry.hpp"

using QtNodes::FlowScene;
using QtNodes::Node;
using QtNodes::NodeGraphicsObject;
using QtNodes::Connection;
using QtNodes::DataModelRegistry;
using QtNodes::NodeDataModel;
//using QtNodes::Properties;
using QtNodes::PortType;
using QtNodes::PortIndex;

FlowScene::
FlowScene(std::shared_ptr<DataModelRegistry> registry)
  : _registry(registry)
{
  setItemIndexMethod(QGraphicsScene::NoIndex);
  
  ResetHistory();
  UpdateHistory();
  
  auto UpdateLamda = [this](Node& n, const QPointF& p)
  {
	  UpdateHistory();
  };
  connect(this, &FlowScene::nodeMoveFinished, this, UpdateLamda);

  anchors.resize(10);  
}


FlowScene::
~FlowScene()
{
  clearScene();
}


//------------------------------------------------------------------------------

std::shared_ptr<Connection>
FlowScene::
createConnection(PortType connectedPort,
                 Node& node,
                 PortIndex portIndex)
{
  auto connection = std::make_shared<Connection>(connectedPort, node, portIndex);

  auto cgo = std::make_unique<ConnectionGraphicsObject>(*this, *connection);

  // after this function connection points are set to node port
  connection->setGraphicsObject(std::move(cgo));

  _connections[connection->id()] = connection;

  connectionCreated(*connection);
  return connection;
}


std::shared_ptr<Connection>
FlowScene::
createConnection(Node& nodeIn,
                 PortIndex portIndexIn,
                 Node& nodeOut,
                 PortIndex portIndexOut)
{

  auto connection =
    std::make_shared<Connection>(nodeIn,
                                 portIndexIn,
                                 nodeOut,
                                 portIndexOut);

  auto cgo = std::make_unique<ConnectionGraphicsObject>(*this, *connection);

  nodeIn.nodeState().setConnection(PortType::In, portIndexIn, *connection);
  nodeOut.nodeState().setConnection(PortType::Out, portIndexOut, *connection);

  // after this function connection points are set to node port
  connection->setGraphicsObject(std::move(cgo));

  // trigger data propagation
  nodeOut.onDataUpdated(portIndexOut);

  _connections[connection->id()] = connection;

  connectionCreated(*connection);
  
  return connection;
}


std::shared_ptr<Connection>
FlowScene::
restoreConnection(QJsonObject const &connectionJson)
{
  QUuid nodeInId  = QUuid(connectionJson["in_id"].toString());
  QUuid nodeOutId = QUuid(connectionJson["out_id"].toString());

  PortIndex portIndexIn  = connectionJson["in_index"].toInt();
  PortIndex portIndexOut = connectionJson["out_index"].toInt();

  auto nodeIn  = _nodes[nodeInId].get();
  auto nodeOut = _nodes[nodeOutId].get();

  return createConnection(*nodeIn, portIndexIn, *nodeOut, portIndexOut);
}

void FlowScene::pasteConnection(QJsonObject const &connectionJson, QUuid newIn, QUuid newOut)
{
  QUuid nodeInId  = QUuid(connectionJson["in_id"].toString());
  QUuid nodeOutId = QUuid(connectionJson["out_id"].toString());

  PortIndex portIndexIn  = connectionJson["in_index"].toInt();
  PortIndex portIndexOut = connectionJson["out_index"].toInt();

  auto nodeIn  = _nodes[newIn].get();
  auto nodeOut = _nodes[newOut].get();

  createConnection(*nodeIn, portIndexIn, *nodeOut, portIndexOut);
}


void
FlowScene::
deleteConnection(Connection& connection)
{
  connectionDeleted(connection);
  connection.removeFromNodes();
  _connections.erase(connection.id());
}


Node&
FlowScene::
createNode(std::unique_ptr<NodeDataModel> && dataModel)
{
  auto node = std::make_unique<Node>(std::move(dataModel));
  auto ngo  = std::make_unique<NodeGraphicsObject>(*this, *node);

  node->setGraphicsObject(std::move(ngo));

  auto nodePtr = node.get();
  _nodes[node->id()] = std::move(node);

  nodeCreated(*nodePtr);
  
  return *nodePtr;
}

void FlowScene::createGroup() {

  std::vector<std::shared_ptr<Node>> selection =  selectedNodes();
  auto group = std::make_unique<Group>(*this);
  
  for(int i=0; i<selection.size(); i++) {
    group->AddNode(selection[i]);
  }
  
  _groups[group->id()] = std::move(group);
}


Node&
FlowScene::
restoreNode(QJsonObject const& nodeJson)
{
  QString modelName = nodeJson["model"].toObject()["name"].toString();
  auto dataModel = registry().create(modelName); //This is where it looks for the node by name

  if (!dataModel)
    throw std::logic_error(std::string("No registered model with name ") +
                            modelName.toLocal8Bit().data());
  auto node = std::make_unique<Node>(std::move(dataModel));
  auto ngo  = std::make_unique<NodeGraphicsObject>(*this, *node);
  node->setGraphicsObject(std::move(ngo));
  node->restore(nodeJson);

  auto nodePtr = node.get();
  _nodes[node->id()] = std::move(node);
  nodeCreated(*nodePtr);
  return *nodePtr;
}

QUuid FlowScene::pasteNode(QJsonObject &nodeJson, QPointF nodeGroupCentroid, QPointF mousePos) {
  QString modelName = nodeJson["model"].toObject()["name"].toString();
  auto dataModel = registry().create(modelName);

  if (!dataModel)
    throw std::logic_error(std::string("No registered model with name ") +
                           modelName.toLocal8Bit().data());

  auto node = std::make_unique<Node>(std::move(dataModel));
  auto ngo  = std::make_unique<NodeGraphicsObject>(*this, *node);

  node->setGraphicsObject(std::move(ngo));
  

  QUuid newId = QUuid::createUuid();
  node->paste(nodeJson, newId);

  QPointF offset = node->nodeGraphicsObject().pos() - nodeGroupCentroid;


  QPointF pos = mousePos + (node->nodeGraphicsObject().pos() - nodeGroupCentroid);
  node->nodeGraphicsObject().setPos(pos);

  auto nodePtr = node.get();
  _nodes[node->id()] = std::move(node);
  nodeCreated(*nodePtr);
  return newId;
}



void
FlowScene::
removeNode(Node& node)
{
  // call signal
  nodeDeleted(node);

  auto deleteConnections =
    [&node, this] (PortType portType)
    {
      auto nodeState = node.nodeState();
      auto const & nodeEntries = nodeState.getEntries(portType);

      for (auto &connections : nodeEntries)
      {
        for (auto const &pair : connections)
          deleteConnection(*pair.second);
      }
    };

  deleteConnections(PortType::In);
  deleteConnections(PortType::Out);

  _nodes.erase(node.id());
}


DataModelRegistry&
FlowScene::
registry() const
{
  return *_registry;
}


void
FlowScene::
setRegistry(std::shared_ptr<DataModelRegistry> registry)
{
  _registry = registry;
}


void
FlowScene::
iterateOverNodes(std::function<void(Node*)> visitor)
{
  for (const auto& _node : _nodes)
  {
    visitor(_node.second.get());
  }
}


void
FlowScene::
iterateOverNodeData(std::function<void(NodeDataModel*)> visitor)
{
  for (const auto& _node : _nodes)
  {
    visitor(_node.second->nodeDataModel());
  }
}


void
FlowScene::
iterateOverNodeDataDependentOrder(std::function<void(NodeDataModel*)> visitor)
{
  std::set<QUuid> visitedNodesSet;

  //A leaf node is a node with no input ports, or all possible input ports empty
  auto isNodeLeaf =
    [](Node const &node, NodeDataModel const &model)
    {
      for (size_t i = 0; i < model.nPorts(PortType::In); ++i)
      {
        auto connections = node.nodeState().connections(PortType::In, i);
        if (!connections.empty())
        {
          return false;
        }
      }

      return true;
    };

  //Iterate over "leaf" nodes
  for (auto const &_node : _nodes)
  {
    auto const &node = _node.second;
    auto model       = node->nodeDataModel();

    if (isNodeLeaf(*node, *model))
    {
      visitor(model);
      visitedNodesSet.insert(node->id());
    }
  }

  auto areNodeInputsVisitedBefore =
    [&](Node const &node, NodeDataModel const &model)
    {
      for (size_t i = 0; i < model.nPorts(PortType::In); ++i)
      {
        auto connections = node.nodeState().connections(PortType::In, i);

        for (auto& conn : connections)
        {
          if (visitedNodesSet.find(conn.second->getNode(PortType::Out)->id()) == visitedNodesSet.end())
          {
            return false;
          }
        }
      }

      return true;
    };

  //Iterate over dependent nodes
  while (_nodes.size() != visitedNodesSet.size())
  {
    for (auto const &_node : _nodes)
    {
      auto const &node = _node.second;
      if (visitedNodesSet.find(node->id()) != visitedNodesSet.end())
        continue;

      auto model = node->nodeDataModel();

      if (areNodeInputsVisitedBefore(*node, *model))
      {
        visitor(model);
        visitedNodesSet.insert(node->id());
      }
    }
  }
}


QPointF
FlowScene::
getNodePosition(const Node& node) const
{
  return node.nodeGraphicsObject().pos();
}


void
FlowScene::
setNodePosition(Node& node, const QPointF& pos) const
{
  node.nodeGraphicsObject().setPos(pos);
  node.nodeGraphicsObject().moveConnections();
}


QSizeF
FlowScene::
getNodeSize(const Node& node) const
{
  return QSizeF(node.nodeGeometry().width(), node.nodeGeometry().height());
}


std::unordered_map<QUuid, std::shared_ptr<Node> > const &
FlowScene::
nodes() const
{
  return _nodes;
}


std::unordered_map<QUuid, std::shared_ptr<Connection> > const &
FlowScene::
connections() const
{
  return _connections;
}


std::vector<std::shared_ptr<Node>>
FlowScene::
selectedNodes() const
{
  QList<QGraphicsItem*> graphicsItems = selectedItems();

  std::vector<std::shared_ptr<Node>> ret;
  ret.reserve(graphicsItems.size());

  for (QGraphicsItem* item : graphicsItems)
  {
    auto ngo = qgraphicsitem_cast<NodeGraphicsObject*>(item);

    if (ngo != nullptr)
    {
      Node* n = &(ngo->node());
      std::shared_ptr<Node> ptr(n);
      ret.push_back(ptr);
    }
  }

  return ret;
}


//------------------------------------------------------------------------------

void
FlowScene::
clearScene()
{
  //Manual node cleanup. Simply clearing the holding datastructures doesn't work, the code crashes when
  // there are both nodes and connections in the scene. (The data propagation internal logic tries to propagate
  // data through already freed connections.)
  std::vector<Node*> nodesToDelete;
  for (auto& node : _nodes)
  {
    nodesToDelete.push_back(node.second.get());
  }

  for (auto& node : nodesToDelete)
  {
    removeNode(*node);
  }
}


void
FlowScene::
save() const
{
  QString fileName =
    QFileDialog::getSaveFileName(nullptr,
                                 tr("Open Flow Scene"),
                                 QDir::homePath(),
                                 tr("Flow Scene Files (*.flow)"));

  if (!fileName.isEmpty())
  {
    if (!fileName.endsWith("flow", Qt::CaseInsensitive))
      fileName += ".flow";

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
      file.write(saveToMemory());
    }
  }
}


void
FlowScene::
load()
{
  clearScene();

  //-------------

  QString fileName =
    QFileDialog::getOpenFileName(nullptr,
                                 tr("Open Flow Scene"),
                                 QDir::homePath(),
                                 tr("Flow Scene Files (*.flow)"));

  if (!QFileInfo::exists(fileName))
    return;

  QFile file(fileName);

  if (!file.open(QIODevice::ReadOnly))
    return;

  QByteArray wholeFile = file.readAll();

  loadFromMemory(wholeFile);
}


QByteArray
FlowScene::
saveToMemory() const
{
  QJsonObject sceneJson;

  QJsonArray nodesJsonArray;

  for (auto const & pair : _nodes)
  {
    auto const &node = pair.second;

    nodesJsonArray.append(node->save());
  }

  sceneJson["nodes"] = nodesJsonArray;

  QJsonArray connectionJsonArray;
  for (auto const & pair : _connections)
  {
    auto const &connection = pair.second;

    QJsonObject connectionJson = connection->save();

    if (!connectionJson.isEmpty())
      connectionJsonArray.append(connectionJson);
  }

  sceneJson["connections"] = connectionJsonArray;

  QJsonArray anchorsJsonArray;
  for(int i=0; i<anchors.size(); i++) {
    QJsonObject anchor;
    anchor["position_x"] = anchors[i].position.x();
    anchor["position_y"] = anchors[i].position.y();
    anchor["scale"] = anchors[i].scale;
    anchorsJsonArray.append(anchor);
  }

  sceneJson["anchors"] = anchorsJsonArray;

  QJsonDocument document(sceneJson);

  return document.toJson();
}


void FlowScene::saveToClipBoard()
{

}

void
FlowScene::
loadFromMemory(const QByteArray& data)
{
  QJsonObject const jsonDocument = QJsonDocument::fromJson(data).object();

  QJsonArray nodesJsonArray = jsonDocument["nodes"].toArray();

  for (int i = 0; i < nodesJsonArray.size(); ++i)
  {
    restoreNode(nodesJsonArray[i].toObject());
  }

  QJsonArray connectionJsonArray = jsonDocument["connections"].toArray();

  for (int i = 0; i < connectionJsonArray.size(); ++i)
  {
    restoreConnection(connectionJsonArray[i].toObject());
  }

  if(jsonDocument.contains("anchors")) {
    QJsonArray anchorsJsonArray = jsonDocument["anchors"].toArray();
    for(int i=0; i<anchorsJsonArray.size(); i++) {
      Anchor a;
      QJsonObject anchorObject =anchorsJsonArray[i].toObject();
      float x = anchorObject["position_x"].toDouble();
      float y = anchorObject["position_y"].toDouble();
      float scale = anchorObject["scale"].toDouble();
      a.position= QPointF(x, y);
      a.scale = scale;
      anchors[i] = a;
    }
  }
}

void FlowScene::Undo()
{
	std::cout << "Undo" << std::endl; 
	if(historyInx > 1)
	{
		writeToHistory = false; 
		clearScene();
		historyInx--;
		loadFromMemory(history[historyInx - 1].data);
		writeToHistory = true; 
	}
}
  
void FlowScene::Redo()
{
	std::cout << "Redo" << std::endl; 
	writeToHistory = false; 
	if(historyInx < history.size())
	{
		std::cout << "historyInx:" << historyInx << " history.size():" << history.size() << std::endl; 
		clearScene();
		loadFromMemory(history[historyInx].data);
		historyInx++;
	}
	else
	{
		std::cout << "Could not redo" << std::endl;
	}
	writeToHistory = true;
}

void FlowScene::UpdateHistory()
{
	if(writeToHistory)
	{
		std::cout << "UpdateHistory" << std::endl; 
		
		SceneHistory sh;
		sh.data = saveToMemory();
		
		if(historyInx < history.size())
		{
			history.resize(historyInx);
			history.push_back(sh);	
		}
		else
		{
			history.push_back(sh);
		}
		
		historyInx++;
	}
}


void FlowScene::ResetHistory()
{
	historyInx = 0; 
	writeToHistory = true; 
	history.clear();
}

int FlowScene::GetHistoryIndex() {
  return historyInx;
}




//------------------------------------------------------------------------------
namespace QtNodes
{

Node*
locateNodeAt(QPointF scenePoint, FlowScene &scene,
             QTransform viewTransform)
{
  // items under cursor
  QList<QGraphicsItem*> items =
    scene.items(scenePoint,
                Qt::IntersectsItemShape,
                Qt::DescendingOrder,
                viewTransform);

  //// items convertable to NodeGraphicsObject
  std::vector<QGraphicsItem*> filteredItems;

  std::copy_if(items.begin(),
               items.end(),
               std::back_inserter(filteredItems),
               [] (QGraphicsItem * item)
    {
      return (dynamic_cast<NodeGraphicsObject*>(item) != nullptr);
    });

  Node* resultNode = nullptr;

  if (!filteredItems.empty())
  {
    QGraphicsItem* graphicsItem = filteredItems.front();
    auto ngo = dynamic_cast<NodeGraphicsObject*>(graphicsItem);

    resultNode = &ngo->node();
  }

  return resultNode;
}
}
