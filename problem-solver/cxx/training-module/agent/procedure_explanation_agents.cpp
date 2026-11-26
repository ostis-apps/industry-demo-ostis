#include "procedure_explanation_agent.hpp"

#include <map>
#include <optional>

#include <sc-memory/sc_memory.hpp>

#include "keynodes/training_keynodes.hpp"
#include "utils/training_utils.hpp"

using namespace training_utils;

namespace
{
std::optional<ScAddrVector> NumberedSteps(ScAgentContext & context, ScAddr const & tuple)
{
  std::map<size_t, ScAddr> steps;
  auto it = context.CreateIterator5(tuple, ScType::PermPosArc, ScType::Node, ScType::PermPosArc, ScType::NodeRole);
  while (it->Next())
  {
    auto const role = it->Get(4);
    auto name = context.GetElementSystemIdentifier(role);
    if (name.compare(0, 5, "rrel_") == 0)
      name.erase(0, 5);
    else
    {
      name = DisplayName(context, role);
      if (name.empty() || name.back() != '\'')
        continue;
      name.pop_back();
    }
    size_t index = 0;
    auto end = name.data() + name.size();
    auto parsed = std::from_chars(name.data(), end, index);
    if (parsed.ec == std::errc{} && parsed.ptr == end && index > 0)
    {
      auto [position, inserted] = steps.emplace(index, it->Get(2));
      if (!inserted && position->second != it->Get(2))
        return std::nullopt;
    }
  }

  ScAddrVector ordered;
  for (auto const & [index, step] : steps)
  {
    if (index != ordered.size() + 1)
      return std::nullopt;
    ordered.push_back(step);
  }
  return ordered;
}

// При отсутствии исходной связки вывод относится к данному описанию. Временная
// дуга позволяет сохранить исходный узел шага, не превратив вывод в правило модели.
void IncludeStepValue(
    ScAgentContext & context,
    ScStructure & result,
    ScAddr const & step,
    ScAddr const & value,
    ScAddr const & role)
{
  auto existing = context.CreateIterator5(step, ScType::PosArc, value, ScType::PermPosArc, role);
  if (existing->Next())
  {
    result << step << existing->Get(1) << value << existing->Get(3) << role;
    return;
  }
  auto arc = context.GenerateConnector(ScType::ConstTempPosArc, step, value);
  auto attribute = context.GenerateConnector(ScType::ConstPermPosArc, role, arc);
  result << step << arc << value << attribute << role;
}

ScAddrVector RelatedNodes(ScAgentContext & context, ScAddr const & source, ScAddr const & relation)
{
  ScAddrVector nodes;
  auto it = context.CreateIterator5(source, ScType::CommonArc, ScType::Node, ScType::PermPosArc, relation);
  while (it->Next())
    AppendUnique(nodes, it->Get(2));
  return nodes;
}

// Прежняя версия записывала варианты как постоянные факты об использовании.
// Такие дуги можно опознать по их включению в ответ на вопрос о процедуре.
bool InLegacyProcedureAnswer(ScAgentContext & context, ScAddr const & arc)
{
  auto structures = context.CreateIterator3(ScType::ConstNodeStructure, ScType::ConstPermPosArc, arc);
  while (structures->Next())
  {
    // В текущем формате ключевым элементом ответа отмечено описание.
    if (SearchByRole(context, structures->Get(0), ScKeynodes::rrel_key_sc_element).IsValid())
      continue;
    auto actions = context.CreateIterator5(
        ScType::ConstNode,
        ScType::ConstCommonArc,
        structures->Get(0),
        ScType::ConstPermPosArc,
        ScKeynodes::nrel_result);
    while (actions->Next())
      if (BelongsToClass(context, actions->Get(0), TrainingKeynodes::action_explain_procedure))
        return true;
  }
  return false;
}

bool IsInstanceOfEither(ScAgentContext & context, ScAddr const & object, ScAddr const & first, ScAddr const & second)
{
  ScAddrVector classes;
  auto types = context.CreateIterator3(ScType::ConstNodeClass, ScType::PermPosArc, object);
  while (types->Next())
    AppendUnique(classes, types->Get(0));

  for (size_t i = 0; i < classes.size(); ++i)
  {
    if (classes[i] == first || classes[i] == second)
      return true;
    auto parents = context.CreateIterator5(
        ScType::ConstNodeClass,
        ScType::ConstCommonArc,
        classes[i],
        ScType::ConstPermPosArc,
        TrainingKeynodes::nrel_inclusion);
    while (parents->Next())
      AppendUnique(classes, parents->Get(0));
  }
  return false;
}

struct Equipment
{
  ScAddrVector used;
  ScAddrVector options;
};

bool IsSelection(ScAgentContext & context, ScAddr const & step)
{
  return context.CheckConnector(TrainingKeynodes::selection_of_equipment, step, ScType::PermPosArc);
}

void ReadSelection(ScAgentContext & context, ScAddr const & selection, Equipment & equipment)
{
  for (auto const & option : RelatedNodes(context, selection, TrainingKeynodes::nrel_object))
    if (context.GetElementType(option).IsConst())
      AppendUnique(equipment.options, option);

  // Конкретный результат выбора подтверждает назначение оборудования.
  // Переменный узел результата в шаблоне рецепта такого назначения не задаёт.
  for (auto const & selected : RelatedNodes(context, selection, TrainingKeynodes::nrel_result))
    if (context.GetElementType(selected).IsConst())
      AppendUnique(equipment.used, selected);
}

Equipment StepEquipment(ScAgentContext & context, ScAddr const & step)
{
  Equipment equipment;
  if (IsSelection(context, step))
    ReadSelection(context, step, equipment);
  else
  {
    for (auto const & object : RelatedNodes(context, step, TrainingKeynodes::nrel_object))
    {
      auto producers = context.CreateIterator5(
          ScType::Node, ScType::CommonArc, object, ScType::PermPosArc, TrainingKeynodes::nrel_result);
      bool selected = false;
      while (producers->Next())
      {
        auto const producer = producers->Get(0);
        if (IsSelection(context, producer))
        {
          if (context.GetElementType(object).IsVar())
            ReadSelection(context, producer, equipment);
          selected = true;
        }
      }
      if (context.GetElementType(object).IsConst()
          && (selected
              || IsInstanceOfEither(
                  context,
                  object,
                  context.SearchElementBySystemIdentifier("equipment"),
                  context.SearchElementBySystemIdentifier("equipment_entity"))))
        AppendUnique(equipment.used, object);
    }
  }

  auto direct = context.CreateIterator5(
      step, ScType::PermPosArc, ScType::ConstNode, ScType::PermPosArc, TrainingKeynodes::rrel_used_equipment);
  while (direct->Next())
  {
    auto item = direct->Get(2);
    if (item != TrainingKeynodes::training_undetermined
        && !(ContainsAddr(equipment.options, item) && InLegacyProcedureAnswer(context, direct->Get(1))))
      AppendUnique(equipment.used, item);
  }
  return equipment;
}

void SetProcedureAnswer(ScAgentContext & context, ScAction & action, ScStructure const & result)
{
  action.SetResult(result);
  auto arc = context.GenerateConnector(ScType::ConstCommonArc, action, result);
  context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_result, arc);
  context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::nrel_answer, arc);
}
}  // namespace

