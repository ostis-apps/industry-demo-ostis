/*
 * Агент ответа на вопрос «что такое X».
 * Результат — структура, ссылающаяся на УЖЕ существующие связки БЗ:
 *   1) X => nrel_definition:: [текст определения];
 *   2) X => nrel_explanation:: [текст пояснения];
 *   3) родительский класс => nrel_inclusion:: X;
 *   4) ключевые связи понятия (связки его основных отношений).
 * Новые знания о понятии не генерируются.
 */

#pragma once

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_structure.hpp>

class AgentForDefinitionAnswer : public ScActionInitiatedAgent
{
public:
  AgentForDefinitionAnswer();

  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  // Включает в ответ все 5 sc-элементов существующей связки отношения
  void AddBundle(
      ScAddr const & first,
      ScAddr const & bundle,
      ScAddr const & second,
      ScAddr const & relation,
      ScAddr const & relationArc,
      ScStructure & result);

  // Связки вида: concept => relation:: ?
  size_t CollectRelationBundles(
      ScAddr const & conceptAddr,
      ScAddr const & relationAddr,
      ScStructure & result,
      ScAddrUnorderedSet & processed);

  // Связки вида: родительский класс => nrel_inclusion:: concept
  size_t CollectParentClassBundles(ScAddr const & conceptAddr, ScStructure & result, ScAddrUnorderedSet & processed);

  // Остальные ключевые связи понятия (все его неролевые отношения)
  size_t CollectKeyRelationBundles(ScAddr const & conceptAddr, ScStructure & result, ScAddrUnorderedSet & processed);

  // Тексты в форме sc-текстов БЗ ISA-88: текст <- definition/explanation/note
  size_t CollectScTexts(ScAddr const & conceptAddr, ScAddr const & textClassAddr, ScStructure & result);

  bool IsClass(ScAddr const & element);
  bool IsNonRoleRelation(ScAddr const & element);
};
