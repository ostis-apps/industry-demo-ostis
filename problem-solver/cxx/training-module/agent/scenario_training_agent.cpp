#include "scenario_training_agent.hpp"

#include <mutex>

#include <sc-memory/sc_memory.hpp>

#include "keynodes/training_keynodes.hpp"
#include "utils/training_utils.hpp"

using namespace training_utils;



ScAddr ScenarioTrainingAgent::GetActionClass() const
{
  return TrainingKeynodes::action_conduct_scenario_training;
}

ScResult ScenarioTrainingAgent::DoProgram(ScAction & action)
{
  auto const & [situationAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(situationAddr))
  {
    m_logger.Error("ScenarioTrainingAgent: learning situation argument is invalid.");
    return action.FinishWithError();
  }

  ScAddr const employeeAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_employee);
  ScAddr const emergencyAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_emergency_situation);

  if (!employeeAddr.IsValid() || !emergencyAddr.IsValid())
  {
    m_logger.Error("ScenarioTrainingAgent: employee or emergency situation is not specified.");
    return action.FinishWithError();
  }

  ScAddrVector const solutions = FindReferenceSolutions(emergencyAddr);
  if (solutions.empty())
  {
    m_logger.Debug("ScenarioTrainingAgent: no reference solutions found in KB for emergency situation.");
    return InsufficientData(action);
  }

  ScStructure resultStructure = m_context.GenerateStructure();

  // Создаём структуру-эталон ответа из эталонных решений БЗ
  ScStructure refAnswerStruct = m_context.GenerateStructure();
  ScAddr const refClassArc =
      m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_reference_answer, refAnswerStruct);
  resultStructure << TrainingKeynodes::concept_reference_answer << refClassArc << refAnswerStruct;

  for (ScAddr const & solAddr : solutions)
  {
    ScAddr const solArc = m_context.GenerateConnector(ScType::ConstPermPosArc, refAnswerStruct, solAddr);
    resultStructure << solArc << solAddr;
  }

  // Создаём тренировочный вопрос
  ScAddr const questionAddr = m_context.GenerateNode(ScType::ConstNode);
  ScAddr const qClassArc =
      m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_training_question, questionAddr);
  resultStructure << TrainingKeynodes::concept_training_question << qClassArc << questionAddr;

  std::string const name = DisplayName(m_context, emergencyAddr);
  SetQuestionText(
      m_context,
      resultStructure,
      questionAddr,
      name.empty() ? "Какие действия предусмотрены для указанной нештатной ситуации?"
                   : "Какие действия предусмотрены для ситуации «" + name + "»?");

  GenerateRoleConstruction(
      m_context, resultStructure, questionAddr, refAnswerStruct, TrainingKeynodes::rrel_reference_answer);
  GenerateRoleConstruction(m_context, resultStructure, questionAddr, emergencyAddr, TrainingKeynodes::rrel_basis);

  GenerateRoleConstruction(m_context, resultStructure, questionAddr, employeeAddr, TrainingKeynodes::rrel_employee);

  // Создаём узел сеанса тренировки и связываем его с сотрудником, ситуацией и вопросом
  ScAddr const sessionAddr = m_context.GenerateNode(ScType::ConstNode);
  ScAddr const sessClassArc = m_context.GenerateConnector(
      ScType::ConstPermPosArc, TrainingKeynodes::concept_scenario_training_session, sessionAddr);
  resultStructure << TrainingKeynodes::concept_scenario_training_session << sessClassArc << sessionAddr;

  // Publish the result before exposing the question: the review can arrive immediately.
  auto const resultArc = m_context.GenerateConnector(ScType::ConstCommonArc, action, resultStructure);
  m_context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_result, resultArc);
  GenerateRoleConstruction(m_context, resultStructure, action, sessionAddr, TrainingKeynodes::rrel_training_session);
  GenerateRoleConstruction(m_context, resultStructure, sessionAddr, employeeAddr, TrainingKeynodes::rrel_employee);
  GenerateRoleConstruction(
      m_context, resultStructure, sessionAddr, emergencyAddr, TrainingKeynodes::rrel_emergency_situation);
  GenerateRoleConstruction(m_context, resultStructure, sessionAddr, questionAddr, TrainingKeynodes::rrel_question);

  // Привязываем вопрос и сеанс к исходной ситуации обучения, чтобы интерфейс и агент проверки их видели
  GenerateRoleConstruction(m_context, resultStructure, situationAddr, questionAddr, TrainingKeynodes::rrel_question);
  GenerateRoleConstruction(
      m_context, resultStructure, situationAddr, sessionAddr, TrainingKeynodes::rrel_training_session);

  action.SetResult(resultStructure);
  return action.FinishSuccessfully();
}