ScAddr ProcedureExplanationAgent::GetActionClass() const
{
  return TrainingKeynodes::action_explain_procedure;
}

ScResult ProcedureExplanationAgent::DoProgram(ScAction & action)
{
  auto const & [procedureAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(procedureAddr))
  {
    m_logger.Error("ProcedureExplanationAgent: procedure argument is invalid.");
    return action.FinishWithError();
  }

  m_logger.Debug("ProcedureExplanationAgent: collecting steps for procedure.");

  StepOrder order = StepOrder::ExplicitList;
  auto const initial = CollectOrderedSteps(procedureAddr, order);
  ScStructure resultStructure = m_context.GenerateStructure();
  auto const steps = TraverseStepsFromInitial(initial, order, resultStructure);
  if (steps.empty())
  {
    resultStructure << TrainingKeynodes::training_undetermined;
    SetProcedureAnswer(m_context, action, resultStructure);
    return action.FinishUnsuccessfully();
  }

  // Узел описания выполнения процедуры
  ScAddr const descriptionAddr = m_context.GenerateNode(ScType::ConstNode);
  ScAddr const descClassArc = m_context.GenerateConnector(
      ScType::ConstPermPosArc, TrainingKeynodes::concept_procedure_execution_description, descriptionAddr);
  resultStructure.Append(descriptionAddr, ScKeynodes::rrel_key_sc_element);
  resultStructure << TrainingKeynodes::concept_procedure_execution_description << descClassArc;

  // Строим 1-ю конструкцию (9-элементная для 2 шагов, продолжается по числу шагов 1', 2', ..., N')
  // и для каждого шага — 2-ю (состояние) и 3-ю (задействованное оборудование) 5-элементные конструкции.
  for (size_t i = 0; i < steps.size(); ++i)
  {
    ScAddr const & stepAddr = steps[i];
    ScAddr const numericRoleAddr = ResolveNumericRole(m_context, i + 1);

    // Конструкция 1: descriptionAddr -> (i+1)': stepAddr
    GenerateRoleConstruction(m_context, resultStructure, descriptionAddr, stepAddr, numericRoleAddr);

    DescribeStep(stepAddr, resultStructure);
  }

  SetProcedureAnswer(m_context, action, resultStructure);
  return action.FinishSuccessfully();
}

