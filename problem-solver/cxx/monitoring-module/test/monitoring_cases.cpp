#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_scs_helper.hpp>
#include <sc-memory/test/dummy_file_interface.hpp>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace
{
void require(bool condition, std::string const & message)
{
  if (!condition)
    throw std::runtime_error(message);
}

class Cases
{
public:
  ScAgentContext ctx;

  ScAddr key(std::string const & id)
  {
    auto addr = ctx.SearchElementBySystemIdentifier(id);
    require(addr.IsValid(), "missing " + id);
    return addr;
  }

  ScStructure run(std::string const & command, ScAddr input, ScAddr option = {}, bool success = true)
  {
    auto action = ctx.GenerateAction(key(command));
    if (input.IsValid())
      action.SetArgument(1, input);
    if (option.IsValid())
      action.SetArgument(2, option);
    require(action.InitiateAndWait(5000), command + " timed out");
    require(success ? action.IsFinishedSuccessfully() : action.IsFinishedUnsuccessfully(), command + " wrong status");
    return action.GetResult();
  }

  ScStructure analyze(std::string id, std::string method)
  {
    return run("action_mon_assess_object", key("mon_case_" + id), key("mon_" + method + "_method"));
  }

  // Require every element of the five-element statement in this specific action result.
  ScAddrVector values(ScStructure & result, std::string const & object, std::string const & role)
  {
    auto it = ctx.CreateIterator5(
        key(object), ScType::ConstPermPosArc, ScType::Unknown, ScType::ConstPermPosArc, key("rrel_mon_" + role));
    ScAddrVector values;
    while (it->Next())
    {
      if (!result.Has(it->Get(1)))
        continue;
      for (unsigned i = 0; i < 5; ++i)
        require(result.Has(it->Get(i)), "incomplete " + role);
      values.push_back(it->Get(2));
    }
    return values;
  }

  void expect(ScStructure & r, std::string obj, std::string role, std::string expected)
  {
    auto v = values(r, obj, role);
    require(std::find(v.begin(), v.end(), key(expected)) != v.end(), obj + " " + role + " expected " + expected);
  }

  void number(ScStructure & r, std::string obj, std::string role, double expected)
  {
    auto v = values(r, obj, role);
    std::string text;
    require(v.size() == 1 && ctx.GetLinkContent(v.front(), text) && std::abs(std::stod(text) - expected) < 1e-8, role);
  }

  void assessment()
  {
    for (auto const & [id, reason] :
         {std::pair{"stuck", "mon_stuck"}, {"invalid", "mon_out_of_range"}, {"pair", "mon_peer_disagreement"}})
    {
      auto r = analyze(id, "validation");
      expect(r, "mon_device_level", "validation", "mon_unreliable");
      expect(r, "mon_device_level", "validation_reason", reason);
    }
    auto good = analyze("low", "validation");
    expect(good, "mon_device_level", "validation", "mon_reliable");
    auto missing = analyze("incomplete", "validation");
    expect(missing, "mon_device_level", "validation", "mon_unknown_validity");
    auto low = analyze("low", "threshold");
    expect(low, "mon_device_level", "compliance", "mon_low");
    number(low, "mon_device_level", "duration", 5);
    expect(low, "mon_device_level", "violated_limit", "mon_limit_level_low");
    auto high = analyze("high", "threshold");
    expect(high, "mon_device_level", "compliance", "mon_high");
    auto normal = analyze("normal", "threshold");
    expect(normal, "mon_device_level", "compliance", "mon_in_range");
    require(values(normal, "mon_device_level", "duration").empty(), "normal reading has a violation duration");
    auto unknown = analyze("incomplete", "threshold");
    expect(unknown, "mon_device_level", "compliance", "mon_unknown");
    auto prediction = analyze("forecast", "prediction");
    expect(prediction, "mon_device_temp", "forecast", "mon_forecast_high");
    number(prediction, "mon_device_temp", "eta", 12);
    number(prediction, "mon_device_temp", "rate", 0.5);
    auto down = analyze("cooling", "prediction");
    expect(down, "mon_device_temp", "forecast", "mon_forecast_low");
    number(down, "mon_device_temp", "eta", 10);
    number(down, "mon_device_temp", "rate", -0.5);
    auto beyond = analyze("short_horizon", "prediction");
    expect(beyond, "mon_device_temp", "forecast", "mon_no_forecast");
    require(values(beyond, "mon_device_temp", "eta").empty(), "unexpected ETA beyond horizon");
    auto worn = analyze("pump", "wear");
    expect(worn, "mon_device_pump", "degradation", "mon_significant");
    for (auto sign : {"mon_current_growth", "mon_slow_actuation", "mon_exhausted_hours", "mon_exhausted_cycles"})
      expect(worn, "mon_device_pump", "wear_sign", sign);
    auto healthy = analyze("healthy", "wear");
    expect(healthy, "mon_device_pump", "degradation", "mon_no_wear");
    auto idle = analyze("idle", "context");
    expect(idle, "mon_site_tank", "state", "mon_idle");
    expect(idle, "mon_device_level", "context_status", "mon_expected_in_state");
    expect(idle, "mon_device_level", "justification", "mon_idle");
    expect(idle, "mon_site_tank", "procedure", "training_curd_procedure");
    expect(idle, "mon_site_tank", "phase", "training_curd_filling");
    auto operating = analyze("pump", "context");
    expect(operating, "mon_site_pump", "active_module", "mon_device_pump");
    auto none = run("action_mon_assess_object", key("mon_case_low"), key("mon_wear_method"), false);
    require(none.Has(key("mon_insufficient")), "no relevant data");
  }

  void classification()
  {
    for (auto const & [id, diagnosis] :
         {std::pair{"low", "low_level"},
          {"high", "overflow"},
          {"temperature", "thermal"},
          {"valve", "valve"},
          {"pump", "pump"},
          {"route", "restricted"},
          {"stuck", "bad_sensor"},
          {"pair", "bad_sensor"},
          {"invalid", "bad_sensor"},
          {"timeout", "timeout"},
          {"wash", "wash"},
          {"idle", "normal"},
          {"healthy", "normal"},
          {"normal", "normal"},
          {"brief", "normal"},
          {"acknowledged", "normal"},
          {"forecast", "normal"}})
    {
      auto input = key(std::string("mon_case_") + id);
      auto r = run("action_mon_analyze_situation", input);
      bool abnormal = std::string(diagnosis) != "normal";
      auto cls = key(abnormal ? std::string("mon_fault_") + diagnosis : "mon_normal_situation");
      auto it = ctx.CreateIterator3(cls, ScType::ConstPermPosArc, ScType::ConstNode);
      ScAddr instance;
      while (it->Next())
        if (r.Has(it->Get(1)) && r.Has(cls) && r.Has(it->Get(2)))
          instance = it->Get(2);
      require(instance.IsValid(), std::string(id) + " wrong classification: " + diagnosis);
      auto source = ctx.CreateIterator5(
          instance, ScType::ConstPermPosArc, input, ScType::ConstPermPosArc, key("rrel_mon_source_observation"));
      require(source->Next() && r.Has(source->Get(1)), "wrong source observation");
      require(r.Has(key("emergency")) == abnormal, "wrong general class");
      require(
          ctx.CheckConnector(key("emergency"), instance, ScType::ConstPermPosArc) == abnormal,
          "missing emergency publication");
      for (auto role : {"rrel_mon_location", "rrel_mon_likely_cause", "rrel_mon_criticality"})
      {
        if (!abnormal && std::string(role) != "rrel_mon_location")
          continue;
        auto f =
            ctx.CreateIterator5(instance, ScType::ConstPermPosArc, ScType::Unknown, ScType::ConstPermPosArc, key(role));
        require(f->Next(), role);
        for (unsigned j = 0; j < 5; ++j)
          require(r.Has(f->Get(j)), role);
      }
      std::cout << "PASS " << id << " -> " << diagnosis << '\n';
    }
    // Two snapshots share a physical sensor; evidence must stay scoped to each request.
    auto job = [](std::string id) -> ScAddr
    {
      Cases independent;
      auto input = independent.key("mon_case_" + id);
      return independent.run("action_mon_assess_object", input);
    };
    auto first = std::async(std::launch::async, job, "low");
    auto second = std::async(std::launch::async, job, "idle");
    ScAddr running = first.get(), idle = second.get();
    auto runningResult = run("action_mon_classify_object", key("mon_case_low"), running);
    auto idleResult = run("action_mon_classify_object", key("mon_case_idle"), idle);
    require(
        runningResult.Has(key("mon_fault_low_level")) && idleResult.Has(key("mon_normal_situation")),
        "snapshot isolation");
    auto r = run("action_mon_analyze_situation", key("mon_case_incomplete"), {}, false);
    require(
        r.Has(key("mon_insufficient")) && !r.Has(key("emergency")) && !r.Has(key("mon_normal_situation")),
        "unknown classified");
  }
};
}  // namespace