ScAddrVector ScenarioTrainingAgent::FindReferenceSolutions(ScAddr const & emergencyAddr)
{
  ScAddrVector solutions;

  // 1. Неролевое отношение emergency => nrel_reference_solution: solution
  ScIterator5Ptr const nrelIt = m_context.CreateIterator5(
      emergencyAddr,
      ScType::ConstCommonArc,
      ScType::Unknown,
      ScType::ConstPermPosArc,
      TrainingKeynodes::nrel_reference_solution);
  while (nrelIt->Next())
    AppendUnique(solutions, nrelIt->Get(2));

  // 2. Резервный вариант: ролевое отношение emergency -> rrel_reference_solution: solution
  if (solutions.empty())
    solutions = SearchAllByRole(m_context, emergencyAddr, TrainingKeynodes::rrel_reference_solution);

  if (solutions.empty())
  {
    // Direction 1 records each solution on the emergency class.
    ScAddrVector classes{emergencyAddr};
    auto types = m_context.CreateIterator3(ScType::ConstNodeClass, ScType::ConstPermPosArc, emergencyAddr);
    while (types->Next())
      AppendUnique(classes, types->Get(0));
    for (auto const & type : classes)
    {
      auto options = m_context.CreateIterator5(
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          type,
          ScType::ConstPermPosArc,
          TrainingKeynodes::rrel_solution_for_class);
      while (options->Next())
        AppendUnique(solutions, options->Get(0));
    }
  }
  return solutions;
}

// ============================================================================
// Этап 2: ScenarioTrainingReviewAgent (составление разбора по оценке ответа)
// ============================================================================

ScAddr ScenarioTrainingReviewAgent::GetActionClass() const
{
  return TrainingKeynodes::action_review_scenario_training;
}

bool ScenarioTrainingReviewAgent::CheckInitiationCondition(
    ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc> const & event)
{
  auto const arc = event.GetOtherElement();
  if (!m_context.IsElement(arc) || m_context.GetElementType(arc) != ScType::ConstPermPosArc)
    return false;
  ScAddr question;
  return FindSessionByAnswer(m_context.GetArcSourceElement(arc), question).IsValid();
}

ScAddr ScenarioTrainingReviewAgent::GetEventSubscriptionElement() const
{
  return TrainingKeynodes::rrel_answer_evaluation;
}

