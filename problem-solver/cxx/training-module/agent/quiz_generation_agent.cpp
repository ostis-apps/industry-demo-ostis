#include "quiz_generation_agent.hpp"

#include <algorithm>

#include <sc-memory/sc_memory.hpp>

#include "keynodes/training_keynodes.hpp"
#include "utils/training_utils.hpp"

using namespace training_utils;

ScAddr QuizGenerationAgent::GetActionClass() const
{
  return TrainingKeynodes::action_generate_quiz;
}

ScResult QuizGenerationAgent::DoProgram(ScAction & action)
{
  auto const & [situationAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(situationAddr))
  {
    m_logger.Error("QuizGenerationAgent: learning situation argument is invalid.");
    return action.FinishWithError();
  }

  ScAddr const employeeAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_employee);
  ScAddr const topicAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_topic);

  if (!employeeAddr.IsValid() || !topicAddr.IsValid())
  {
    m_logger.Error("QuizGenerationAgent: employee or topic is not specified in learning situation.");
    return action.FinishWithError();
  }

  // Необязательные параметры: число вопросов и сложность
  size_t questionCount = 0;
  ScAddr const countLinkAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_question_count);
  bool const hasExplicitCount = TryReadPositiveInt(m_context, countLinkAddr, questionCount);

  ScAddr difficultyAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_difficulty);
  bool const hasExplicitDifficulty = difficultyAddr.IsValid();

  if ((countLinkAddr.IsValid() && !hasExplicitCount)
      || (hasExplicitDifficulty && difficultyAddr != TrainingKeynodes::difficulty_easy
          && difficultyAddr != TrainingKeynodes::difficulty_medium
          && difficultyAddr != TrainingKeynodes::difficulty_hard))
    return action.FinishWithError();

  if (!hasExplicitCount || !hasExplicitDifficulty)
  {
    DetermineParametersFromHistory(
        situationAddr, employeeAddr, hasExplicitCount, hasExplicitDifficulty, questionCount, difficultyAddr);
  }

  std::vector<TopicFact> const facts = CollectTopicFacts(topicAddr, difficultyAddr);
  if (facts.empty())
  {
    m_logger.Debug("QuizGenerationAgent: no facts found in KB for given topic.");
    return InsufficientData(action);
  }

  if (hasExplicitCount && questionCount > facts.size())
    return InsufficientData(action);

  size_t const actualCount = std::min(questionCount, facts.size());
  ScStructure resultStructure = m_context.GenerateStructure();

  // Узел набора проверочных вопросов по теме
  ScAddr const quizSetAddr = m_context.GenerateNode(ScType::ConstNode);
  ScAddr const quizClassArc =
      m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_quiz_question_set, quizSetAddr);
  resultStructure << TrainingKeynodes::concept_quiz_question_set << quizClassArc << quizSetAddr;

  // Связываем набор вопросов с темой
  ScAddr const topicArc = m_context.GenerateConnector(ScType::ConstPermPosArc, quizSetAddr, topicAddr);
  ScAddr const topicRoleArc =
      m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::rrel_topic, topicArc);
  resultStructure << topicArc << topicAddr << TrainingKeynodes::rrel_topic << topicRoleArc;

  for (size_t i = 0; i < actualCount; ++i)
  {
    TopicFact const & fact = facts[i];
    ScAddr const questionAddr = m_context.GenerateNode(ScType::ConstNode);
    ScAddr const qClassArc =
        m_context.GenerateConnector(ScType::ConstPermPosArc, TrainingKeynodes::concept_quiz_question, questionAddr);
    resultStructure << TrainingKeynodes::concept_quiz_question << qClassArc << questionAddr;

    SetQuestionText(m_context, resultStructure, questionAddr, fact.prompt);
    GenerateRoleConstruction(m_context, resultStructure, questionAddr, employeeAddr, TrainingKeynodes::rrel_employee);

    // Конструкция 1 (9-элементная, продолжается по числу вопросов): quizSetAddr -> (i+1)': questionAddr
    ScAddr const numericRoleAddr = ResolveNumericRole(m_context, i + 1);
    GenerateRoleConstruction(m_context, resultStructure, quizSetAddr, questionAddr, numericRoleAddr);

    // Конструкция 2 (5-элементная): questionAddr -> rrel_difficulty: difficultyAddr
    GenerateRoleConstruction(
        m_context, resultStructure, questionAddr, difficultyAddr, TrainingKeynodes::rrel_difficulty);

    // Структура-эталон ответа на вопрос
    ScStructure refAnswerStruct = m_context.GenerateStructure();
    ScAddr const refClassArc = m_context.GenerateConnector(
        ScType::ConstPermPosArc, TrainingKeynodes::concept_reference_answer, refAnswerStruct);
    resultStructure << TrainingKeynodes::concept_reference_answer << refClassArc;

    for (ScAddr const & refElem : fact.referenceElements)
    {
      ScAddr const elemArc = m_context.GenerateConnector(ScType::ConstPermPosArc, refAnswerStruct, refElem);
      resultStructure << elemArc << refElem;
    }

    // Конструкция 3 (5-элементная): questionAddr -> rrel_reference_answer: refAnswerStruct
    GenerateRoleConstruction(
        m_context, resultStructure, questionAddr, refAnswerStruct, TrainingKeynodes::rrel_reference_answer);

    // Конструкция 4 (5-элементная): questionAddr -> rrel_basis: fact.basisAddr
    GenerateRoleConstruction(m_context, resultStructure, questionAddr, fact.basisAddr, TrainingKeynodes::rrel_basis);
  }

  action.SetResult(resultStructure);
  return action.FinishSuccessfully();
}

