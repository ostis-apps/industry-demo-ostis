/*
 * This source file is part of an OSTIS project. For the latest info, see http://ostis.net
 * Distributed under the MIT License
 * (See accompanying file COPYING.MIT or copy at http://opensource.org/licenses/MIT)
 */

#include "exampleModule.hpp"

#include "agents/SubdividingSearchAgent.hpp"
#include "agents/IsomorphicSearchAgent.hpp"
#include "agents/PathSearchAgent.hpp"
#include "agents/ExampleInferenceAgent.hpp"

#include "agents/AgentForDefinitionAnswer.hpp"
#include "agents/AgentForClassificationAnswer.hpp"
#include "agents/AgentForRelationExplanation.hpp"
#include "agents/AgentForConceptSearch.hpp"

SC_MODULE_REGISTER(ExampleModule)
    ->Agent<SubdividingSearchAgent>()
    ->Agent<IsomorphicSearchAgent>()
    ->Agent<PathSearchAgent>()
    ->Agent<ExampleInferenceAgent>()
    ->Agent<AgentForDefinitionAnswer>()
    ->Agent<AgentForClassificationAnswer>()
    ->Agent<AgentForRelationExplanation>()
    ->Agent<AgentForConceptSearch>();
