#include "graph.hpp"
#include <algorithm>
#include <cmath>

namespace monitoring
{
namespace
{
ScAddr device(Graph const & g, ScAddr channel)
{
  auto physical = g.one(channel, "rrel_mon_device");
  return physical.IsValid() ? physical : channel;
}

struct Limits
{
  ScAddr lowSetpoint, highSetpoint;
  Number low, high;

  bool valid() const
  {
    return low && high && *low < *high;
  }

  int side(double value) const
  {
    return value < *low ? -1 : value > *high ? 1 : 0;
  }
};

Limits limits(Graph const & g, ScAddr c)
{
  auto low = g.one(c, "rrel_mon_low"), high = g.one(c, "rrel_mon_high");
  auto value = [&](ScAddr setpoint)
  {
    if (setpoint.IsValid() && g.ctx.GetElementType(setpoint).IsLink())
      return g.number(setpoint);
    return g.value(setpoint, "rrel_mon_value");
  };
  return {low, high, value(low), value(high)};
}

bool configured(Graph const & g, ScAddr source, char const * role)
{
  return !g.all(source, role).empty();
}
}  // namespace

void validate(Graph const & g, ScAddr snapshot, ScStructure & result)
{
  auto sensors = g.all(snapshot, "rrel_mon_sensor");
  for (auto sensor : sensors)
  {
    auto series = history(g, sensor);
    auto lower = g.value(sensor, "rrel_mon_phys_min"), upper = g.value(sensor, "rrel_mon_phys_max");
    auto addr = device(g, sensor);
    bool complete = series && lower && upper && *lower < *upper;
    ScAddrVector reasons;
    if (complete && (series->back().value < *lower || series->back().value > *upper))
      reasons.push_back(g.key("mon_out_of_range"));
    if (configured(g, sensor, "rrel_mon_stuck_window"))
    {
      auto window = g.value(sensor, "rrel_mon_stuck_window");
      if (!series || !window || *window <= 0 || series->back().time - series->front().time < *window)
        complete = false;
      else
      {
        auto first = series->end() - 1;
        while (first != series->begin() && (first - 1)->value == series->back().value)
          --first;
        if (series->back().time - first->time >= *window)
          reasons.push_back(g.key("mon_stuck"));
      }
    }
    if (configured(g, sensor, "rrel_mon_peer") || configured(g, sensor, "rrel_mon_tolerance"))
    {
      auto peer = g.one(sensor, "rrel_mon_peer");
      auto tolerance = g.value(sensor, "rrel_mon_tolerance");
      auto other = history(g, peer);
      if (!series || !other || peer == sensor || std::find(sensors.begin(), sensors.end(), peer) == sensors.end()
          || !tolerance || *tolerance < 0 || series->back().time != other->back().time)
        complete = false;
      else if (std::abs(series->back().value - other->back().value) > *tolerance)
        reasons.push_back(g.key("mon_peer_disagreement"));
    }
    auto status = reasons.empty() ? (complete ? "mon_reliable" : "mon_unknown_validity") : "mon_unreliable";
    g.field(result, addr, "rrel_mon_validation", g.key(status));
    for (auto reason : reasons)
      g.field(result, addr, "rrel_mon_validation_reason", reason);
  }
}

void thresholds(Graph const & g, ScAddr snapshot, ScStructure & result)
{
  for (auto sensor : g.all(snapshot, "rrel_mon_sensor"))
  {
    auto s = history(g, sensor);
    auto lim = limits(g, sensor);
    auto addr = device(g, sensor);
    if (!s || !lim.valid())
    {
      g.field(result, addr, "rrel_mon_compliance", g.key("mon_unknown"));
      continue;
    }
    int side = lim.side(s->back().value);
    g.field(result, addr, "rrel_mon_compliance", g.key(side < 0 ? "mon_low" : side > 0 ? "mon_high" : "mon_in_range"));
    if (side == 0)
      continue;
    g.field(result, addr, "rrel_mon_violated_limit", side < 0 ? lim.lowSetpoint : lim.highSetpoint);
    auto first = s->end() - 1;
    while (first != s->begin() && lim.side((first - 1)->value) == side)
      --first;
    g.field(result, addr, "rrel_mon_duration", g.numeric(s->back().time - first->time));
    if (first != s->begin())
      g.field(result, addr, "rrel_mon_duration_upper", g.numeric(s->back().time - (first - 1)->time));
  }
}

void predict(Graph const & g, ScAddr snapshot, ScStructure & result)
{
  for (auto sensor : g.all(snapshot, "rrel_mon_sensor"))
  {
    if (!configured(g, sensor, "rrel_mon_sample"))
      continue;
    auto addr = device(g, sensor);
    auto s = history(g, sensor);
    auto lim = limits(g, sensor);
    auto window = g.value(snapshot, "rrel_mon_window"), horizon = g.value(snapshot, "rrel_mon_horizon");
    ScAddr status = g.key("mon_unknown");
    if (s && lim.valid() && window && horizon && *window > 0 && *horizon > 0)
    {
      auto end = s->back();
      auto first = std::lower_bound(
          s->begin(),
          s->end(),
          end.time - *window,
          [](Sample p, double t)
          {
            return p.time < t;
          });
      if (first->time < end.time)
      {
        double rate = (end.value - first->value) / (end.time - first->time);
        if (std::isfinite(rate))
        {
          status = g.key("mon_no_forecast");
          if (rate != 0 && lim.side(end.value) == 0)
          {
            double eta = ((rate > 0 ? *lim.high : *lim.low) - end.value) / rate;
            if (eta >= 0 && eta <= *horizon)
            {
              status = g.key(rate > 0 ? "mon_forecast_high" : "mon_forecast_low");
              g.field(result, addr, "rrel_mon_eta", g.numeric(eta));
              g.field(result, addr, "rrel_mon_rate", g.numeric(rate));
            }
          }
        }
      }
    }
    g.field(result, addr, "rrel_mon_forecast", status);
  }
}

void wear(Graph const & g, ScAddr snapshot, ScStructure & result)
{
  for (auto module : g.all(snapshot, "rrel_mon_module"))
  {
    if (!configured(g, module, "rrel_mon_metric"))
      continue;
    auto addr = device(g, module);
    int worst = 0;
    bool complete = true;
    for (auto metric : g.all(module, "rrel_mon_metric"))
    {
      auto kind = g.one(metric, "rrel_mon_kind");
      auto s = history(g, metric);
      auto warn = g.value(metric, "rrel_mon_warn"), severe = g.value(metric, "rrel_mon_severe");
      bool growth = kind == g.key("mon_current") || kind == g.key("mon_response_time");
      bool cumulative = kind == g.key("mon_hours") || kind == g.key("mon_cycles");
      if (!s || (!growth && !cumulative) || !warn || !severe || *warn <= 0 || *warn > *severe
          || (growth && s->size() < 2))
      {
        complete = false;
        continue;
      }
      double value = s->back().value - (growth ? s->front().value : 0);
      int degree = value >= *severe ? 2 : value >= *warn ? 1 : 0;
      worst = std::max(worst, degree);
      if (degree)
      {
        char const * sign = kind == g.key("mon_current")         ? "mon_current_growth"
                            : kind == g.key("mon_response_time") ? "mon_slow_actuation"
                            : kind == g.key("mon_hours")         ? "mon_exhausted_hours"
                                                                 : "mon_exhausted_cycles";
        g.field(result, addr, "rrel_mon_wear_sign", g.key(sign));
      }
    }
    g.field(
        result,
        addr,
        "rrel_mon_degradation",
        g.key(
            worst == 2   ? "mon_significant"
            : worst == 1 ? "mon_moderate"
            : complete   ? "mon_no_wear"
                         : "mon_unknown"));
  }
}

void context(Graph const & g, ScAddr snapshot, ScStructure & result)
{
  auto object = g.one(snapshot, "rrel_mon_object");
  if (!object.IsValid())
    return;
  auto state = g.one(snapshot, "rrel_mon_state_input");
  for (auto const & roles :
       {std::pair{"rrel_mon_state_input", "rrel_mon_state"},
        {"rrel_mon_procedure_input", "rrel_mon_procedure"},
        {"rrel_mon_phase_input", "rrel_mon_phase"}})
  {
    auto target = g.one(snapshot, roles.first);
    g.field(result, object, roles.second, target.IsValid() ? target : g.key("mon_unknown"));
  }
  for (auto module : g.all(snapshot, "rrel_mon_module"))
    g.field(result, object, "rrel_mon_active_module", device(g, module));
  // Contextual exceptions are facts of the state, scoped to a sensor kind and a threshold direction.
  for (auto sensor : g.all(snapshot, "rrel_mon_sensor"))
  {
    auto s = history(g, sensor);
    auto lim = limits(g, sensor);
    if (!s || !lim.valid() || !state.IsValid())
      continue;
    int side = lim.side(s->back().value);
    if (!side)
      continue;
    for (auto exemption : g.all(state, "rrel_mon_exception"))
    {
      auto kind = g.one(sensor, "rrel_mon_kind");
      if (kind.IsValid() && kind == g.one(exemption, "rrel_mon_kind")
          && g.one(exemption, "rrel_mon_expected") == g.key(side < 0 ? "mon_low" : "mon_high"))
      {
        auto addr = device(g, sensor);
        g.field(result, addr, "rrel_mon_context_status", g.key("mon_expected_in_state"));
        g.field(result, addr, "rrel_mon_justification", state);
        break;
      }
    }
  }
}
}  // namespace monitoring
