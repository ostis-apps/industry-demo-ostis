#include "answer_checking_agent.hpp"

#include <mutex>
#include <string>

#include <sc-memory/sc_memory.hpp>

#include "keynodes/training_keynodes.hpp"
#include "utils/training_utils.hpp"

using namespace training_utils;

ScAddr AnswerCheckingAgent::GetActionClass() const
{
  return TrainingKeynodes::action_check_employee_answer;
}

ScAddr AnswerCheckingAgent::GetEventSubscriptionElement() const
{
  return TrainingKeynodes::concept_employee_answer_awaiting_check;
}

ScResult AnswerCheckingAgent::DoProgram(
    ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc> const & event,
    ScAction & action)
{
  static std::mutex checking;
  std::lock_guard<std::mutex> lock(checking);
  ScAddr const waitingArcAddr = event.GetArc();
  ScAddr const answerAddr = event.GetOtherElement();

  if (!m_context.IsElement(answerAddr))
  {
    m_logger.Error("AnswerCheckingAgent: employee answer element is invalid.");
    return action.FinishWithError();
  }

  auto submissions = m_context.CreateIterator5(
      ScType::ConstNode, ScType::ConstPermPosArc, answerAddr, ScType::ConstPermPosArc, TrainingKeynodes::rrel_employee_answer);
  while (submissions->Next())
  {
    ScAddr const owner = submissions->Get(0);
    if (!BelongsToClass(m_context, owner, TrainingKeynodes::action_submit_training_answer))
      continue;
    auto submission = m_context.ConvertToAction(owner);
    if (!submission.IsInitiated() || submission.IsFinished())
      continue;
    try
    {
      auto result = CheckAnswer(waitingArcAddr, answerAddr, action);
      submission.SetResult(action.GetResult());
      if (action.IsFinishedSuccessfully())
        submission.FinishSuccessfully();
      else
        submission.FinishUnsuccessfully();
      return result;
    }
    catch (utils::ScException const &)
    {
      if (!submission.IsFinished())
        submission.FinishWithError();
      throw;
    }
  }
  return CheckAnswer(waitingArcAddr, answerAddr, action);
}

ScResult AnswerCheckingAgent::CheckAnswer(ScAddr const & waitingArcAddr, ScAddr const & answerAddr, ScAction & action)
{
  if (BelongsToClass(m_context, answerAddr, TrainingKeynodes::concept_checked_employee_answer))
  {
    if (m_context.IsElement(waitingArcAddr))
      m_context.EraseElement(waitingArcAddr);
    return action.FinishSuccessfully();
  }

  action.SetArgument(1, answerAddr);
  ScAddr const questionAddr = QuestionForAnswer(m_context, answerAddr);
  if (!questionAddr.IsValid())
  {
    m_logger.Error("AnswerCheckingAgent: question for employee answer not found.");
    return InsufficientData(action);
  }

  ScAddr const refAnswerAddr = SearchByRole(m_context, questionAddr, TrainingKeynodes::rrel_reference_answer);
  ScAddrVector const referenceElements = refAnswerAddr.IsValid() ? ExtractElements(refAnswerAddr) : ScAddrVector{};
  ScAddrVector employeeElements = ExtractElements(answerAddr);
  ScAddrVector const submittedElements = employeeElements;
  if (ContainsAddr(referenceElements, answerAddr))
    employeeElements = {answerAddr};

  ScAddrVector missingElements;
  size_t matchedCount = 0;

  for (ScAddr const & refElem : referenceElements)
  {
    auto match = std::find_if(
        employeeElements.begin(),
        employeeElements.end(),
        [&](ScAddr const & item)
        {
          return MatchesElement(refElem, item);
        });
    if (match != employeeElements.end())
    {
      ++matchedCount;
      employeeElements.erase(match);
    }
    else
      missingElements.push_back(refElem);
  }

  // Выбираем оценку по доле совпадения с эталоном
  ScAddr evaluationAddr = TrainingKeynodes::evaluation_incorrect;
  if (referenceElements.empty())
    evaluationAddr = TrainingKeynodes::training_undetermined;
  else if (missingElements.empty() && employeeElements.empty())
    evaluationAddr = TrainingKeynodes::evaluation_correct;
  else if (matchedCount > 0)
    evaluationAddr = TrainingKeynodes::evaluation_partially_correct;

  ScStructure resultStructure = m_context.GenerateStructure();

  resultStructure.Append(answerAddr, ScKeynodes::rrel_key_sc_element);
  auto questionLinks = m_context.CreateIterator5(
      answerAddr, ScType::ConstPermPosArc, questionAddr, ScType::ConstPermPosArc, TrainingKeynodes::rrel_question);
  if (questionLinks->Next())
    for (size_t i = 0; i < 5; ++i)
      resultStructure << questionLinks->Get(i);
  for (auto const & element : submittedElements)
  {
    auto members = m_context.CreateIterator3(answerAddr, ScType::ConstPermPosArc, element);
    if (members->Next())
      resultStructure << members->Get(1) << element;
  }

  ScAddr const employee = SearchByRole(m_context, questionAddr, TrainingKeynodes::rrel_employee);
  if (employee.IsValid() && !SearchByRole(m_context, answerAddr, TrainingKeynodes::rrel_employee).IsValid())
    GenerateRoleConstruction(m_context, resultStructure, answerAddr, employee, TrainingKeynodes::rrel_employee);

  // Конструкция 2 (5-элементная, для каждого пропущенного элемента):
  // answerAddr -> rrel_missing_element: missingElem
  for (ScAddr const & missingElem : missingElements)
  {
    GenerateRoleConstruction(
        m_context, resultStructure, answerAddr, missingElem, TrainingKeynodes::rrel_missing_element);
  }

  // Конструкция 3 (5-элементная): answerAddr -> rrel_hint: hintAddr (из rrel_basis вопроса)
  ScAddr const basisAddr = SearchByRole(m_context, questionAddr, TrainingKeynodes::rrel_basis);
  if (basisAddr.IsValid())
  {
    GenerateRoleConstruction(m_context, resultStructure, answerAddr, basisAddr, TrainingKeynodes::rrel_hint);
  }

  // Конструкция 1 (5-элементная): answerAddr -> rrel_answer_evaluation: evaluationAddr
  // Генерируется последней, чтобы подписчики на появление оценки (агент №8) видели полную картину.
  GenerateRoleConstruction(
      m_context, resultStructure, answerAddr, evaluationAddr, TrainingKeynodes::rrel_answer_evaluation);

  // Снимаем ответ с ожидания и помечаем как проверенный
  if (m_context.IsElement(waitingArcAddr))
    m_context.EraseElement(waitingArcAddr);

  m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_checked_employee_answer, answerAddr);

  action.SetResult(resultStructure);
  return action.FinishSuccessfully();
}

