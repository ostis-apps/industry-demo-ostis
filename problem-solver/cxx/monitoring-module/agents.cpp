#include "graph.hpp"
#include <sc-memory/sc_module.hpp>
#include <mutex>

using namespace monitoring;

namespace
{
ScKeynode const assess{"action_mon_assess_object", ScType::ConstNodeClass};
ScKeynode const classifyAction{"action_mon_classify_object", ScType::ConstNodeClass};
ScKeynode const prepare{"action_mon_analyze_situation", ScType::ConstNodeClass};
ScKeynode const continuation{"action_mon_collect_result", ScType::ConstNodeClass};
constexpr char const * methods[] =
    {"mon_validation_method", "mon_threshold_method", "mon_prediction_method", "mon_wear_method", "mon_context_method"};
constexpr Analysis analyses[] = {validate, thresholds, predict, wear, context};
std::mutex publishMutex;

ScResult insufficient(Graph const & g, ScAction & action)
{
  auto result = g.ctx.GenerateStructure();
  g.field(result, action, "rrel_mon_failure_reason", g.key("mon_insufficient"));
  action.SetResult(result);
  return action.FinishUnsuccessfully();
}

void attach(Graph const & g, ScAddr src, std::string const & role, ScAddr value)
{
  auto arc = g.ctx.GenerateConnector(ScType::ConstPermPosArc, src, value);
  g.ctx.GenerateConnector(ScType::ConstPermPosArc, g.key(role), arc);
}
}  // namespace

// All five agents subscribe to the same initiated action. Their partial results and completion
// markers belong to that action alone. Only the last contributor publishes the aggregate.
template <size_t Index>
class ObjectAssessmentAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override
  {
    return assess;
  }

  ScResult DoProgram(ScAction & action) override
  {
    Graph g(m_context);
    auto [snapshot, filter] = action.GetArguments<2>();
    auto method = g.key(methods[Index]);
    if (filter.IsValid() && filter != method)
    {
      if constexpr (Index == 0)
      {
        bool known = false;
        for (auto id : methods)
          if (filter == g.key(id))
            known = true;
        if (!known)
          return insufficient(g, action);
      }
      return ScActionInitiatedAgent::DoProgram(action);
    }
    auto part = m_context.GenerateStructure();
    if (snapshot.IsValid())
      analyses[Index](g, snapshot, part);

    std::lock_guard<std::mutex> lock(publishMutex);
    if (action.IsFinished())
      return ScActionInitiatedAgent::DoProgram(action);
    if (filter.IsValid())
    {
      if (part.IsEmpty())
        return insufficient(g, action);
      action.SetResult(part);
      return action.FinishSuccessfully();
    }
    auto parts = g.one(action, "rrel_mon_parts");
    if (!parts.IsValid())
    {
      parts = m_context.GenerateNode(ScType::ConstNode);
      attach(g, action, "rrel_mon_parts", parts);
    }
    if (g.one(parts, methods[Index]).IsValid())
      return ScActionInitiatedAgent::DoProgram(action);
    auto membership = m_context.GenerateConnector(ScType::ConstPermPosArc, parts, part);
    m_context.GenerateConnector(ScType::ConstPermPosArc, method, membership);
    auto components = m_context.CreateIterator3(parts, ScType::ConstPermPosArc, ScType::ConstNodeStructure);
    ScAddrVector complete;
    while (components->Next())
      complete.push_back(components->Get(2));
    if (complete.size() < std::size(methods))
      return ScActionInitiatedAgent::DoProgram(action);
    auto result = m_context.GenerateStructure();
    for (auto component : complete)
      g.merge(result, component);
    if (result.IsEmpty())
      return insufficient(g, action);
    action.SetResult(result);
    return action.FinishSuccessfully();
  }
};

class SensorValidationAgent : public ObjectAssessmentAgent<0>
{
};

class ThresholdCheckAgent : public ObjectAssessmentAgent<1>
{
};

class TrendPredictionAgent : public ObjectAssessmentAgent<2>
{
};

