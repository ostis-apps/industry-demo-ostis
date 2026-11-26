#include "AgentForDefinitionAnswer.hpp"

#include <sc-memory/sc_memory.hpp>

#include "keynodes/Keynodes.hpp"

AgentForDefinitionAnswer::AgentForDefinitionAnswer()
{
  m_logger = utils::ScLogger(
      utils::ScLogger::ScLogType::File, "logs/AgentForDefinitionAnswer.log", utils::ScLogLevel::Debug);
}

ScAddr AgentForDefinitionAnswer::GetActionClass() const
{
  return Keynodes::action_get_definition;
}

ScResult AgentForDefinitionAnswer::DoProgram(ScAction & action)
{
  // ВХОД: rrel_1 — понятие, о котором задан вопрос
  auto const & [conceptAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(conceptAddr))
  {
    m_logger.Error("Concept argument of question `what is X` is not specified.");
    return action.FinishWithError();
  }

  m_logger.Debug("Question `what is ", m_context.GetElementSystemIdentifier(conceptAddr), "`");

  ScStructure result = m_context.GenerateStructure();
  result << conceptAddr;

  ScAddrUnorderedSet processed;  // адреса связок, уже включённых в ответ (защита от повторов)
  size_t found = 0;

  // 1. Определение*: понятие => определение*:: sc-ссылка с текстом определения
  size_t const definitions = CollectRelationBundles(conceptAddr, Keynodes::nrel_definition, result, processed);
  found += definitions;

  // 2. Пояснение* и примечание*: понятие => пояснение*:: sc-ссылка с текстом
  size_t const explanations = CollectRelationBundles(conceptAddr, Keynodes::nrel_explanation, result, processed)
                              + CollectRelationBundles(conceptAddr, Keynodes::nrel_note, result, processed);
  found += explanations;

  // 3. Включение*: родительский класс => включение*:: понятие
  found += CollectParentClassBundles(conceptAddr, result, processed);

  // 4. Ключевые связи понятия — ссылки на существующие связки его основных отношений
  found += CollectKeyRelationBundles(conceptAddr, result, processed);

  // 5. Если текстов нет в виде связок определения*/пояснения*, ищем их в форме sc-текстов,
  //    принятой в БЗ ISA-88 и Савушкин (текст <- definition;; <= nrel_sc_text_translation:: ...)
  if (definitions == 0)
    found += CollectScTexts(conceptAddr, Keynodes::definition, result);
  if (explanations == 0)
  {
    found += CollectScTexts(conceptAddr, Keynodes::explanation, result);
    found += CollectScTexts(conceptAddr, Keynodes::note, result);
  }

  if (found == 0)
  {
    m_logger.Warning("Insufficient data: no knowledge about the concept found in the knowledge base.");
    return action.FinishUnsuccessfully();
  }

  action.SetResult(result);
  m_logger.Info("Definition answer formed. Constructions found: ", found);
  return action.FinishSuccessfully();
}

void AgentForDefinitionAnswer::AddBundle(
    ScAddr const & first,
    ScAddr const & bundle,
    ScAddr const & second,
    ScAddr const & relation,
    ScAddr const & relationArc,
    ScStructure & result)
{
  // в ответ попадают сами элементы существующей связки, а не их копии
  result << first << bundle << second << relation << relationArc;
}

size_t AgentForDefinitionAnswer::CollectRelationBundles(
    ScAddr const & conceptAddr,
    ScAddr const & relationAddr,
    ScStructure & result,
    ScAddrUnorderedSet & processed)
{
  size_t count = 0;
  ScIterator5Ptr const it5 = m_context.CreateIterator5(
      conceptAddr, ScType::ConstCommonArc, ScType::Unknown, ScType::ConstPermPosArc, relationAddr);
  while (it5->Next())
  {
    ScAddr const & bundle = it5->Get(1);
    if (!processed.insert(bundle).second)
      continue;

    AddBundle(conceptAddr, bundle, it5->Get(2), relationAddr, it5->Get(3), result);
    ++count;
  }
  return count;
}