ScAddrVector ProcedureExplanationAgent::CollectOrderedSteps(ScAddr const & procedureAddr, StepOrder & order)
{
  auto direct = NumberedSteps(m_context, procedureAddr);
  if (!direct || !direct->empty())
    return direct.value_or(ScAddrVector{});

  // В GWF декомпозиция класса рецепта может быть задана на переменном экземпляре.
  ScAddrVector candidates{procedureAddr};
  auto instances = m_context.CreateIterator3(procedureAddr, ScType::PermPosArc, ScType::Node);
  while (instances->Next())
    AppendUnique(candidates, instances->Get(2));
  auto contours = m_context.CreateIterator5(
      ScType::Node, ScType::PermPosArc, procedureAddr, ScType::PermPosArc, TrainingKeynodes::rrel_key_sc_element);
  while (contours->Next())
    AppendUnique(candidates, contours->Get(0));

  ScAddrVector tuples;
  for (auto const & candidate : candidates)
  {
    auto it = m_context.CreateIterator5(
        ScType::Node, ScType::CommonArc, candidate, ScType::PermPosArc, TrainingKeynodes::nrel_decomposition_of_action);
    while (it->Next())
      AppendUnique(tuples, it->Get(0));
  }
  if (tuples.size() == 1)
  {
    auto sequence = NumberedSteps(m_context, tuples.front());
    if (!sequence || !sequence->empty())
    {
      order = StepOrder::NumberedEntries;
      return sequence.value_or(ScAddrVector{});
    }
    auto members = m_context.CreateIterator3(tuples.front(), ScType::PermPosArc, ScType::Node);
    order = StepOrder::Transitions;
    ScAddrVector steps;
    while (members->Next())
      AppendUnique(steps, members->Get(2));
    return steps;
  }
  if (tuples.empty()
      && IsInstanceOfEither(m_context, procedureAddr, TrainingKeynodes::phase, TrainingKeynodes::operation))
    return {procedureAddr};
  return {};
}

