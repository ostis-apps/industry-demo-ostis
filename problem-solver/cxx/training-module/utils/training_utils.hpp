#pragma once

#include <algorithm>
#include <charconv>
#include <string>

#include "keynodes/training_keynodes.hpp"

#include <sc-memory/sc_memory.hpp>

namespace training_utils
{

inline bool ContainsAddr(ScAddrVector const & vec, ScAddr const & target)
{
  return std::find(vec.begin(), vec.end(), target) != vec.end();
}

inline void AppendUnique(ScAddrVector & vec, ScAddr const & addr)
{
  if (addr.IsValid() && !ContainsAddr(vec, addr))
    vec.push_back(addr);
}

// Получает или создаёт числовой ролевой атрибут rrel_1, rrel_2, ... (в sc-web отображается как 1', 2', ...)
inline ScAddr ResolveNumericRole(ScAgentContext & context, size_t index)
{
  return context.ResolveElementSystemIdentifier("rrel_" + std::to_string(index), ScType::ConstNodeRole);
}

// Строит точную 5-элементную SC-конструкцию:
// (1: source) --(2: ConstPermPosArc)--> (3: target), и (4: role) --(5: ConstPermPosArc)--> (2)
// Все 5 элементов добавляются в результирующую структуру.
inline ScAddr GenerateRoleConstruction(
    ScAgentContext & context,
    ScStructure & structure,
    ScAddr const & sourceAddr,
    ScAddr const & targetAddr,
    ScAddr const & roleAddr)
{
  ScAddr const arcAddr = context.GenerateConnector(ScType::ConstPermPosArc, sourceAddr, targetAddr);
  ScAddr const roleArcAddr = context.GenerateConnector(ScType::ConstPermPosArc, roleAddr, arcAddr);
  structure << sourceAddr << arcAddr << targetAddr << roleAddr << roleArcAddr;
  return arcAddr;
}

// Ищет первый элемент target в конструкции: source -> role: target
inline ScAddr SearchByRole(ScAgentContext & context, ScAddr const & sourceAddr, ScAddr const & roleAddr)
{
  ScIterator5Ptr const it5 =
      context.CreateIterator5(sourceAddr, ScType::ConstPermPosArc, ScType::Unknown, ScType::ConstPermPosArc, roleAddr);
  if (it5->Next())
    return it5->Get(2);
  return ScAddr::Empty;
}

// Ищет все элементы target в конструкции: source -> role: target
inline ScAddrVector SearchAllByRole(ScAgentContext & context, ScAddr const & sourceAddr, ScAddr const & roleAddr)
{
  ScAddrVector result;
  ScIterator5Ptr const it5 =
      context.CreateIterator5(sourceAddr, ScType::ConstPermPosArc, ScType::Unknown, ScType::ConstPermPosArc, roleAddr);
  while (it5->Next())
    AppendUnique(result, it5->Get(2));
  return result;
}

// Проверяет принадлежность elementAddr классу classAddr
inline bool BelongsToClass(ScAgentContext & context, ScAddr const & elementAddr, ScAddr const & classAddr)
{
  ScIterator3Ptr const it3 = context.CreateIterator3(classAddr, ScType::ConstPermPosArc, elementAddr);
  return it3->Next();
}

// Читает положительное целое число из sc-ссылки
inline bool TryReadPositiveInt(ScAgentContext & context, ScAddr const & linkAddr, size_t & outValue)
{
  if (!linkAddr.IsValid() || !context.GetElementType(linkAddr).IsLink())
    return false;

  std::string content;
  if (!context.GetLinkContent(linkAddr, content) || content.empty())
    return false;

  auto const end = content.data() + content.size();
  auto const parsed = std::from_chars(content.data(), end, outValue);
  return parsed.ec == std::errc{} && parsed.ptr == end && outValue > 0;
}

// Оба событийных обработчика должны выбрать один и тот же вопрос.
inline ScAddr QuestionForAnswer(ScAgentContext & context, ScAddr const & answer)
{
  auto questions = SearchAllByRole(context, answer, TrainingKeynodes::rrel_question);
  if (questions.empty())
  {
    auto owners = context.CreateIterator5(
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        answer,
        ScType::ConstPermPosArc,
        TrainingKeynodes::rrel_employee_answer);
    while (owners->Next())
    {
      ScAddr const owner = owners->Get(0);
      if (SearchByRole(context, owner, TrainingKeynodes::rrel_reference_answer).IsValid())
        AppendUnique(questions, owner);
      else
        for (auto const & q : SearchAllByRole(context, owner, TrainingKeynodes::rrel_question))
          AppendUnique(questions, q);
    }
  }
  return questions.size() == 1 ? questions.front() : ScAddr::Empty;
}

inline std::string DisplayName(ScAgentContext & context, ScAddr const & element)
{
  auto names = context.CreateIterator5(
      element, ScType::ConstCommonArc, ScType::ConstNodeLink, ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf);
  std::string fallback;
  while (names->Next())
  {
    std::string name;
    if (context.GetLinkContent(names->Get(2), name))
    {
      if (BelongsToClass(context, names->Get(2), ScKeynodes::lang_ru))
        return name;
      fallback = name;
    }
  }
  return fallback.empty() ? context.GetElementSystemIdentifier(element) : fallback;
}

inline void SetQuestionText(
    ScAgentContext & context,
    ScStructure & result,
    ScAddr const & question,
    std::string const & text)
{
  auto link = context.GenerateLink();
  context.SetLinkContent(link, text);
  auto arc = context.GenerateConnector(ScType::ConstCommonArc, question, link);
  auto label = context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf, arc);
  auto lang = context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::lang_ru, link);
  result << question << arc << link << ScKeynodes::nrel_main_idtf << label << ScKeynodes::lang_ru << lang;
}

inline ScResult InsufficientData(ScAction & action)
{
  action.FormResult(TrainingKeynodes::training_undetermined);
  return action.FinishUnsuccessfully();
}

}  // namespace training_utils
