#pragma once

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_event.hpp>

class AnswerCheckingAgent : public ScAgent<ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc>>
{
public:
  ScAddr GetActionClass() const override;
  ScAddr GetEventSubscriptionElement() const override;

  ScResult DoProgram(
      ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc> const & event,
      ScAction & action) override;

private:
  ScResult CheckAnswer(ScAddr const & waitingArc, ScAddr const & answer, ScAction & action);
  ScAddrVector ExtractElements(ScAddr const & containerAddr);
  bool MatchesElement(ScAddr const & reference, ScAddr const & submitted);
};

class SubmitTrainingAnswerAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;
};
