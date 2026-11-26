#include "training_module.hpp"
#include "agent/procedure_explanation_agent.hpp"
#include "agent/quiz_generation_agent.hpp"
#include "agent/answer_checking_agent.hpp"
#include "agent/scenario_training_agent.hpp"

SC_MODULE_REGISTER(TrainingModule)
    ->Agent<ProcedureExplanationAgent>()
    ->Agent<QuizGenerationAgent>()
    ->Agent<AnswerCheckingAgent>()
    ->Agent<SubmitTrainingAnswerAgent>()
    ->Agent<ScenarioTrainingAgent>()
    ->Agent<ScenarioTrainingReviewAgent>()
    ->Agent<ShowTrainingReviewAgent>();
