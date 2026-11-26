#pragma once

#include <sc-memory/sc_keynodes.hpp>

class TrainingKeynodes : public ScKeynodes
{
public:
  // --- Классы действий (запуск агентов) ---
  static inline ScKeynode const action_explain_procedure{
      "action_explain_procedure",
      ScType::ConstNodeClass};
  static inline ScKeynode const action_generate_quiz{
      "action_generate_quiz",
      ScType::ConstNodeClass};
  static inline ScKeynode const action_conduct_scenario_training{
      "action_conduct_scenario_training",
      ScType::ConstNodeClass};

  static inline ScKeynode const ui_nrel_command_result{"ui_nrel_command_result", ScType::ConstNodeNonRole};
  static inline ScKeynode const ui_rrel_command_arguments{"ui_rrel_command_arguments", ScType::ConstNodeRole};
  static inline ScKeynode const action_submit_training_answer{"action_submit_training_answer", ScType::ConstNodeClass};
  static inline ScKeynode const action_show_training_review{"action_show_training_review", ScType::ConstNodeClass};
  static inline ScKeynode const training_review_pending{"training_review_pending", ScType::ConstNode};
  static inline ScKeynode const action_check_employee_answer{"action_check_employee_answer", ScType::ConstNodeClass};
  static inline ScKeynode const action_review_scenario_training{"action_review_scenario_training", ScType::ConstNodeClass};
  static inline ScKeynode const training_undetermined{"training_undetermined", ScType::ConstNode};
  static inline ScKeynode const nrel_answer{"nrel_answer", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_private_subject_domain{"nrel_private_subject_domain", ScType::ConstNodeNonRole};
  static inline ScKeynode const rrel_solution_for_class{"rrel_solution_for_class", ScType::ConstNodeRole};

  // --- Существующие узлы БЗ (s88-ostis / khutorok-ostis / emergency) ---
  static inline ScKeynode const procedural_element{"procedural_element", ScType::ConstNodeClass};
  static inline ScKeynode const recipe_procedure{"recipe_procedure", ScType::ConstNodeClass};
  static inline ScKeynode const unit_procedure{"unit_procedure", ScType::ConstNodeClass};
  static inline ScKeynode const operation{"operation", ScType::ConstNodeClass};
  static inline ScKeynode const phase{"phase", ScType::ConstNodeClass};

  static inline ScKeynode const nrel_decomposition_of_action{
      "nrel_decomposition_of_action",
      ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_execution_order{"nrel_execution_order", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_order_of_execution{"nrel_order_of_execution", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_goto{"nrel_goto", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_then{"nrel_then", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_else{"nrel_else", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_object{"nrel_object", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_result{"nrel_result", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_inclusion{"nrel_inclusion", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_contains{"nrel_contains", ScType::ConstNodeNonRole};

  static inline ScKeynode const rrel_key_sc_element{"rrel_key_sc_element", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_maximum_studied_object_class{
      "rrel_maximum_studied_object_class",
      ScType::ConstNodeRole};
  static inline ScKeynode const rrel_not_maximum_studied_object_class{
      "rrel_not_maximum_studied_object_class",
      ScType::ConstNodeRole};
  static inline ScKeynode const rrel_explored_relation{"rrel_explored_relation", ScType::ConstNodeRole};

  static inline ScKeynode const definition{"definition", ScType::ConstNodeClass};
  static inline ScKeynode const explanation{"explanation", ScType::ConstNodeClass};
  static inline ScKeynode const emergency{"emergency", ScType::ConstNodeClass};
  static inline ScKeynode const emergency_at_the_workplace{"emergency_at_the_workplace", ScType::ConstNodeClass};

  // --- Узлы агента 5: agent_for_procedure_explanation ---
  static inline ScKeynode const concept_procedure_execution_description{
      "concept_procedure_execution_description",
      ScType::ConstNodeClass};
  static inline ScKeynode const rrel_executed_in_state{"rrel_executed_in_state", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_used_equipment{"rrel_used_equipment", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_equipment_option{"rrel_equipment_option", ScType::ConstNodeRole};
  static inline ScKeynode const selection_of_equipment{"selection_of_equipment", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_state{"nrel_state", ScType::ConstNodeNonRole};
  static inline ScKeynode const state_running{"state_running", ScType::ConstNode};

  // --- Узлы ситуации обучения и агента 6: agent_for_quiz_generation ---
  static inline ScKeynode const concept_learning_situation{"concept_learning_situation", ScType::ConstNodeClass};
  static inline ScKeynode const concept_quiz_question_set{"concept_quiz_question_set", ScType::ConstNodeClass};
  static inline ScKeynode const concept_quiz_question{"concept_quiz_question", ScType::ConstNodeClass};
  static inline ScKeynode const concept_reference_answer{"concept_reference_answer", ScType::ConstNodeClass};

  static inline ScKeynode const rrel_employee{"rrel_employee", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_topic{"rrel_topic", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_answer_history{"rrel_answer_history", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_question_count{"rrel_question_count", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_difficulty{"rrel_difficulty", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_reference_answer{"rrel_reference_answer", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_basis{"rrel_basis", ScType::ConstNodeRole};

  static inline ScKeynode const difficulty_easy{"difficulty_easy", ScType::ConstNode};
  static inline ScKeynode const difficulty_medium{"difficulty_medium", ScType::ConstNode};
  static inline ScKeynode const difficulty_hard{"difficulty_hard", ScType::ConstNode};

  // --- Узлы агента 7: agent_for_answer_checking ---
  static inline ScKeynode const concept_employee_answer_awaiting_check{
      "concept_employee_answer_awaiting_check",
      ScType::ConstNodeClass};
  static inline ScKeynode const concept_checked_employee_answer{
      "concept_checked_employee_answer",
      ScType::ConstNodeClass};

  static inline ScKeynode const rrel_question{"rrel_question", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_employee_answer{"rrel_employee_answer", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_answer_evaluation{"rrel_answer_evaluation", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_missing_element{"rrel_missing_element", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_hint{"rrel_hint", ScType::ConstNodeRole};

  static inline ScKeynode const evaluation_correct{"evaluation_correct", ScType::ConstNode};
  static inline ScKeynode const evaluation_partially_correct{"evaluation_partially_correct", ScType::ConstNode};
  static inline ScKeynode const evaluation_incorrect{"evaluation_incorrect", ScType::ConstNode};

  // --- Узлы агента 8: agent_for_scenario_training ---
  static inline ScKeynode const concept_scenario_training_session{
      "concept_scenario_training_session",
      ScType::ConstNodeClass};
  static inline ScKeynode const concept_training_question{"concept_training_question", ScType::ConstNodeClass};
  static inline ScKeynode const concept_training_review{"concept_training_review", ScType::ConstNodeClass};

  static inline ScKeynode const rrel_emergency_situation{"rrel_emergency_situation", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_training_session{"rrel_training_session", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_training_review{"rrel_training_review", ScType::ConstNodeRole};
  static inline ScKeynode const nrel_reference_solution{"nrel_reference_solution", ScType::ConstNodeNonRole};

  static inline ScKeynode const rrel_employee_action{"rrel_employee_action", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_reference_solution{"rrel_reference_solution", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_evaluation{"rrel_evaluation", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_outcome{"rrel_outcome", ScType::ConstNodeRole};

  static inline ScKeynode const outcome_scenario_passed{"outcome_scenario_passed", ScType::ConstNode};
  static inline ScKeynode const outcome_scenario_failed{"outcome_scenario_failed", ScType::ConstNode};
};