size_t AgentForDefinitionAnswer::CollectParentClassBundles(
    ScAddr const & conceptAddr,
    ScStructure & result,
    ScAddrUnorderedSet & processed)
{
  size_t count = 0;
  // ищем связки включения*, ВТОРЫМ компонентом которых является понятие
  ScIterator5Ptr const it5 = m_context.CreateIterator5(
      ScType::Unknown, ScType::ConstCommonArc, conceptAddr, ScType::ConstPermPosArc, ScKeynodes::nrel_inclusion);
  while (it5->Next())
  {
    ScAddr const & parentAddr = it5->Get(0);
    if (parentAddr == conceptAddr || !IsClass(parentAddr))
      continue;

    ScAddr const & bundle = it5->Get(1);
    if (!processed.insert(bundle).second)
      continue;

    AddBundle(parentAddr, bundle, conceptAddr, ScKeynodes::nrel_inclusion, it5->Get(3), result);
    ++count;
  }
  return count;
}

size_t AgentForDefinitionAnswer::CollectKeyRelationBundles(
    ScAddr const & conceptAddr,
    ScStructure & result,
    ScAddrUnorderedSet & processed)
{
  size_t count = 0;
  ScIterator3Ptr const it3 = m_context.CreateIterator3(conceptAddr, ScType::ConstCommonArc, ScType::Unknown);
  while (it3->Next())
  {
    ScAddr const & bundle = it3->Get(1);
    if (processed.count(bundle) != 0)
      continue;

    // отношение — это узел, помечающий дугу связки
    ScIterator3Ptr const markers = m_context.CreateIterator3(ScType::Unknown, ScType::ConstPermPosArc, bundle);
    while (markers->Next())
    {
      ScAddr const & relationAddr = markers->Get(0);
      if (!IsNonRoleRelation(relationAddr))
        continue;

      AddBundle(conceptAddr, bundle, it3->Get(2), relationAddr, markers->Get(1), result);
      processed.insert(bundle);
      ++count;
    }
  }
  return count;
}

size_t AgentForDefinitionAnswer::CollectScTexts(
    ScAddr const & conceptAddr,
    ScAddr const & textClassAddr,
    ScStructure & result)
{
  size_t count = 0;
  // в БЗ ISA-88: sc-текст -> rrel_key_sc_element:: понятие
  ScIterator5Ptr const texts = m_context.CreateIterator5(
      ScType::Unknown, ScType::ConstPermPosArc, conceptAddr, ScType::ConstPermPosArc, ScKeynodes::rrel_key_sc_element);
  while (texts->Next())
  {
    ScAddr const & textAddr = texts->Get(0);
    if (!m_context.CheckConnector(textClassAddr, textAddr, ScType::ConstPermPosArc))
      continue;

    result << textAddr << texts->Get(1) << conceptAddr << texts->Get(3) << ScKeynodes::rrel_key_sc_element;
    ++count;

    // переводы sc-текста: узел перевода => nrel_sc_text_translation:: sc-текст
    ScIterator5Ptr const translations = m_context.CreateIterator5(
        ScType::Unknown, ScType::ConstCommonArc, textAddr, ScType::ConstPermPosArc, Keynodes::nrel_sc_text_translation);
    while (translations->Next())
    {
      ScAddr const & translationAddr = translations->Get(0);
      result << translationAddr << translations->Get(1) << textAddr << translations->Get(3)
             << Keynodes::nrel_sc_text_translation;

      // примеры (sc-ссылки с содержимым текста): перевод -> rrel_example:: ссылка
      ScIterator5Ptr const examples = m_context.CreateIterator5(
          translationAddr, ScType::ConstPermPosArc, ScType::ConstNodeLink, ScType::ConstPermPosArc, Keynodes::rrel_example);
      while (examples->Next())
        result << examples->Get(0) << examples->Get(1) << examples->Get(2) << examples->Get(3) << Keynodes::rrel_example;
    }
  }
  return count;
}

bool AgentForDefinitionAnswer::IsClass(ScAddr const & element)
{
  ScType const type = m_context.GetElementType(element);
  bool const hasClassSubtype = (type.BitAnd(*ScType::NodeClass) == *ScType::NodeClass) && !type.IsLink();
  return hasClassSubtype || m_context.CheckConnector(Keynodes::sc_node_class, element, ScType::ConstPermPosArc);
}

bool AgentForDefinitionAnswer::IsNonRoleRelation(ScAddr const & element)
{
  ScType const type = m_context.GetElementType(element);
  return (type.BitAnd(*ScType::NodeNonRole) == *ScType::NodeNonRole)
         || m_context.CheckConnector(Keynodes::sc_node_non_role_relation, element, ScType::ConstPermPosArc);
}