ScResult ScenarioTrainingReviewAgent::DoProgram(
    ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc> const & event,
    ScAction & action)
{
  static std::mutex reviewing;
  std::lock_guard<std::mutex> lock(reviewing);
  // Событие: rrel_answer_evaluation -> evalArc, где evalArc = (answerAddr -> evaluationAddr)
  ScAddr const evalArcAddr = event.GetOtherElement();
  if (!m_context.IsElement(evalArcAddr))
    return action.FinishSuccessfully();

  auto const & [answerAddr, evaluationAddr] = m_context.GetConnectorIncidentElements(evalArcAddr);
  if (!answerAddr.IsValid() || !evaluationAddr.IsValid())
    return action.FinishSuccessfully();

  // Проверяем, относится ли этот ответ к сеансу тренировки по нештатной ситуации
  ScAddr questionAddr;
  ScAddr const sessionAddr = FindSessionByAnswer(answerAddr, questionAddr);
  if (!sessionAddr.IsValid())
    return action.FinishSuccessfully();

  action.SetArgument(1, sessionAddr);
  for (auto const & review : SearchAllByRole(m_context, sessionAddr, TrainingKeynodes::rrel_training_review))
    if (SearchByRole(m_context, review, TrainingKeynodes::rrel_employee_action) == answerAddr)
      return action.FinishSuccessfully();

  // Собираем эталонные решения из эталонного ответа тренировочного вопроса
  ScAddrVector referenceSolutions;
  ScAddr const refAnswerAddr = SearchByRole(m_context, questionAddr, TrainingKeynodes::rrel_reference_answer);
  if (refAnswerAddr.IsValid())
  {
    ScIterator3Ptr const refIt = m_context.CreateIterator3(refAnswerAddr, ScType::ConstPermPosArc, ScType::Unknown);
    while (refIt->Next())
      AppendUnique(referenceSolutions, refIt->Get(2));
  }

  // Определяем итог тренировки
  ScAddr outcomeAddr = TrainingKeynodes::training_undetermined;
  if (evaluationAddr == TrainingKeynodes::evaluation_correct)
    outcomeAddr = TrainingKeynodes::outcome_scenario_passed;
  else if (
      evaluationAddr == TrainingKeynodes::evaluation_incorrect
      || evaluationAddr == TrainingKeynodes::evaluation_partially_correct)
    outcomeAddr = TrainingKeynodes::outcome_scenario_failed;

  // Формируем структуру разбора тренировки
  ScStructure reviewStructure = m_context.GenerateStructure();

  ScAddr const reviewAddr = m_context.GenerateNode(ScType::ConstNode);
  ScAddr const revClassArc =
      m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_training_review, reviewAddr);
  reviewStructure << TrainingKeynodes::concept_training_review << revClassArc << reviewAddr;

  // Конструкция 1 (5-элементная): reviewAddr -> rrel_employee_action: answerAddr
  GenerateRoleConstruction(m_context, reviewStructure, reviewAddr, answerAddr, TrainingKeynodes::rrel_employee_action);

  // Include the choices themselves so separate attempts are readable in the review.
  if (m_context.GetElementType(answerAddr) == ScType::ConstNodeStructure)
  {
    auto choices = m_context.CreateIterator3(answerAddr, ScType::ConstPermPosArc, ScType::Unknown);
    while (choices->Next())
    {
      auto roles = m_context.CreateIterator3(ScType::ConstNodeRole, ScType::ConstPermPosArc, choices->Get(1));
      if (!roles->Next())
        reviewStructure << choices->Get(1) << choices->Get(2);
    }
  }

  // Конструкция 2 (5-элементная, для каждого эталонного решения):
  // reviewAddr -> rrel_reference_solution: solutionAddr
  for (ScAddr const & solutionAddr : referenceSolutions)
  {
    GenerateRoleConstruction(
        m_context, reviewStructure, reviewAddr, solutionAddr, TrainingKeynodes::rrel_reference_solution);
  }

  // Конструкция 3 (5-элементная): reviewAddr -> rrel_evaluation: evaluationAddr
  GenerateRoleConstruction(m_context, reviewStructure, reviewAddr, evaluationAddr, TrainingKeynodes::rrel_evaluation);

  // Конструкция 4 (5-элементная): reviewAddr -> rrel_outcome: outcomeAddr
  GenerateRoleConstruction(m_context, reviewStructure, reviewAddr, outcomeAddr, TrainingKeynodes::rrel_outcome);

  // Привязываем разбор и его структуру к сеансу тренировки
  ScAddr const sessRevArc = m_context.GenerateConnector(ScType::ConstPermPosArc, sessionAddr, reviewAddr);
  auto sessionReviewRoleArc =
      m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::rrel_training_review, sessRevArc);

  reviewStructure << sessionAddr << sessRevArc << TrainingKeynodes::rrel_training_review << sessionReviewRoleArc;

  ScAddr const resArc = m_context.GenerateConnector(ScType::ConstCommonArc, sessionAddr, reviewStructure);
  m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::nrel_result, resArc);

  auto owners = m_context.CreateIterator5(
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      sessionAddr,
      ScType::ConstPermPosArc,
      TrainingKeynodes::rrel_training_session);
  while (owners->Next())
  {
    auto owner = owners->Get(0);
    if (!BelongsToClass(m_context, owner, TrainingKeynodes::action_conduct_scenario_training))
      continue;
    auto results = m_context.CreateIterator5(
        owner, ScType::ConstCommonArc, ScType::ConstNodeStructure, ScType::ConstPermPosArc, ScKeynodes::nrel_result);
    if (results->Next())
    {
      auto original = m_context.ConvertToStructure(results->Get(2));
      original << reviewStructure << sessRevArc << TrainingKeynodes::rrel_training_review << sessionReviewRoleArc;
      auto elements = m_context.CreateIterator3(reviewStructure, ScType::ConstPermPosArc, ScType::Unknown);
      while (elements->Next())
        original << elements->Get(2);
    }
  }

  action.SetResult(reviewStructure);
  return action.FinishSuccessfully();
}

