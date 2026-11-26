#include "AgentForConceptSearch.hpp"

#include <cctype>

#include <sc-memory/sc_memory.hpp>

#include "keynodes/Keynodes.hpp"

AgentForConceptSearch::AgentForConceptSearch()
{
  m_logger = utils::ScLogger(
      utils::ScLogger::ScLogType::File, "logs/AgentForConceptSearch.log", utils::ScLogLevel::Info);
}

ScAddr AgentForConceptSearch::GetActionClass() const
{
  return Keynodes::action_search_concepts;
}

ScResult AgentForConceptSearch::DoProgram(ScAction & action)
{
  // ВХОД: rrel_1 — sc-ссылка со строкой запроса, rrel_2 — язык (необязательно)
  auto const & [queryLinkAddr, requestedLanguageAddr] = action.GetArguments<2>();

  if (!m_context.IsElement(queryLinkAddr))
  {
    m_logger.Error("Query argument (rrel_1) is not specified.");
    return action.FinishWithError();
  }

    std::string queryText;
    ScAddr languageAddr = requestedLanguageAddr;
  if (!TryGetQueryText(queryLinkAddr, languageAddr, queryText))
  {
    m_logger.Error("Query argument must be an sc-link with content or a node with a main identifier.");
    return action.FinishWithError();
  }

  std::string const normalizedQuery = NormalizeText(queryText);
  if (normalizedQuery.empty())
  {
    m_logger.Error("Query string is empty.");
    return action.FinishWithError();
  }

  // язык запроса: если не задан или неизвестен — ищем по всем языкам
  
  if (m_context.IsElement(languageAddr) && languageAddr != ScKeynodes::lang_ru
      && languageAddr != Keynodes::lang_en)
  {
    m_logger.Warning("Unknown language argument. Searching in all languages.");
    languageAddr = ScAddr::Empty;
  }

  m_logger.Debug("Query: `", normalizedQuery, "`");

  try
  {
    ScAddrToValueUnorderedMap<Match> matches;

    // порядок обхода задаёт приоритет основания совпадения
    CollectMatchesByRelation(
        ScKeynodes::nrel_main_idtf, MatchReason::MainIdentifier, normalizedQuery, languageAddr, matches);
    CollectMatchesByRelation(
        ScKeynodes::nrel_idtf, MatchReason::NonMainIdentifier, normalizedQuery, languageAddr, matches);
    CollectMatchesByRelation(
        Keynodes::nrel_definition, MatchReason::Definition, normalizedQuery, languageAddr, matches);

    // множество найденных понятий создаётся ВСЕГДА, даже если оно пусто
    ScAddr const foundSetAddr = m_context.GenerateNode(ScType::ConstNode);

    ScStructure result = m_context.GenerateStructure();
    result << foundSetAddr;

    for (auto const & item : matches)
      AddMatchToResult(item.second, foundSetAddr, result);

    action.SetResult(result);
    m_logger.Info("Concepts found: ", matches.size());

    // пустое множество — это корректный ответ, а не ошибка
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

void AgentForConceptSearch::CollectMatchesByRelation(
    ScAddr const & relationAddr,
    MatchReason reason,
    std::string const & normalizedQuery,
    ScAddr const & languageAddr,
    ScAddrToValueUnorderedMap<Match> & matches)
{
  // Все выходящие дуги из отношения — это дуги-маркеры его связок.
  // Так мы обходим ровно связки идентификаторов и определений, а не всю БЗ.
  ScIterator3Ptr const markers = m_context.CreateIterator3(relationAddr, ScType::ConstPermPosArc, ScType::Unknown);
  while (markers->Next())
  {
    if (matches.size() >= MAX_FOUND_CONCEPTS)
    {
      m_logger.Warning("Result limit reached: ", MAX_FOUND_CONCEPTS, " concepts.");
      return;
    }

    ScAddr const bundleAddr = markers->Get(2);
    if (!m_context.GetElementType(bundleAddr).IsCommonArc())
      continue;

    // связка: понятие => отношение:: sc-ссылка с текстом
    auto const [conceptAddr, textAddr] = m_context.GetConnectorIncidentElements(bundleAddr);
    if (!m_context.GetElementType(textAddr).IsLink())
      continue;

    // если понятие уже найдено по более сильному основанию — пропускаем
    auto const found = matches.find(conceptAddr);
    if (found != matches.end() && !(reason < found->second.reason))
      continue;

    std::string content;
    if (!m_context.GetLinkContent(textAddr, content))
      continue;
    if (NormalizeText(content).find(normalizedQuery) == std::string::npos)
      continue;
    if (!MatchesLanguage(textAddr, languageAddr))
      continue;
    if (!IsConcept(conceptAddr))
      continue;

    matches[conceptAddr] = Match{conceptAddr, textAddr, bundleAddr, relationAddr, markers->Get(1), reason};
  }
}

void AgentForConceptSearch::AddMatchToResult(
    Match const & match,
    ScAddr const & foundSetAddr,
    ScStructure & result)
{
  // ПЕРВАЯ (трёхэлементная) конструкция:
  // множество найденных понятий -> sc-дуга принадлежности -> понятие
  ScAddr const membershipArcAddr =
      m_context.GenerateConnector(ScType::ConstPermPosArc, foundSetAddr, match.conceptAddr);
  result << foundSetAddr << membershipArcAddr << match.conceptAddr;

  // ВТОРАЯ (пятиэлементная) конструкция:
  // понятие -> основание совпадения':: sc-ссылка-основание
  // (основной идентификатор / неосновной идентификатор / определение —
  //  основание видно по тому, какая ссылка стоит в третьей позиции)
  ScAddr const reasonArcAddr =
      m_context.GenerateConnector(ScType::ConstPermPosArc, match.conceptAddr, match.textAddr);
  ScAddr const reasonRoleArcAddr =
      m_context.GenerateConnector(ScType::ConstPermPosArc, Keynodes::rrel_match_reason, reasonArcAddr);
  result << match.conceptAddr << reasonArcAddr << match.textAddr << Keynodes::rrel_match_reason
         << reasonRoleArcAddr;

  // дополнительно включаем существующую связку, по которой найдено совпадение,
  // чтобы в интерфейсе было видно, идентификатор это или определение
  result << match.bundleAddr << match.relationAddr << match.relationArcAddr;
}

bool AgentForConceptSearch::IsConcept(ScAddr const & element)
{
  if (!m_context.IsElement(element))
    return false;

  ScType const type = m_context.GetElementType(element);

  // понятие — sc-узел (в том числе отношение: относительное понятие),
  // но не sc-ссылка и не раздел БЗ
  if (!type.IsNode() || type.IsLink())
    return false;
  if (type.BitAnd(*ScType::NodeStructure) == *ScType::NodeStructure)
    return false;

  return true;
}

bool AgentForConceptSearch::MatchesLanguage(ScAddr const & textAddr, ScAddr const & languageAddr)
{
  if (!m_context.IsElement(languageAddr))
    return true;
  return m_context.CheckConnector(languageAddr, textAddr, ScType::ConstPermPosArc);
}

std::string AgentForConceptSearch::NormalizeText(std::string const & text)
{
  std::string lowered;
  lowered.reserve(text.size());

  for (size_t i = 0; i < text.size(); ++i)
  {
    auto const c = static_cast<unsigned char>(text[i]);

    // ASCII: A-Z -> a-z
    if (c >= 'A' && c <= 'Z')
    {
      lowered.push_back(static_cast<char>(c + 32));
      continue;
    }

    // кириллица в UTF-8 (двухбайтовые последовательности)
    if (c == 0xD0 && i + 1 < text.size())
    {
      auto const d = static_cast<unsigned char>(text[i + 1]);
      if (d >= 0x90 && d <= 0x9F)  // А..П -> а..п
      {
        lowered.push_back(static_cast<char>(0xD0));
        lowered.push_back(static_cast<char>(d + 0x20));
        ++i;
        continue;
      }
      if (d >= 0xA0 && d <= 0xAF)  // Р..Я -> р..я
      {
        lowered.push_back(static_cast<char>(0xD1));
        lowered.push_back(static_cast<char>(d - 0x20));
        ++i;
        continue;
      }
      if (d == 0x81)  // Ё -> ё
      {
        lowered.push_back(static_cast<char>(0xD1));
        lowered.push_back(static_cast<char>(0x91));
        ++i;
        continue;
      }
    }

    lowered.push_back(static_cast<char>(c));
  }

  size_t begin = 0;
  size_t end = lowered.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(lowered[begin])))
    ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(lowered[end - 1])))
    --end;

  return lowered.substr(begin, end - begin);
}

bool AgentForConceptSearch::TryGetQueryText(
    ScAddr const & argumentAddr,
    ScAddr const & languageAddr,
    std::string & queryText)
{
  // 1) аргумент — sc-ссылка: берём её содержимое
  if (m_context.GetElementType(argumentAddr).IsLink() && m_context.GetLinkContent(argumentAddr, queryText))
    return true;

  // 2) аргумент — узел: берём содержимое ссылки его основного идентификатора
  ScIterator5Ptr const it5 = m_context.CreateIterator5(
      argumentAddr,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      ScKeynodes::nrel_main_idtf);
  while (it5->Next())
  {
    ScAddr const & idtfLinkAddr = it5->Get(2);
    if (!MatchesLanguage(idtfLinkAddr, languageAddr))
      continue;
    if (m_context.GetLinkContent(idtfLinkAddr, queryText))
      return true;
  }

  return false;
}
