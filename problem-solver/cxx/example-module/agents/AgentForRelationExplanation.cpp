#include "AgentForRelationExplanation.hpp"

#include <algorithm>
#include <queue>
#include <utility>

#include <sc-memory/sc_memory.hpp>

#include "keynodes/Keynodes.hpp"

AgentForRelationExplanation::AgentForRelationExplanation()
{
  // для подробной отладки обхода подними уровень до utils::ScLogLevel::Debug
  m_logger = utils::ScLogger(
      utils::ScLogger::ScLogType::File, "logs/AgentForRelationExplanation.log", utils::ScLogLevel::Info);
}

ScAddr AgentForRelationExplanation::GetActionClass() const
{
  return Keynodes::action_explain_relation;
}

bool AgentForRelationExplanation::CheckInitiationCondition(ScActionInitiatedEvent const & event)
{
  ScAddr const actionAddr = event.GetArcTargetElement();

  // реагируем только на действия нашего класса
  return m_context.CheckConnector(Keynodes::action_explain_relation, actionAddr, ScType::ConstPermPosArc);
}

ScResult AgentForRelationExplanation::DoProgram(ScAction & action)
{
  // ВХОД: rrel_1 — первое понятие, rrel_2 — второе понятие
  auto const & [startAddr, finishAddr] = action.GetArguments<2>();

  m_logger.Debug(
      "Arguments: rrel_1 = `", m_context.GetElementSystemIdentifier(startAddr),
      "`, rrel_2 = `", m_context.GetElementSystemIdentifier(finishAddr), "`");

  if (!m_context.IsElement(startAddr) || !m_context.IsElement(finishAddr))
  {
    m_logger.Error("Both concept arguments must be specified.");
    return action.FinishWithError();
  }

  if (startAddr == finishAddr)
  {
    m_logger.Error("Start and finish concepts must not match.");
    return action.FinishWithError();
  }

  try
  {
    std::vector<Connection> path;
    if (!SearchPath(startAddr, finishAddr, path))
    {
      // пути в базе знаний нет — это недостаточность данных, а не отсутствие связи
      ReportInsufficientData(action, startAddr, finishAddr);
      return action.FinishUnsuccessfully();
    }

    ScStructure result = m_context.GenerateStructure();
    AddPathToResult(startAddr, finishAddr, path, result);

    action.SetResult(result);
    m_logger.Info("Path found. Length: ", path.size());
    return action.FinishSuccessfully();
  }
  catch (utils::ScException const & exception)
  {
    m_logger.Error("ScException in DoProgram: ", exception.Message());
    return action.FinishWithError();
  }
  catch (std::exception const & exception)
  {
    m_logger.Error("std::exception in DoProgram: ", exception.what());
    return action.FinishWithError();
  }
}

std::vector<AgentForRelationExplanation::Connection> AgentForRelationExplanation::GetConnections(
    ScAddr const & conceptAddr)
{
  std::vector<Connection> connections;
  size_t rawConnectors = 0;

  // bundleSource -> bundle -> bundleTarget — связка ровно так, как она лежит в БЗ
  auto const & AppendConnection =
      [&](ScAddr const & bundleSource, ScAddr const & bundle, ScAddr const & bundleTarget)
  {
    // сосед — другой конец связки, независимо от направления обхода
    ScAddr const & otherAddr = (bundleSource == conceptAddr) ? bundleTarget : bundleSource;
    if (otherAddr == conceptAddr)
      return;
    if (!IsConcept(otherAddr))
      return;

    ScAddr relationAddr = ScAddr::Empty;
    ScAddr relationArcAddr = ScAddr::Empty;
    bool hasRoleMarker = false;

    // ищем отношение-маркер связки
    ScIterator3Ptr const markers = m_context.CreateIterator3(ScType::Unknown, ScType::ConstPermPosArc, bundle);
    while (markers->Next())
    {
      ScAddr const & candidateAddr = markers->Get(0);
      if (!m_context.IsElement(relationAddr) && IsNonRoleRelation(candidateAddr))
      {
        relationAddr = candidateAddr;
        relationArcAddr = markers->Get(1);
      }
      else if (IsRoleRelation(candidateAddr))
        hasRoleMarker = true;
    }

    // связка помечена ТОЛЬКО ролями — это не ребро графа понятий
    if (!m_context.IsElement(relationAddr) && hasRoleMarker)
      return;

    // маркера нет совсем: принимаем немаркированные дуги/рёбра общего вида
    connections.push_back({bundleSource, bundleTarget, bundle, relationAddr, relationArcAddr});
  };

  // Рёбра графа понятий — только связки отношений (дуги и рёбра общего вида).
  // Дуги принадлежности (класс -> экземпляр, раздел -> элемент, действие -> аргумент)
  // намеренно НЕ обходятся: иначе путь пойдёт через служебные сущности БЗ.
  ScType const connectorTypes[] = {ScType::ConstCommonArc, ScType::ConstCommonEdge};

  for (ScType const & connectorType : connectorTypes)
  {
    ScIterator3Ptr const outgoing = m_context.CreateIterator3(conceptAddr, connectorType, ScType::Unknown);
    while (outgoing->Next())
    {
      ++rawConnectors;
      AppendConnection(conceptAddr, outgoing->Get(1), outgoing->Get(2));
    }

    ScIterator3Ptr const incoming = m_context.CreateIterator3(ScType::Unknown, connectorType, conceptAddr);
    while (incoming->Next())
    {
      ++rawConnectors;
      AppendConnection(incoming->Get(0), incoming->Get(1), conceptAddr);
    }
  }

  m_logger.Debug(
      "Node `", m_context.GetElementSystemIdentifier(conceptAddr), "`: connectors = ", rawConnectors,
      ", accepted connections = ", connections.size());

  return connections;
}

