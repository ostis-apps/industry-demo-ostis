"""Run the four training agents on a running local OSTIS instance."""
import argparse
import time
from uuid import uuid4

from sc_client import client
from sc_client.models import ScIdtfResolveParams


def write(scs):
    if client.generate_elements_by_scs([scs]) != [True]:
        raise RuntimeError("sc-machine не смогла записать учебный пример")


def wait_for(pattern, description):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        # A multi-line SCs template is passed as text to py-sc-client.
        matches = client.search_by_template("\n" + pattern)
        if matches:
            return matches[0]
        time.sleep(0.15)
    raise RuntimeError(f"За 10 секунд в базе не появился результат: {description}")


def run_action(name, action_class, argument):
    write(f"{name} <- {action_class};; {name} -> rrel_1: {argument};;")
    write(f"action_initiated -> {name};;")
    wait_for(f"action_finished_successfully _-> {name};;", action_class)


def show(label, action):
    print(f"{label}\n  http://localhost:8000/?action={action.value}", flush=True)


def address(identifier):
    addr, = client.resolve_keynodes(ScIdtfResolveParams(idtf=identifier, type=None))
    if not addr:
        raise RuntimeError(f"В загруженной базе отсутствует {identifier}. Проверь сборку базы знаний.")
    return addr


def check():
    recipe = "recipe_procedure_preparation_curd_mass"
    for identifier in (recipe, "subject_domain_isa88", "training_low_tank_level_example",
                       "training_stop_pump", "training_connect_reserve_tank"):
        address(identifier)

    prefix = "training_demo_" + uuid4().hex
    procedure, quiz, scenario = (f"{prefix}_{kind}" for kind in ("procedure", "quiz", "scenario"))
    lesson, answer = f"{prefix}_lesson", f"{prefix}_answer"

    run_action(procedure, "action_explain_procedure", recipe)
    steps = " ".join(f"_demo_result _-> rrel_{i};;" for i in range(1, 10))
    wait_for(f"{procedure} _=> nrel_answer:: _demo_result;; _demo_result _-> _demo_description;; "
             f"concept_procedure_execution_description _-> _demo_description;; {steps}", "шаги рецепта 1–9")
    show("№5. В описании рецепта найдены номера шагов с 1-го по 9-й.", address(procedure))

    write(f"{lesson} <- concept_learning_situation; -> rrel_employee: {prefix}_employee; "
          "-> rrel_topic: subject_domain_isa88; -> rrel_question_count: [2]; "
          "-> rrel_emergency_situation: training_low_tank_level_example;;")
    run_action(quiz, "action_generate_quiz", lesson)
    pattern = f"{quiz} _=> nrel_result:: _demo_result;; _demo_result _-> _demo_set;; concept_quiz_question_set _-> _demo_set;;"
    for i in (1, 2):
        pattern += (f"_demo_set _-> rrel_{i}:: _demo_q{i};; _demo_q{i} _-> rrel_reference_answer:: _demo_ref{i};; "
                    f"_demo_q{i} _-> rrel_basis:: _demo_basis{i};;")
    wait_for(pattern, "два вопроса с эталонами и основаниями")
    show("№6. Получены два вопроса по ISA-88 с эталонами и основаниями.", address(quiz))

    run_action(scenario, "action_conduct_scenario_training", lesson)
    write(f"{answer} <- sc_node_structure; -> training_stop_pump;; {lesson} -> rrel_employee_answer: {answer};;")
    write(f"concept_employee_answer_awaiting_check -> {answer};;")
    checked = wait_for(f"action_check_employee_answer _-> _demo_check;; action_finished_successfully _-> _demo_check;; "
                       f"_demo_check _-> rrel_1:: {answer};; {answer} _-> rrel_answer_evaluation:: evaluation_partially_correct;; "
                       f"{answer} _-> rrel_missing_element:: training_connect_reserve_tank;;", "оценка и пропуск")
    show("№7. Ответ «остановить насос» частично верен. Пропущено подключение резервной ёмкости.", checked.get("_demo_check"))

    review = wait_for(f"{scenario} _-> rrel_training_session:: _demo_session;; _demo_session _-> rrel_training_review:: _demo_review;; "
                      f"_demo_review _-> rrel_employee_action:: {answer};; _demo_review _-> rrel_outcome:: outcome_scenario_failed;; "
                      "action_review_scenario_training _-> _demo_action;; action_finished_successfully _-> _demo_action;; "
                      "_demo_action _=> nrel_result:: _demo_result;; _demo_result _-> _demo_review;;", "разбор данного сеанса")
    show("№8. Разбор готов. Итог — «сценарий не пройден».", review.get("_demo_action"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", default="ws://127.0.0.1:8090/ws_json")
    args = parser.parse_args()
    try:
        client.connect(args.server)
        if not client.is_connected():
            raise RuntimeError(f"Нет соединения с {args.server}. Запусти sc-machine.")
        check()
    except RuntimeError as error:
        raise SystemExit(str(error))
    finally:
        client.disconnect()
