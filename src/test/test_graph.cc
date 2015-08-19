#include "gtest/gtest.h"
#include "utils/graph.h"

using namespace singa;

TEST(GraphTest, DAG) {
  Graph g;
  g.AddNode("a");
  g.AddNode("b");
  g.AddNode("c");
  g.AddNode("d1");
  g.AddNode("d2");
  g.AddNode("e");
  g.AddNode("f");
  g.AddEdge("d1", "d2");
  g.AddEdge("d2", "d1");
  g.AddEdge("a", "b");
  g.AddEdge("b", "c");
  g.AddEdge("c", "d1");
  g.AddEdge("e", "d2");
  g.AddEdge("d1", "f");
  g.AddEdge("d2", "f");
  g.Sort();
}
