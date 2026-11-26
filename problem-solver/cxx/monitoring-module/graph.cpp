#include "graph.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace monitoring
{
ScAddr Graph::key(std::string const & id) const
{
  auto addr = ctx.SearchElementBySystemIdentifier(id);
  if (!addr.IsValid())
    SC_THROW_EXCEPTION(utils::ExceptionItemNotFound, "Missing monitoring ontology element: " << id);
  return addr;
}

ScAddrVector Graph::all(ScAddr source, std::string const & role) const
{
  ScAddrVector values;
  if (!source.IsValid())
    return values;
  auto it = ctx.CreateIterator5(source, ScType::ConstPermPosArc, ScType::Unknown, ScType::ConstPermPosArc, key(role));
  while (it->Next())
    if (std::find(values.begin(), values.end(), it->Get(2)) == values.end())
      values.push_back(it->Get(2));
  return values;
}

ScAddr Graph::one(ScAddr source, std::string const & role) const
{
  auto values = all(source, role);
  return values.size() == 1 ? values.front() : ScAddr::Empty;
}

Number Graph::number(ScAddr link) const
{
  std::string text;
  if (!link.IsValid() || !ctx.GetElementType(link).IsLink() || !ctx.GetLinkContent(link, text))
    return {};
  std::replace(text.begin(), text.end(), ',', '.');
  double value;
  auto end = text.data() + text.size();
  auto parsed = std::from_chars(text.data(), end, value);
  if (parsed.ptr == end && parsed.ec == std::errc{} && std::isfinite(value))
    return value;
  return {};
}

Number Graph::value(ScAddr source, std::string const & role) const
{
  return number(one(source, role));
}

ScAddr Graph::numeric(double value) const
{
  char buffer[64];
  auto end = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 12);
  auto link = ctx.GenerateLink();
  ctx.SetLinkContent(link, std::string(buffer, end.ptr));
  return link;
}

void Graph::field(ScStructure & graph, ScAddr source, std::string const & role, ScAddr value) const
{
  auto label = key(role);
  auto arc = ctx.GenerateConnector(ScType::ConstPermPosArc, source, value);
  auto attr = ctx.GenerateConnector(ScType::ConstPermPosArc, label, arc);
  graph << source << arc << value << label << attr;
}

void Graph::member(ScStructure & graph, ScAddr cls, ScAddr element) const
{
  auto arc = ctx.GenerateConnector(ScType::ConstPermPosArc, cls, element);
  graph << cls << arc << element;
}

void Graph::merge(ScStructure & target, ScAddr source) const
{
  auto it = ctx.CreateIterator3(source, ScType::ConstPermPosArc, ScType::Unknown);
  while (it->Next())
    target << it->Get(2);
}

ScAddrVector Graph::evidence(ScAddr graph, ScAddr source, ScAddr role) const
{
  ScAddrVector values;
  if (!source.IsValid() || !graph.IsValid())
    return values;
  auto it = ctx.CreateIterator5(source, ScType::ConstPermPosArc, ScType::Unknown, ScType::ConstPermPosArc, role);
  while (it->Next())
    if (ctx.CheckConnector(graph, it->Get(1), ScType::ConstPermPosArc)
        && ctx.CheckConnector(graph, it->Get(3), ScType::ConstPermPosArc))
      values.push_back(it->Get(2));
  return values;
}

std::optional<Series> history(Graph const & g, ScAddr sensor)
{
  Series series;
  for (auto sample : g.all(sensor, "rrel_mon_sample"))
  {
    auto t = g.value(sample, "rrel_mon_time"), v = g.value(sample, "rrel_mon_value");
    if (!t || !v)
      return {};
    series.push_back({*t, *v});
  }
  if (series.empty())
    return {};
  std::sort(
      series.begin(),
      series.end(),
      [](Sample a, Sample b)
      {
        return a.time < b.time;
      });
  for (size_t i = 1; i < series.size(); ++i)
    if (series[i - 1].time == series[i].time)
      return {};
  return series;
}
}  // namespace monitoring