int main(int argc, char ** argv)
{
  if (argc != 3)
    return 2;
  std::string scratch = (fs::temp_directory_path() / "ostis-monitoring-XXXXXX").string();
  if (!mkdtemp(scratch.data()))
    return 2;
  fs::path root = scratch;
  std::string ext = (root / "extensions").string(), storage = (root / "kb").string();
  fs::create_directory(ext);
  fs::create_symlink(fs::absolute(argv[2]), fs::path(ext) / "libmonitoring-module.so");
  char const * dirs[] = {ext.c_str()};
  sc_memory_params p;
  sc_memory_params_clear(&p);
  p.storage = storage.c_str();
  p.extensions_directories = dirs;
  p.extensions_directories_count = 1;
  p.dump_memory = false;
  p.dump_memory_statistics = false;
  p.log_level = "Error";
  if (!ScMemory::Initialize(p))
    return 2;
  int status = 0;
  try
  {
    Cases test;
    SCsHelper loader(test.ctx, std::make_shared<DummyFileInterface>());
    for (auto name :
         {"monitoring_concepts.scs", "classification_rules.scs", "monitoring_cases.scs", "monitoring_menu.scs"})
    {
      std::ifstream file(fs::path(argv[1]) / name);
      std::string source((std::istreambuf_iterator<char>(file)), {});
      require(!source.empty() && loader.GenerateBySCsText(source), name);
    }
    test.assessment();
    test.classification();
  }
  catch (std::exception const & e)
  {
    std::cerr << "FAIL " << e.what() << '\n';
    status = 1;
  }
  ScMemory::Shutdown(false);
  fs::remove_all(root);
  return status;
}
