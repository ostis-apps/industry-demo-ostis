#include "graph.hpp"
#include <algorithm>
#include <limits>

namespace monitoring
{
namespace
{
enum class Match
{
  No,
  Unknown,
  Yes
};

Match condition(Graph const & g, ScAddr snapshot, ScAddr graph, ScAddr term)
{
  auto kind = g.one(term, "rrel_mon_kind"), role = g.one(term, "rrel_mon_attribute");
  auto expected = g.one(term, "rrel_mon_expected");
  if (!kind.IsValid() || !role.IsValid() || !expected.IsValid())
    return Match::Unknown;
  bool found = false, incomplete = false;
  auto channels = g.all(snapshot, "rrel_mon_sensor");
  bool anySensor = kind == g.key("mon_any_sensor");
  if (!anySensor)
  {
    auto modules = g.all(snapshot, "rrel_mon_module");
    channels.insert(channels.end(), modules.begin(), modules.end());
  }
  for (auto channel : channels)
  {
    if (!anySensor && g.one(channel, "rrel_mon_kind") != kind)
      continue;
    found = true;
    auto source = g.one(channel, "rrel_mon_device");
    if (!source.IsValid())
      source = channel;
    auto values = g.evidence(graph, source, role);
    if (values.size() != 1 || values[0] == g.key("mon_unknown") || values[0] == g.key("mon_unknown_validity"))
    {
      incomplete = true;
      continue;
    }
    if (role == g.key("rrel_mon_compliance"))
    {
      auto valid = g.evidence(graph, source, g.key("rrel_mon_validation"));
      if (valid.size() != 1 || valid.front() != g.key("mon_reliable"))
      {
        incomplete = true;
        continue;
      }
    }
    if (values.front() != expected)
      continue;
    if (!g.all(term, "rrel_mon_min_duration").empty())
    {
      auto d = g.evidence(graph, source, g.key("rrel_mon_duration"));
      auto duration = d.size() == 1 ? g.number(d.front()) : Number{};
      auto minimum = g.value(term, "rrel_mon_min_duration");
      if (!duration || !minimum || *minimum < 0)
      {
        incomplete = true;
        continue;
      }
      if (*duration < *minimum)
      {
        auto upper = g.evidence(graph, source, g.key("rrel_mon_duration_upper"));
        auto bound = upper.size() == 1 ? g.number(upper.front()) : Number{};
        if (!bound || *bound > *minimum)
          incomplete = true;
        continue;
      }
    }
    if (g.ctx.CheckConnector(g.key("mon_context_sensitive"), term, ScType::ConstPermPosArc))
    {
      auto object = g.one(snapshot, "rrel_mon_object");
      auto states = g.evidence(graph, object, g.key("rrel_mon_state"));
      if (states.size() != 1 || states.front() == g.key("mon_unknown"))
      {
        incomplete = true;
        continue;
      }
      auto allowed = g.evidence(graph, source, g.key("rrel_mon_context_status"));
      if (std::find(allowed.begin(), allowed.end(), g.key("mon_expected_in_state")) != allowed.end())
        continue;
    }
    return Match::Yes;
  }
  return found && !incomplete ? Match::No : Match::Unknown;
}
}  // namespace

bool classify(Graph const & g, ScAddr snapshot, ScAddr graph, ScStructure & result)
{
  auto location = g.one(snapshot, "rrel_mon_object");
  auto profile = g.one(snapshot, "rrel_mon_profile");
  auto rules = g.all(profile, "rrel_mon_rule");
  ScAddr type, severity, cause;
  double best = -std::numeric_limits<double>::infinity();
  bool incomplete = rules.empty(), conflict = false;
  for (auto rule : rules)
  {
    auto terms = g.all(rule, "rrel_mon_condition");
    Match match = terms.empty() ? Match::Unknown : Match::Yes;
    for (auto term : terms)
    {
      Match part = condition(g, snapshot, graph, term);
      if (part == Match::No)
      {
        match = Match::No;
        break;
      }
      if (part == Match::Unknown)
        match = part;
    }
    if (match == Match::Unknown)
      incomplete = true;
    if (match != Match::Yes)
      continue;
    auto rank = g.value(rule, "rrel_mon_priority");
    auto cls = g.one(rule, "rrel_mon_class"), level = g.one(rule, "rrel_mon_severity"),
         why = g.one(rule, "rrel_mon_cause");
    if (!rank || !cls.IsValid() || !level.IsValid() || !why.IsValid())
    {
      incomplete = true;
      continue;
    }
    if (*rank > best)
    {
      best = *rank;
      type = cls;
      severity = level;
      cause = why;
      conflict = false;
    }
    else if (*rank == best && (type != cls || severity != level || why != cause))
      conflict = true;
  }

  auto occurrence = g.ctx.GenerateNode(ScType::ConstNode);
  result.Append(occurrence, ScKeynodes::rrel_key_sc_element);
  g.field(result, occurrence, "rrel_mon_source_observation", snapshot);
  if (location.IsValid())
    g.field(result, occurrence, "rrel_mon_location", location);
  if (!location.IsValid() || conflict || (!type.IsValid() && incomplete))
  {
    g.field(result, occurrence, "rrel_mon_failure_reason", g.key("mon_insufficient"));
    return false;
  }
  if (!type.IsValid())
  {
    g.member(result, g.key("mon_normal_situation"), occurrence);
    return true;
  }
  g.member(result, type, occurrence);
  g.field(result, occurrence, "rrel_mon_criticality", severity);
  g.field(result, occurrence, "rrel_mon_likely_cause", cause);
  // Publish the general class last: an observer can already read all details of this occurrence.
  g.member(result, g.key("emergency"), occurrence);
  return true;
}
}  // namespace monitoring