ScAddr ScenarioTrainingReviewAgent::FindSessionByAnswer(ScAddr const & answerAddr, ScAddr & outQuestionAddr)
{
  ScAddr const questionAddr = QuestionForAnswer(m_context, answerAddr);
  if (!questionAddr.IsValid())
    return ScAddr::Empty;

  // Шаг 2: ищем сеанс concept_scenario_training_session, связанный с этим вопросом через rrel_question
  ScIterator5Ptr const sessIt = m_context.CreateIterator5(
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      questionAddr,
      ScType::ConstPermPosArc,
      TrainingKeynodes::rrel_question);
  ScAddr const employee = SearchByRole(m_context, answerAddr, TrainingKeynodes::rrel_employee);
  while (sessIt->Next())
  {
    ScAddr const sessionAddr = sessIt->Get(0);
    if (BelongsToClass(m_context, sessionAddr, TrainingKeynodes::concept_scenario_training_session)
        && (!employee.IsValid() || employee == SearchByRole(m_context, sessionAddr, TrainingKeynodes::rrel_employee)))
    {
      outQuestionAddr = questionAddr;
      return sessionAddr;
    }
  }

  return ScAddr::Empty;
}

ScAddr ShowTrainingReviewAgent::GetActionClass() const
{
  return TrainingKeynodes::action_show_training_review;
}

ScResult ShowTrainingReviewAgent::DoProgram(ScAction & action)
{
  auto const [question] = action.GetArguments<1>();
  if (!m_context.IsElement(question))
    return InsufficientData(action);
  auto sessions = m_context.CreateIterator5(
      ScType::ConstNode, ScType::ConstPermPosArc, question, ScType::ConstPermPosArc, TrainingKeynodes::rrel_question);
  while (sessions->Next())
  {
    ScAddr const session = sessions->Get(0);
    if (!BelongsToClass(m_context, session, TrainingKeynodes::concept_scenario_training_session))
      continue;
    auto result = m_context.GenerateStructure();
    for (size_t i = 0; i < 5; ++i)
      result << sessions->Get(i);
    auto reviews = m_context.CreateIterator5(
        session, ScType::ConstCommonArc, ScType::ConstNodeStructure, ScType::ConstPermPosArc, ScKeynodes::nrel_result);
    bool ready = false;
    while (reviews->Next())
    {
      auto members = m_context.CreateIterator3(reviews->Get(2), ScType::ConstPermPosArc, ScType::Unknown);
      while (members->Next())
        result << members->Get(2);
      ready = true;
    }
    if (!ready)
      result << TrainingKeynodes::training_review_pending;
    action.SetResult(result);
    return action.FinishSuccessfully();
  }
  return InsufficientData(action);
}