void QuizGenerationAgent::DetermineParametersFromHistory(
    ScAddr const & situationAddr,
    ScAddr const & employeeAddr,
    bool hasExplicitCount,
    bool hasExplicitDifficulty,
    size_t & questionCount,
    ScAddr & difficultyAddr)
{
  // Собираем прошлые ответы из узла истории (rrel_answer_history на ситуации или на сотруднике)
  ScAddrVector pastAnswers;
  ScAddr historyAddr = SearchByRole(m_context, situationAddr, TrainingKeynodes::rrel_answer_history);
  if (!historyAddr.IsValid())
    historyAddr = SearchByRole(m_context, employeeAddr, TrainingKeynodes::rrel_answer_history);

  if (historyAddr.IsValid())
  {
    ScIterator3Ptr const histIt = m_context.CreateIterator3(historyAddr, ScType::ConstPermPosArc, ScType::Unknown);
    while (histIt->Next())
      AppendUnique(pastAnswers, histIt->Get(2));
  }

  // Также учитываем проверенные ответы, напрямую привязанные к сотруднику
  ScIterator5Ptr const empAnsIt = m_context.CreateIterator5(
      ScType::Unknown, ScType::ConstPermPosArc, employeeAddr, ScType::ConstPermPosArc, TrainingKeynodes::rrel_employee);
  while (empAnsIt->Next())
  {
    ScAddr const candidate = empAnsIt->Get(0);
    if (SearchByRole(m_context, candidate, TrainingKeynodes::rrel_answer_evaluation).IsValid())
      AppendUnique(pastAnswers, candidate);
  }

  size_t totalEvaluated = 0;
  double scoreSum = 0.0;

  for (ScAddr const & ansAddr : pastAnswers)
  {
    ScAddr const evalAddr = SearchByRole(m_context, ansAddr, TrainingKeynodes::rrel_answer_evaluation);
    if (evalAddr != TrainingKeynodes::evaluation_correct && evalAddr != TrainingKeynodes::evaluation_partially_correct
        && evalAddr != TrainingKeynodes::evaluation_incorrect)
      continue;

    ++totalEvaluated;
    if (evalAddr == TrainingKeynodes::evaluation_correct)
      scoreSum += 1.0;
    else if (evalAddr == TrainingKeynodes::evaluation_partially_correct)
      scoreSum += 0.5;
  }

  if (totalEvaluated == 0)
  {
    if (!hasExplicitDifficulty)
      difficultyAddr = TrainingKeynodes::difficulty_medium;
    if (!hasExplicitCount)
      questionCount = 3;
    return;
  }

  double const accuracy = scoreSum / static_cast<double>(totalEvaluated);
  if (accuracy >= 0.8)
  {
    if (!hasExplicitDifficulty)
      difficultyAddr = TrainingKeynodes::difficulty_hard;
    if (!hasExplicitCount)
      questionCount = 3;
  }
  else if (accuracy >= 0.5)
  {
    if (!hasExplicitDifficulty)
      difficultyAddr = TrainingKeynodes::difficulty_medium;
    if (!hasExplicitCount)
      questionCount = 4;
  }
  else
  {
    if (!hasExplicitDifficulty)
      difficultyAddr = TrainingKeynodes::difficulty_easy;
    if (!hasExplicitCount)
      questionCount = 5;
  }
}

