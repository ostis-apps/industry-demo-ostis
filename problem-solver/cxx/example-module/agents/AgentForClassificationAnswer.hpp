/*
 * Агент ответа на вопрос «какие бывают X».
 * Результат — структура, содержащая для каждого вида (подкласса):
 *   1) множество видов понятия X -> sc-дуга принадлежности -> подкласс;
 *   2) существующую связку X => nrel_inclusion:: подкласс.
 */

#pragma once

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_structure.hpp>

class AgentForClassificationAnswer : public ScActionInitiatedAgent
{
public:
  AgentForClassificationAnswer();

  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  // найденный вид понятия и связка включения*, которой он задан в БЗ
  struct Kind
  {
    ScAddr subclassAddr;
    ScAddr inclusionBundleAddr;
    ScAddr inclusionRelationArcAddr;
  };

  bool IsClass(ScAddr const & element);
};
