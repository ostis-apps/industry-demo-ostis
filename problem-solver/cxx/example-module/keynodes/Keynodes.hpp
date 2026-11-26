/*
 * This source file is part of an OSTIS project. For the latest info, see
 * http://ostis.net Distributed under the MIT License
 */

#pragma once

#include <sc-memory/sc_addr.hpp>
#include <sc-memory/sc_keynodes.hpp>

class Keynodes : public ScKeynodes
{
public:
  // --- уже существовавшие ключевые sc-элементы модуля ---
  static inline ScKeynode const action_search_subdividing{"action_search_subdividing", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_subdividing{"nrel_subdividing", ScType::ConstNodeNonRole};
  static inline ScKeynode const action_search_isomorphic_structures{
      "action_search_isomorphic_structures",
      ScType::ConstNodeClass};
  static inline ScKeynode const nrel_search_result{"nrel_search_result", ScType::ConstNodeNonRole};
  static inline ScKeynode const empty_set{"empty_set", ScType::ConstNodeClass};
  static inline ScKeynode const action_search_minimum_path{"action_search_minimum_path", ScType::ConstNodeClass};
  static inline ScKeynode const action_example_inference{"action_example_inference", ScType::ConstNodeClass};

  // --- классы действий (вопросов) для агентов ответов ---
  // вопрос «что такое X»
  static inline ScKeynode const action_get_definition{"action_get_definition", ScType::ConstNodeClass};
  // вопрос «какие бывают X»
  static inline ScKeynode const action_get_classification{"action_get_classification", ScType::ConstNodeClass};
  // вопрос «как связаны X и Y»
  static inline ScKeynode const action_explain_relation{"action_explain_relation", ScType::ConstNodeClass};

  // --- отношения, связки которых включаются в ответ ---
  static inline ScKeynode const nrel_definition{"nrel_definition", ScType::ConstNodeNonRole};    // определение*
  static inline ScKeynode const nrel_explanation{"nrel_explanation", ScType::ConstNodeNonRole};  // пояснение*
  static inline ScKeynode const nrel_note{"nrel_note", ScType::ConstNodeNonRole};                // примечание*
  static inline ScKeynode const nrel_contains{"nrel_contains", ScType::ConstNodeNonRole};        // содержать* (ISA-88)
  // nrel_inclusion (включение*), nrel_main_idtf, rrel_key_sc_element, rrel_1..rrel_20 — из ScKeynodes

  // --- ролевые отношения для цепочки связей ---
  static inline ScKeynode const rrel_path_begin{"rrel_path_begin", ScType::ConstNodeRole};  // начало пути'
  static inline ScKeynode const rrel_path_end{"rrel_path_end", ScType::ConstNodeRole};      // конец пути'

  // --- классы sc-элементов (нужны для проверки типов узлов БЗ) ---
  static inline ScKeynode const sc_node_class{"sc_node_class", ScType::ConstNodeClass};
  static inline ScKeynode const sc_node_role_relation{"sc_node_role_relation", ScType::ConstNodeClass};
  static inline ScKeynode const sc_node_non_role_relation{"sc_node_non_role_relation", ScType::ConstNodeClass};

  // --- форма представления текстов, принятая в БЗ ISA-88/Савушкин ---
  static inline ScKeynode const definition{"definition", ScType::ConstNodeClass};
  static inline ScKeynode const explanation{"explanation", ScType::ConstNodeClass};
  static inline ScKeynode const note{"note", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_sc_text_translation{"nrel_sc_text_translation", ScType::ConstNodeNonRole};
  static inline ScKeynode const rrel_example{"rrel_example", ScType::ConstNodeRole};
    // вопрос «что соответствует запросу»
  static inline ScKeynode const action_search_concepts{"action_search_concepts", ScType::ConstNodeClass};

  // ролевое отношение «основание совпадения'»
  static inline ScKeynode const rrel_match_reason{"rrel_match_reason", ScType::ConstNodeRole};

  // lang_ru, nrel_main_idtf, nrel_idtf уже есть в ScKeynodes; nrel_definition добавлен ранее
  static inline ScKeynode const lang_en{"lang_en", ScType::ConstNodeClass};
};

