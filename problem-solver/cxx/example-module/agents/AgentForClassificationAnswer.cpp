#include "AgentForClassificationAnswer.hpp"

#include <sc-memory/sc_memory.hpp>

#include "keynodes/Keynodes.hpp"

AgentForClassificationAnswer::AgentForClassificationAnswer()
{
  m_logger = utils::ScLogger(
      utils::ScLogger::ScLogType::File, "logs/AgentForClassificationAnswer.log", utils::ScLogLevel::Debug);
}

ScAddr AgentForClassificationAnswer::GetActionClass() const
{
  return Keynodes::action_get_classification;
}

ScResult AgentForClassificationAnswer::DoProgram(ScAction & action)
{
  // ВХОД: rrel_1 — понятие (класс), виды которого требуется перечислить
  auto const & [conceptAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(conceptAddr))
  {
    m_logger.Error("Concept argument of question `what kinds of X` is not specified.");
    return action.FinishWithError();
  }

  m_logger.Debug("Question `what kinds of ", m_context.GetElementSystemIdentifier(conceptAddr), "`");

  // 1. Собираем подклассы: понятие => включение*:: подкласс
  std::vector<Kind> kinds;
  ScAddrUnorderedSet usedSubclasses;
  ScIterator5Ptr const it5 = m_context.CreateIterator5(
      conceptAddr, ScType::ConstCommonArc, ScType::Unknown, ScType::ConstPermPosArc, ScKeynodes::nrel_inclusion);
  while (it5->Next())
  {
    ScAddr const & subclassAddr = it5->Get(2);
    if (subclassAddr == conceptAddr || !IsClass(subclassAddr))
      continue;
    if (!usedSubclasses.insert(subclassAddr).second)
      continue;

    kinds.push_back({subclassAddr, it5->Get(1), it5->Get(3)});
  }

  if (kinds.empty())
  {
    m_logger.Warning("Insufficient data: the concept has no subclasses in the knowledge base.");
    return action.FinishUnsuccessfully();
  }

  // 2. Генерируем sc-узел, обозначающий множество видов понятия
  ScAddr const kindsSetAddr = m_context.GenerateNode(ScType::ConstNode);

  ScStructure result = m_context.GenerateStructure();
  result << conceptAddr << kindsSetAddr << ScKeynodes::nrel_inclusion;

  // 3. Для каждого вида строим обе конструкции результата
  for (Kind const & kind : kinds)
  {
    // первая (трёхэлементная) конструкция: множество видов -> дуга принадлежности -> подкласс
    ScAddr const membershipArcAddr =
        m_context.GenerateConnector(ScType::ConstPermPosArc, kindsSetAddr, kind.subclassAddr);
    result << kindsSetAddr << membershipArcAddr << kind.subclassAddr;

    // вторая конструкция: существующая связка включения* между понятием и подклассом
    result << conceptAddr << kind.inclusionBundleAddr << kind.subclassAddr << kind.inclusionRelationArcAddr
           << ScKeynodes::nrel_inclusion;
  }

  action.SetResult(result);
  m_logger.Info("Classification answer formed. Kinds found: ", kinds.size());
  return action.FinishSuccessfully();
}

bool AgentForClassificationAnswer::IsClass(ScAddr const & element)
{
  ScType const type = m_context.GetElementType(element);
  bool const hasClassSubtype = (type.BitAnd(*ScType::NodeClass) == *ScType::NodeClass) && !type.IsLink();
  return hasClassSubtype || m_context.CheckConnector(Keynodes::sc_node_class, element, ScType::ConstPermPosArc);
}