ScAddrVector AnswerCheckingAgent::ExtractElements(ScAddr const & containerAddr)
{
  ScAddrVector elements;

  ScIterator3Ptr const it3 = m_context.CreateIterator3(containerAddr, ScType::ConstPermPosArc, ScType::Unknown);
  while (it3->Next())
  {
    ScAddr const arcAddr = it3->Get(1);
    ScAddr const elemAddr = it3->Get(2);

    // Исключаем служебные ролевые связи самого ответа (rrel_question, rrel_employee, оценки и т.д.)
    bool const isServiceRole = BelongsToClass(m_context, arcAddr, TrainingKeynodes::rrel_question)
                               || BelongsToClass(m_context, arcAddr, TrainingKeynodes::rrel_employee)
                               || BelongsToClass(m_context, arcAddr, TrainingKeynodes::rrel_answer_evaluation)
                               || BelongsToClass(m_context, arcAddr, TrainingKeynodes::rrel_missing_element)
                               || BelongsToClass(m_context, arcAddr, TrainingKeynodes::rrel_hint);

    if (!isServiceRole)
      AppendUnique(elements, elemAddr);
  }

  // Если ответ или эталон задан одиночным узлом/ссылкой без вложенных дуг
  if (elements.empty() && m_context.GetElementType(containerAddr) != ScType::ConstNodeStructure
      && !BelongsToClass(m_context, containerAddr, TrainingKeynodes::concept_reference_answer))
    elements.push_back(containerAddr);

  return elements;
}

bool AnswerCheckingAgent::MatchesElement(ScAddr const & reference, ScAddr const & submitted)
{
  if (reference == submitted)
    return true;
  if (!m_context.GetElementType(reference).IsLink() || !m_context.GetElementType(submitted).IsLink())
    return false;
  std::string expectedText, submittedText;
  return m_context.GetLinkContent(reference, expectedText) && m_context.GetLinkContent(submitted, submittedText)
         && expectedText == submittedText;
}

ScAddr SubmitTrainingAnswerAgent::GetActionClass() const
{
  return TrainingKeynodes::action_submit_training_answer;
}

ScResult SubmitTrainingAnswerAgent::DoProgram(ScAction & action)
{
  auto const [question, first] = action.GetArguments<2>();
  if (!m_context.IsElement(question) || !m_context.IsElement(first)
      || !SearchByRole(m_context, question, TrainingKeynodes::rrel_reference_answer).IsValid())
    return InsufficientData(action);

  // A menu template binds two required arguments. sc-web keeps all the selected
  // elements in the original command tuple, including additional answer elements.
  ScAddr arguments = action;
  auto instances = m_context.CreateIterator5(
      GetActionClass(), ScType::ConstPermPosArc, action, ScType::ConstPermPosArc, ScType::ConstNodeStructure);
  while (instances->Next())
  {
    auto commands = m_context.CreateIterator5(
        ScType::ConstNode, ScType::ConstCommonArc, instances->Get(4), ScType::ConstPermPosArc,
        TrainingKeynodes::ui_nrel_command_result);
    if (commands->Next())
    {
      auto tuple = SearchByRole(m_context, commands->Get(0), TrainingKeynodes::ui_rrel_command_arguments);
      if (tuple.IsValid())
      {
        arguments = tuple;
        break;
      }
    }
  }

  auto answer = m_context.GenerateStructure();
  answer << first;
  for (size_t i = 3; i <= ScKeynodes::GetRrelIndexNum(); ++i)
  {
    auto next = SearchByRole(m_context, arguments, ScKeynodes::GetRrelIndex(i));
    if (!next.IsValid())
      break;
    answer << next;
  }
  auto questionArc = m_context.GenerateConnector(ScType::ConstPermPosArc, answer, question);
  m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::rrel_question, questionArc);
  auto submissionArc = m_context.GenerateConnector(ScType::ConstPermPosArc, action, answer);
  m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::rrel_employee_answer, submissionArc);

  // The checking event finishes this same UI action with its result. Do not block an event worker.
  m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_employee_answer_awaiting_check, answer);
  return ScActionInitiatedAgent::DoProgram(action);
}
