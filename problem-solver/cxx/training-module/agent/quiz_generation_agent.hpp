#pragma once

#include <utility>
#include <vector>

#include <sc-memory/sc_agent.hpp>

class QuizGenerationAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;

  ScResult DoProgram(ScAction & action) override;

private:
  struct TopicFact
  {
    std::string prompt;
    ScAddr basisAddr;
    ScAddrVector referenceElements;
  };

  void DetermineParametersFromHistory(
      ScAddr const & situationAddr,
      ScAddr const & employeeAddr,
      bool hasExplicitCount,
      bool hasExplicitDifficulty,
      size_t & questionCount,
      ScAddr & difficultyAddr);

  std::vector<TopicFact> CollectTopicFacts(ScAddr const & topicAddr, ScAddr const & difficultyAddr);
  ScAddr FindConceptDefinition(ScAddr const & conceptAddr);
};
