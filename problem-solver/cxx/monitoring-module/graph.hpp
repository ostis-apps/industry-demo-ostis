#pragma once

#include <sc-memory/sc_agent.hpp>
#include <optional>
#include <vector>

namespace monitoring
{
using Number = std::optional<double>;

class Graph
{
public:
  explicit Graph(ScAgentContext & context)
    : ctx(context)
  {
  }

  ScAddr key(std::string const & id) const;
  ScAddrVector all(ScAddr source, std::string const & role) const;
  ScAddr one(ScAddr source, std::string const & role) const;
  Number number(ScAddr link) const;
  Number value(ScAddr source, std::string const & role) const;
  ScAddr numeric(double value) const;
  void field(ScStructure & graph, ScAddr source, std::string const & role, ScAddr value) const;
  void member(ScStructure & graph, ScAddr cls, ScAddr element) const;
  void merge(ScStructure & target, ScAddr source) const;
  ScAddrVector evidence(ScAddr graph, ScAddr source, ScAddr role) const;
  ScAgentContext & ctx;
};

struct Sample
{
  double time;
  double value;
};

using Series = std::vector<Sample>;
std::optional<Series> history(Graph const & g, ScAddr sensor);
using Analysis = void (*)(Graph const &, ScAddr, ScStructure &);
void validate(Graph const &, ScAddr, ScStructure &);
void thresholds(Graph const &, ScAddr, ScStructure &);
void predict(Graph const &, ScAddr, ScStructure &);
void wear(Graph const &, ScAddr, ScStructure &);
void context(Graph const &, ScAddr, ScStructure &);
bool classify(Graph const &, ScAddr snapshot, ScAddr evidence, ScStructure & result);
}  // namespace monitoring