ScAddrVector ProcedureExplanationAgent::TraverseStepsFromInitial(
    ScAddrVector const & initialSteps,
    StepOrder order,
    ScStructure & result)
{
  ScAddrVector reachable;
  for (auto const & step : initialSteps)
    AppendUnique(reachable, step);
  bool const repeatedSteps = reachable.size() < initialSteps.size();
  ScAddrToValueUnorderedMap<ScAddrVector> predecessors;
  if (order != StepOrder::Transitions && !repeatedSteps)
    for (size_t i = 1; i < initialSteps.size(); ++i)
      AppendUnique(predecessors[initialSteps[i]], initialSteps[i - 1]);
  size_t const seedCount = reachable.size();

  for (size_t i = 0; i < reachable.size(); ++i)
  {
    ScAddr current = reachable[i];
    for (ScAddr const & rel :
         {TrainingKeynodes::nrel_goto,
          TrainingKeynodes::nrel_then,
          TrainingKeynodes::nrel_else,
          TrainingKeynodes::nrel_execution_order,
          TrainingKeynodes::nrel_order_of_execution})
    {
      auto next = m_context.CreateIterator5(current, ScType::CommonArc, ScType::Node, ScType::PermPosArc, rel);
      while (next->Next())
      {
        // У отдельной фазы и явного списка есть собственные границы:
        // переход к следующей фазе родительской процедуры относится к другому вопросу.
        if (order == StepOrder::ExplicitList && !ContainsAddr(initialSteps, next->Get(2)))
          continue;
        // Сохраняем переход вместе с признаком ветви.
        result << current << next->Get(1) << next->Get(2) << next->Get(3) << rel;
        AppendUnique(reachable, next->Get(2));
        AppendUnique(predecessors[next->Get(2)], current);
      }
    }
  }

  if (repeatedSteps)
    return reachable.size() == seedCount ? initialSteps : ScAddrVector{};
  if (order == StepOrder::Transitions && reachable.size() > 1)
  {
    auto roots = std::count_if(
        reachable.begin(),
        reachable.end(),
        [&](ScAddr const & step)
        {
          return predecessors[step].empty();
        });
    if (roots != 1)
      return {};
  }

  ScAddrVector ordered;
  while (ordered.size() < reachable.size())
  {
    auto const count = ordered.size();
    for (auto const & step : reachable)
    {
      auto const & required = predecessors[step];
      if (!ContainsAddr(ordered, step)
          && std::all_of(
              required.begin(),
              required.end(),
              [&](ScAddr const & prev)
              {
                return ContainsAddr(ordered, prev);
              }))
        ordered.push_back(step);
    }
    if (ordered.size() == count)
      return {};  // Для цикла без условия выхода порядок выполнения не определён.
  }
  return ordered;
}

ScAddrVector ProcedureExplanationAgent::FindStepStates(ScAddr const & stepAddr)
{
  ScAddrVector states;
  auto roles = m_context.CreateIterator5(
      stepAddr, ScType::PermPosArc, ScType::ConstNode, ScType::PermPosArc, TrainingKeynodes::rrel_executed_in_state);
  while (roles->Next())
    if (roles->Get(2) != TrainingKeynodes::training_undetermined)
      AppendUnique(states, roles->Get(2));
  for (auto const & state : RelatedNodes(m_context, stepAddr, TrainingKeynodes::nrel_state))
    if (m_context.GetElementType(state).IsConst() && state != TrainingKeynodes::training_undetermined)
      AppendUnique(states, state);
  return states;
}

void ProcedureExplanationAgent::DescribeStep(ScAddr const & stepAddr, ScStructure & result)
{
  // Шаги GWF часто анонимны; класс раскрывает смысл шага («наполнение», «мойка» и т. д.).
  auto classes = m_context.CreateIterator3(ScType::ConstNodeClass, ScType::PermPosArc, stepAddr);
  while (classes->Next())
    result << classes->Get(0) << classes->Get(1) << stepAddr;

  auto states = FindStepStates(stepAddr);
  if (states.empty())
    states.push_back(TrainingKeynodes::training_undetermined);
  for (auto const & state : states)
    IncludeStepValue(m_context, result, stepAddr, state, TrainingKeynodes::rrel_executed_in_state);

  auto equipment = StepEquipment(m_context, stepAddr);
  if (equipment.used.empty())
    equipment.used.push_back(TrainingKeynodes::training_undetermined);
  for (auto const & item : equipment.used)
    IncludeStepValue(m_context, result, stepAddr, item, TrainingKeynodes::rrel_used_equipment);
  for (auto const & option : equipment.options)
    IncludeStepValue(m_context, result, stepAddr, option, TrainingKeynodes::rrel_equipment_option);
}
