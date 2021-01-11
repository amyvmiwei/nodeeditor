#include "NodeConnectionInteraction.hpp"

#include <QtCore/QDebug>

#include <iostream>

#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeGeometry.hpp"
#include "NodeGraphicsObject.hpp"
#include "NodeGraphicsScene.hpp"


namespace QtNodes
{

NodeConnectionInteraction::
NodeConnectionInteraction(NodeGraphicsObject & ngo,
                          ConnectionGraphicsObject & cgo,
                          NodeGraphicsScene &  scene)
  : _ngo(ngo)
  , _cgo(cgo)
  , _scene(scene)
{}


bool
NodeConnectionInteraction::
canConnect(PortIndex * portIndex) const
{
  // 1) Connection requires a port

  PortType requiredPort = _cgo.connectionState().requiredPort();

  if (requiredPort == PortType::None)
  {
    return false;
  }

  NodeId connectedNodeId =
    getNodeId(oppositePort(requiredPort), _cgo.connectionId());

  // 1.5) Forbid connecting the node to itself

  if (_ngo.nodeId() == connectedNodeId)
    return false;

  // 2) connection point is on top of the node port

  QPointF connectionPoint =
    _cgo.sceneTransform().map(_cgo.endPoint(requiredPort));

  *portIndex = nodePortIndexUnderScenePoint(requiredPort,
                                            connectionPoint);

  if (*portIndex == InvalidPortIndex)
  {
    return false;
  }

  // 3) Node port is vacant

  // port should be empty
  if (!nodePortIsEmpty(requiredPort, *portIndex))
    return false;

  // 4) Connection type equals node port type.

  GraphModel & model = _ngo.nodeScene()->graphModel();

  NodeDataType const connectionDataType =
    model.portData(connectedNodeId,
                   oppositePort(requiredPort),
                   getPortIndex(oppositePort(requiredPort), _cgo.connectionId()),
                   PortRole::DataType).value<NodeDataType>();

  NodeDataType const targetDataType =
    model.portData(_ngo.nodeId(),
                   requiredPort,
                   *portIndex,
                   PortRole::DataType).value<NodeDataType>();

  if (connectionDataType.id != targetDataType.id)
  {
    //if (requiredPort == PortType::In)
    //{
    //converter = _scene->registry().getTypeConverter(connectionDataType, candidateNodeDataType);
    //}
    //else if (requiredPort == PortType::Out)
    //{
    //converter = _scene->registry().getTypeConverter(candidateNodeDataType, connectionDataType);
    //}

    //return (converter != nullptr);

    return false;
  }

  return true;
}


bool
NodeConnectionInteraction::
tryConnect() const
{
  // 1) Check conditions from 'canConnect'

  //TypeConverter converter;

  PortIndex targetPortIndex = InvalidPortIndex;
  if (!canConnect(&targetPortIndex))
  {
    return false;
  }

  // 1.5) If the connection is possible but a type conversion is
  // needed, assign a convertor to connection
  //if (converter)
  //{
  //_connectionId->setTypeConverter(converter);
  //}

  // 2) Create new connection

  ConnectionId incompleteConnectionId = _cgo.connectionId();

  PortType requiredPort = _cgo.connectionState().requiredPort();

  ConnectionId newConnectionId =
    makeCompleteConnectionId(incompleteConnectionId,
                             _ngo.nodeId(),
                             targetPortIndex);

  _ngo.nodeScene()->useDraftConnection(newConnectionId);

  // 4) Adjust Connection geometry

  _ngo.moveConnections();
  _ngo.nodeState().resetReactionToConnection();

  // Repaint old connected Node
  NodeGraphicsObject * oppositeNgo =
    _scene.nodeGraphicsObject(getNodeId(oppositePort(requiredPort),
                              newConnectionId));
  oppositeNgo->update();


  // 5) Poke model to intiate data transfer

  //auto outNode = _connectionId->getNode(PortType::Out);
  //if (outNode)
  //{
  //PortIndex outPortIndex = _connectionId->getPortIndex(PortType::Out);
  //outNode->onDataUpdated(outPortIndex);
  //}

  return true;
}


bool
NodeConnectionInteraction::
disconnect(PortType portToDisconnect) const
{
  ConnectionId connectionId = _cgo.connectionId();

  ConnectionId incompleteConnectionId =
    makeIncompleteConnectionId(connectionId,
                               portToDisconnect);

  // Fetch connection from the scene.
  // Connection is removed from the graph inside.
  std::unique_ptr<ConnectionGraphicsObject> uniqueCgo =
    _scene.deleteConnection(connectionId);

  // Repaint connection points.
  NodeId connectedNodeId =
    getNodeId(oppositePort(portToDisconnect), connectionId);
  _scene.nodeGraphicsObject(connectedNodeId)->update();

  NodeId disconnectedNodeId =
    getNodeId(portToDisconnect, connectionId);
  _scene.nodeGraphicsObject(disconnectedNodeId)->update();

  // Converted to "draft" connection with the new incomplete id.
  return _scene.makeDraftConnection(std::move(uniqueCgo),
                                    incompleteConnectionId);
}


// ------------------ util functions below

PortType
NodeConnectionInteraction::
connectionRequiredPort() const
{
  auto const &state = _cgo.connectionState();

  return state.requiredPort();
}


QPointF
NodeConnectionInteraction::
nodePortScenePosition(PortType portType, PortIndex portIndex) const
{
  NodeGeometry geometry(_ngo);

  QPointF p =
    geometry.portScenePosition(portType, portIndex, _ngo.sceneTransform());

  return p;
}


PortIndex
NodeConnectionInteraction::
nodePortIndexUnderScenePoint(PortType portType,
                             QPointF const & scenePoint) const
{
  NodeGeometry geometry(_ngo);

  QTransform sceneTransform = _ngo.sceneTransform();

  PortIndex portIndex = geometry.checkHitScenePoint(portType,
                                                    scenePoint,
                                                    sceneTransform);
  return portIndex;
}


bool
NodeConnectionInteraction::
nodePortIsEmpty(PortType portType, PortIndex portIndex) const
{
  GraphModel const & model = _ngo.nodeScene()->graphModel();

  auto const & connectedNodes =
    model.connectedNodes(_ngo.nodeId(), portType, portIndex);

  if (connectedNodes.empty())
    return true;

  ConnectionPolicy const outPolicy =
    model.portData(_ngo.nodeId(),
                   portType,
                   portIndex,
                   PortRole::ConnectionPolicy).value<ConnectionPolicy>();

  return (portType == PortType::Out &&
          outPolicy == ConnectionPolicy::Many);

}


}
