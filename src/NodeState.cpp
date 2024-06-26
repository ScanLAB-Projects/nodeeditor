#include "NodeState.hpp"

#include "NodeDataModel.hpp"

#include "Connection.hpp"

using QtNodes::NodeState;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::Connection;

NodeState::
NodeState(std::unique_ptr<NodeDataModel> const &model)
  : _inConnections(model->nPorts(PortType::In))
  , _outConnections(model->nPorts(PortType::Out))
  , _reaction(NOT_REACTING)
  , _reactingPortType(PortType::None)
  , _resizing(false)
  , _model(model)
{}


std::vector<NodeState::ConnectionPtrSet> const &
NodeState::
getEntries(PortType portType) const
{
  if (portType == PortType::In)
    return _inConnections;
  else
    return _outConnections;
}


std::vector<NodeState::ConnectionPtrSet> &
NodeState::
getEntries(PortType portType)
{
  if (portType == PortType::In)
    return _inConnections;
  else
    return _outConnections;
}
   

NodeState::ConnectionPtrSet
NodeState::
connections(PortType portType, PortIndex portIndex) const
{
  auto const &connections = getEntries(portType);

  return connections[portIndex];
}


std::vector<Connection*>
NodeState::
allConnections() const
{
  std::vector<Connection*> res;
  std::vector<NodeState::ConnectionPtrSet> ins = getEntries(PortType::In);
  for(int i=0; i<ins.size(); i++)
  {
    for(auto &connection : ins[i])
    {
      res.push_back(connection.second);
    }
  }
  std::vector<NodeState::ConnectionPtrSet> outs = getEntries(PortType::Out);
  for(int i=0; i<outs.size(); i++)
  {
    for(auto &connection : outs[i])
    {
      res.push_back(connection.second);
    }
  }

  return res;
}


void
NodeState::
setConnection(PortType portType,
              PortIndex portIndex,
              Connection& connection)
{
  auto &connections = getEntries(portType);

  connections[portIndex].insert(std::make_pair(connection.id(),
                                               &connection));
}


void
NodeState::
eraseConnection(PortType portType,
                PortIndex portIndex,
                QUuid id)
{
  getEntries(portType)[portIndex].erase(id);
}


void
NodeState::
eraseInputAtIndex(PortIndex portIndex)
{
  
}

NodeState::ReactToConnectionState
NodeState::
reaction() const
{
  return _reaction;
}


PortType
NodeState::
reactingPortType() const
{
  return _reactingPortType;
}


NodeDataType
NodeState::
reactingDataType() const
{
  return _reactingDataType;
}


void
NodeState::
setReaction(ReactToConnectionState reaction,
            PortType reactingPortType,
            NodeDataType reactingDataType)
{
  _reaction = reaction;

  _reactingPortType = reactingPortType;

  _reactingDataType = reactingDataType;
}


bool
NodeState::
isReacting() const
{
  return _reaction == REACTING;
}


void
NodeState::
setResizing(bool resizing)
{
  _resizing = resizing;
}


bool
NodeState::
resizing() const
{
  return _resizing;
}



void
NodeState::
updateEntries()
{
 _inConnections.resize(_model->nPorts(PortType::In));
 _outConnections.resize(_model->nPorts(PortType::Out)); 
}