bool AgentForRelationExplanation::SearchPath(
    ScAddr const & startAddr,
    ScAddr const & finishAddr,
    std::vector<Connection> & path)
{
  ScAddrToValueUnorderedMap<Connection> cameFrom;
  ScAddrUnorderedSet visited;
  std::queue<std::pair<ScAddr, size_t>> queue;

  visited.insert(startAddr);
  queue.push({startAddr, 0});
  bool isFound = false;

  while (!queue.empty() && !isFound)
  {
    auto const [currentAddr, depth] = queue.front();
    queue.pop();

    if (depth >= MAX_RELATION_PATH_LENGTH)
      continue;

    if (visited.size() > MAX_VISITED_CONCEPTS)
    {
      m_logger.Warning("Search limit reached: too many concepts visited.");
      break;
    }

    for (Connection const & connection : GetConnections(currentAddr))
    {
      // сосед — другой конец связки относительно текущей вершины
      ScAddr const neighborAddr =
          (connection.fromAddr == currentAddr) ? connection.toAddr : connection.fromAddr;

      if (visited.count(neighborAddr) != 0)
        continue;

      visited.insert(neighborAddr);
      cameFrom.emplace(neighborAddr, connection);

      if (neighborAddr == finishAddr)
      {
        isFound = true;
        break;
      }
      queue.push({neighborAddr, depth + 1});
    }
  }

  if (!isFound)
  {
    m_logger.Debug("Path NOT found. Visited: ", visited.size());
    return false;
  }

  // восстановление пути от конца к началу (с защитой от зацикливания)
  ScAddr currentAddr = finishAddr;
  size_t guard = 0;
  while (currentAddr != startAddr)
  {
    auto const it = cameFrom.find(currentAddr);
    if (it == cameFrom.end() || ++guard > MAX_RELATION_PATH_LENGTH + 1)
    {
      m_logger.Error("Path reconstruction failed.");
      path.clear();
      return false;
    }

    Connection const & connection = it->second;
    path.push_back(connection);
    currentAddr = (connection.fromAddr == currentAddr) ? connection.toAddr : connection.fromAddr;
  }

  std::reverse(path.begin(), path.end());
  return true;
}