std::vector<QuizGenerationAgent::TopicFact> QuizGenerationAgent::CollectTopicFacts(
    ScAddr const & topicAddr,
    ScAddr const & difficultyAddr)
{
  ScAddrVector domains{topicAddr};
  ScAddrVector concepts;
  for (size_t i = 0; i < domains.size(); ++i)
  {
    ScAddr const domain = domains[i];
    auto children = m_context.CreateIterator5(
        domain,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        TrainingKeynodes::nrel_private_subject_domain);
    while (children->Next())
      AppendUnique(domains, children->Get(2));
    for (ScAddr const & role :
         {TrainingKeynodes::rrel_maximum_studied_object_class,
          TrainingKeynodes::rrel_not_maximum_studied_object_class,
          TrainingKeynodes::rrel_explored_relation})
      for (auto const & concept : SearchAllByRole(m_context, domain, role))
        AppendUnique(concepts, concept);
  }

  if (concepts.empty())
  {
    AppendUnique(concepts, topicAddr);
    auto members = m_context.CreateIterator3(topicAddr, ScType::ConstPermPosArc, ScType::ConstNode);
    while (members->Next())
      AppendUnique(concepts, members->Get(2));
    auto children = m_context.CreateIterator5(
        topicAddr,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        TrainingKeynodes::nrel_inclusion);
    while (children->Next())
      AppendUnique(concepts, children->Get(2));
  }

  std::vector<TopicFact> facts;
  for (auto const & concept : concepts)
  {
    auto const name = DisplayName(m_context, concept);
    ScAddr const definition = FindConceptDefinition(concept);
    if (definition.IsValid())
      facts.push_back({"Какое понятие описано в приложенном определении или пояснении?", definition, {concept}});
    if (difficultyAddr == TrainingKeynodes::difficulty_easy && definition.IsValid())
      continue;

    for (ScAddr const & relation : {TrainingKeynodes::nrel_inclusion, TrainingKeynodes::nrel_contains})
    {
      auto it = m_context.CreateIterator5(
          concept, ScType::ConstCommonArc, ScType::ConstNode, ScType::ConstPermPosArc, relation);
      ScAddrVector targets;
      while (it->Next())
        AppendUnique(targets, it->Get(2));
      if (targets.empty() || name.empty())
        continue;
      std::string prefix = relation == TrainingKeynodes::nrel_inclusion ? "Какие непосредственные подклассы имеет «"
                                                                        : "Какие части непосредственно содержит «";
      TopicFact fact{prefix + name + "»?", concept, targets};
      facts.push_back(std::move(fact));
    }
  }
  if (difficultyAddr == TrainingKeynodes::difficulty_hard)
    std::stable_sort(
        facts.begin(),
        facts.end(),
        [](TopicFact const & a, TopicFact const & b)
        {
          return a.referenceElements.size() > b.referenceElements.size();
        });
  return facts;
}

ScAddr QuizGenerationAgent::FindConceptDefinition(ScAddr const & conceptAddr)
{
  // Ищем фрагмент definition или explanation по rrel_key_sc_element (как в БЗ s88-ostis и emergency)
  ScIterator5Ptr const keyIt = m_context.CreateIterator5(
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      conceptAddr,
      ScType::ConstPermPosArc,
      TrainingKeynodes::rrel_key_sc_element);
  while (keyIt->Next())
  {
    ScAddr const statementAddr = keyIt->Get(0);
    if (BelongsToClass(m_context, statementAddr, TrainingKeynodes::definition)
        || BelongsToClass(m_context, statementAddr, TrainingKeynodes::explanation))
    {
      return statementAddr;
    }
  }
  for (auto const * idtf : {"nrel_definition", "nrel_explanation"})
  {
    auto relation = m_context.SearchElementBySystemIdentifier(idtf);
    if (!relation.IsValid())
      continue;
    auto it = m_context.CreateIterator5(
        conceptAddr, ScType::ConstCommonArc, ScType::ConstNode, ScType::ConstPermPosArc, relation);
    if (it->Next())
      return it->Get(2);
  }
  return ScAddr::Empty;
}
