#pragma once

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_event.hpp>

// Этап 1: инициирование действия «провести тренировку по ситуации»
class ScenarioTrainingAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;

  ScResult DoProgram(ScAction & action) override;

private:
  ScAddrVector FindReferenceSolutions(ScAddr const & emergencyAddr);
};

// Этап 2: появление оценки ответа сотрудника в сеансе тренировки -> построение разбора
class ScenarioTrainingReviewAgent : public ScAgent<ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc>>
{
public:
  ScAddr GetActionClass() const override;
  bool CheckInitiationCondition(ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc> const & event) override;
  ScAddr GetEventSubscriptionElement() const override;

  ScResult DoProgram(
      ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc> const & event,
      ScAction & action) override;

private:
  ScAddr FindSessionByAnswer(ScAddr const & answerAddr, ScAddr & outQuestionAddr);
};

class ShowTrainingReviewAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;
};
