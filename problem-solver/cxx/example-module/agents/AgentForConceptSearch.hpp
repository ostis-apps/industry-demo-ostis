/*
 * Агент поиска понятий по текстовому запросу («что соответствует запросу»).
 *
 * ВХОД (в памяти):
 *   rrel_1 — sc-ссылка, содержимым которой является строка запроса;
 *   rrel_2 — язык запроса (lang_ru / lang_en), необязательно.
 *
 * АЛГОРИТМ: обход связок отношений «основной идентификатор*», «идентификатор*»,
 * «определение*»; регистронезависимый поиск подстроки запроса в содержимом
 * их sc-ссылок.
 *
 * РЕЗУЛЬТАТ — структура:
 *   1) множество найденных понятий -> sc-дуга принадлежности -> понятие;
 *   2) понятие -> основание совпадения':: sc-ссылка-основание
 *      (основной идентификатор / неосновной идентификатор / определение).
 *
 * Если ничего не найдено — результат содержит пустое множество,
 * действие завершается УСПЕШНО (это не ошибка).
 */

#pragma once

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_structure.hpp>

class AgentForConceptSearch : public ScActionInitiatedAgent
{
public:
  AgentForConceptSearch();

  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  // основание совпадения; порядок задаёт приоритет при нескольких совпадениях
  enum class MatchReason
  {
    MainIdentifier,     // основной идентификатор
    NonMainIdentifier,  // неосновной идентификатор
    Definition          // определение
  };

  struct Match
  {
    ScAddr conceptAddr;      // найденное понятие
    ScAddr textAddr;         // sc-ссылка, в содержимом которой найдено совпадение
    ScAddr bundleAddr;       // существующая связка идентификатора/определения
    ScAddr relationAddr;     // отношение этой связки
    ScAddr relationArcAddr;  // дуга от отношения к связке
    MatchReason reason;      // основание совпадения
  };

  static constexpr size_t MAX_FOUND_CONCEPTS = 200;

  void CollectMatchesByRelation(
      ScAddr const & relationAddr,
      MatchReason reason,
      std::string const & normalizedQuery,
      ScAddr const & languageAddr,
      ScAddrToValueUnorderedMap<Match> & matches);

  void AddMatchToResult(Match const & match, ScAddr const & foundSetAddr, ScStructure & result);

  bool IsConcept(ScAddr const & element);
  bool MatchesLanguage(ScAddr const & textAddr, ScAddr const & languageAddr);

  // приведение к нижнему регистру (ASCII + кириллица) и обрезка пробелов
  static std::string NormalizeText(std::string const & text);
    // извлечение строки запроса из аргумента:
  // либо это sc-ссылка с содержимым, либо узел с основным идентификатором
  bool TryGetQueryText(ScAddr const & argumentAddr, ScAddr const & languageAddr, std::string & queryText);
};