class WearAnalysisAgent : public ObjectAssessmentAgent<3>
{
};

class ProcessContextAnalysisAgent : public ObjectAssessmentAgent<4>
{
};

class SituationClassificationAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override
  {
    return classifyAction;
  }

  ScResult DoProgram(ScAction & action) override
  {
    Graph g(m_context);
    auto [snapshot, evidence] = action.GetArguments<2>();
    if (!snapshot.IsValid())
      return insufficient(g, action);
    if (!evidence.IsValid())
      evidence = g.one(snapshot, "rrel_mon_evidence");
    if (!evidence.IsValid())
      return insufficient(g, action);
    if (m_context.GetElementType(evidence) != ScType::ConstNodeStructure)
    {
      if (!m_context.CheckConnector(ScKeynodes::action_finished_successfully, evidence, ScType::ConstPermPosArc))
        return insufficient(g, action);
      auto completed = m_context.ConvertToAction(evidence);
      if (completed.GetArgument(1) != snapshot)
        return insufficient(g, action);
      evidence = completed.GetResult();
    }
    auto result = m_context.GenerateStructure();
    bool classified = classify(g, snapshot, evidence, result);
    action.SetResult(result);
    return classified ? action.FinishSuccessfully() : action.FinishUnsuccessfully();
  }
};

// The one-click UI command posts two actions in memory. The classifier also accepts a previously
// completed assessment directly. No worker thread blocks waiting for another agent.
class AnalyzeSituationCommand : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override
  {
    return prepare;
  }

  ScResult DoProgram(ScAction & action) override
  {
    Graph g(m_context);
    auto snapshot = action.GetArgument(1);
    if (!snapshot.IsValid())
      return insufficient(g, action);
    auto child = m_context.GenerateAction(assess);
    child.SetArgument(1, snapshot);
    attach(g, child, "rrel_mon_parent", action);
    child.Initiate();
    return ScActionInitiatedAgent::DoProgram(action);
  }
};

using FinishedEvent = ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc>;

class CollectAnalysisCommand : public ScAgent<FinishedEvent>
{
public:
  ScAddr GetActionClass() const override
  {
    return continuation;
  }

  ScAddr GetEventSubscriptionElement() const override
  {
    return ScKeynodes::action_finished;
  }

  bool CheckInitiationCondition(FinishedEvent const & event) override
  {
    auto child = event.GetOtherElement();
    if (!m_context.CheckConnector(assess, child, ScType::ConstPermPosArc)
        && !m_context.CheckConnector(classifyAction, child, ScType::ConstPermPosArc))
      return false;
    return Graph(m_context).one(child, "rrel_mon_parent").IsValid();
  }

  ScResult DoProgram(FinishedEvent const & event, ScAction & action) override
  {
    Graph g(m_context);
    auto child = m_context.ConvertToAction(event.GetOtherElement());
    auto parent = m_context.ConvertToAction(g.one(child, "rrel_mon_parent"));
    if (child.IsFinishedSuccessfully() && m_context.CheckConnector(assess, child, ScType::ConstPermPosArc))
    {
      auto next = m_context.GenerateAction(classifyAction);
      next.SetArgument(1, child.GetArgument(1));
      next.SetArgument(2, child);
      attach(g, next, "rrel_mon_parent", parent);
      next.Initiate();
    }
    else
    {
      parent.SetResult(child.GetResult());
      if (child.IsFinishedSuccessfully())
        parent.FinishSuccessfully();
      else
        parent.FinishUnsuccessfully();
    }
    return action.FinishSuccessfully();
  }
};

class MonitoringModule : public ScModule
{
};

SC_MODULE_REGISTER(MonitoringModule)
    ->Agent<SensorValidationAgent>()
    ->Agent<ThresholdCheckAgent>()
    ->Agent<TrendPredictionAgent>()
    ->Agent<WearAnalysisAgent>()
    ->Agent<ProcessContextAnalysisAgent>()
    ->Agent<SituationClassificationAgent>()
    ->Agent<AnalyzeSituationCommand>()
    ->Agent<CollectAnalysisCommand>();
