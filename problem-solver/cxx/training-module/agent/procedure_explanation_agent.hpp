#pragma once

#include <sc-memory/sc_agent.hpp>

class ProcedureExplanationAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;

  ScResult DoProgram(ScAction & action) override;

private:
  enum class StepOrder
  {
    ExplicitList,
    NumberedEntries,
    Transitions
  };
  ScAddrVector CollectOrderedSteps(ScAddr const & procedureAddr, StepOrder & order);
  ScAddrVector TraverseStepsFromInitial(ScAddrVector const & initialSteps, StepOrder order, ScStructure & result);
  ScAddrVector FindStepStates(ScAddr const & stepAddr);
  void DescribeStep(ScAddr const & stepAddr, ScStructure & result);
};
