/*
 * Агент ответа на вопрос «как связаны X и Y».
 *
 * Алгоритм: поиск в ширину (BFS) кратчайшего пути в графе понятий,
 * рёбра которого — существующие связки бинарных неролевых отношений
 * (содержать*, включение*, ...). Обход идёт в обе стороны, т.к. вопрос
 * «как связаны» не задаёт направления связи.
 *
 * Результат — цепочка связей:
 *   1) путь -> rrel_i:: связка отношения (для каждой связки пути, i = 1, 2, ...);
 *   2) путь -> начало пути':: X;
 *   3) путь -> конец пути':: Y.
 *
 * Если пути в БЗ нет — действие завершается безуспешно с причиной
 * «недостаточность данных»: отсутствие записи не означает отсутствия связи.
 */

#pragma once

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_structure.hpp>

class AgentForRelationExplanation : public ScActionInitiatedAgent
{
public:
  AgentForRelationExplanation();

  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;
  bool CheckInitiationCondition(ScActionInitiatedEvent const & event) override;

private:
  // шаг пути — существующая в БЗ связка отношения между двумя понятиями
  struct Connection
  {
    ScAddr fromAddr;         // источник связки (как в БЗ)
    ScAddr toAddr;           // цель связки (как в БЗ)
    ScAddr bundleAddr;       // сама связка (sc-дуга/ребро общего вида)
    ScAddr relationAddr;     // неролевое отношение-маркер (может быть пустым)
    ScAddr relationArcAddr;  // дуга принадлежности от отношения к связке
  };

  // предельное число связок в пути; не больше числа доступных rrel_N (20)
  static constexpr size_t MAX_RELATION_PATH_LENGTH = 8;
  static constexpr size_t MAX_VISITED_CONCEPTS = 5000;

  std::vector<Connection> GetConnections(ScAddr const & conceptAddr);
  bool SearchPath(ScAddr const & startAddr, ScAddr const & finishAddr, std::vector<Connection> & path);
  void AddPathToResult(
      ScAddr const & startAddr,
      ScAddr const & finishAddr,
      std::vector<Connection> const & path,
      ScStructure & result);
  void ReportInsufficientData(ScAction & action, ScAddr const & startAddr, ScAddr const & finishAddr);

  bool IsConcept(ScAddr const & element);
  bool IsNonRoleRelation(ScAddr const & element);
  bool IsRoleRelation(ScAddr const & element);
};