void AgentForRelationExplanation::AddPathToResult(
    ScAddr const & startAddr,
    ScAddr const & finishAddr,
    std::vector<Connection> const & path,
    ScStructure & result)
{
  // sc-узел, обозначающий путь от понятия X к понятию Y
  ScAddr const pathAddr = m_context.GenerateNode(ScType::ConstNode);
  result << pathAddr;

  for (size_t i = 0; i < path.size(); ++i)
  {
    Connection const & connection = path[i];

    // сама связка отношения, уже существующая в БЗ (новые знания не генерируются)
    if (m_context.IsElement(connection.relationAddr))
      result << connection.fromAddr << connection.bundleAddr << connection.toAddr
             << connection.relationAddr << connection.relationArcAddr;
    else
      result << connection.fromAddr << connection.bundleAddr << connection.toAddr;

    // включение связки в путь с порядковым номером: путь -> rrel_(i+1):: связка
    ScAddr const orderArcAddr =
        m_context.GenerateConnector(ScType::ConstPermPosArc, pathAddr, connection.bundleAddr);
    ScAddr const & orderRelationAddr = ScKeynodes::GetRrelIndex(i + 1);
    ScAddr const orderRoleArcAddr =
        m_context.GenerateConnector(ScType::ConstPermPosArc, orderRelationAddr, orderArcAddr);

    result << pathAddr << orderArcAddr << connection.bundleAddr << orderRelationAddr << orderRoleArcAddr;
  }

  // путь -> начало пути':: X
  ScAddr const beginArcAddr = m_context.GenerateConnector(ScType::ConstPermPosArc, pathAddr, startAddr);
  ScAddr const beginRoleArcAddr =
      m_context.GenerateConnector(ScType::ConstPermPosArc, Keynodes::rrel_path_begin, beginArcAddr);
  result << pathAddr << beginArcAddr << startAddr << Keynodes::rrel_path_begin << beginRoleArcAddr;

  // путь -> конец пути':: Y
  ScAddr const endArcAddr = m_context.GenerateConnector(ScType::ConstPermPosArc, pathAddr, finishAddr);
  ScAddr const endRoleArcAddr =
      m_context.GenerateConnector(ScType::ConstPermPosArc, Keynodes::rrel_path_end, endArcAddr);
  result << pathAddr << endArcAddr << finishAddr << Keynodes::rrel_path_end << endRoleArcAddr;
}

void AgentForRelationExplanation::ReportInsufficientData(
    ScAction & action,
    ScAddr const & startAddr,
    ScAddr const & finishAddr)
{
  std::string const reason = "недостаточность данных: в базе знаний не найден путь от `"
                             + m_context.GetElementSystemIdentifier(startAddr) + "` к `"
                             + m_context.GetElementSystemIdentifier(finishAddr) + "`";
  m_logger.Warning(reason);

  // причина фиксируется в БЗ как пояснение* к действию (результат при этом остаётся пустым)
  ScAddr const reasonLinkAddr = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(reasonLinkAddr, reason);
  ScAddr const reasonArcAddr = m_context.GenerateConnector(ScType::ConstCommonArc, action, reasonLinkAddr);
  m_context.GenerateConnector(ScType::ConstPermPosArc, Keynodes::nrel_explanation, reasonArcAddr);
}

bool AgentForRelationExplanation::IsConcept(ScAddr const & element)
{
  if (!m_context.IsElement(element))
    return false;

  ScType const type = m_context.GetElementType(element);

  // вершинами графа могут быть только sc-узлы (не sc-ссылки и не sc-коннекторы)
  if (!type.IsNode() || type.IsLink())
    return false;

  // сами отношения вершинами графа понятий не являются.
  // ВАЖНО: проверки ниже сравнивают BitAnd с ПОЛНОЙ маской, а не с нулём:
  // семантические подтипы в sc-machine содержат базовый бит sc_type_node,
  // поэтому `BitAnd(...) != 0` истинно для любого узла и отклоняло всё подряд.
  if (IsRoleRelation(element) || IsNonRoleRelation(element))
    return false;

  // разделы БЗ и прочие sc-структуры — служебные сущности, а не понятия предметной области
  if (type.BitAnd(*ScType::NodeStructure) == *ScType::NodeStructure)
    return false;

  return true;
}

bool AgentForRelationExplanation::IsNonRoleRelation(ScAddr const & element)
{
  if (!m_context.IsElement(element))
    return false;

  ScType const type = m_context.GetElementType(element);
  return (type.BitAnd(*ScType::NodeNonRole) == *ScType::NodeNonRole)
         || m_context.CheckConnector(Keynodes::sc_node_non_role_relation, element, ScType::ConstPermPosArc);
}

bool AgentForRelationExplanation::IsRoleRelation(ScAddr const & element)
{
  if (!m_context.IsElement(element))
    return false;

  ScType const type = m_context.GetElementType(element);
  return (type.BitAnd(*ScType::NodeRole) == *ScType::NodeRole)
         || m_context.CheckConnector(Keynodes::sc_node_role_relation, element, ScType::ConstPermPosArc);
}
