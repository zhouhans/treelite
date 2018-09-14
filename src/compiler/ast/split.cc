/*!
 * Copyright 2017 by Contributors
 * \file split.cc
 * \brief Split prediction subroutine into multiple translation units (files)
 */
#include "./builder.h"

namespace treelite {
namespace compiler {

DMLC_REGISTRY_FILE_TAG(split);

int count_tu_nodes(ASTNode* node) {
  int accum = (dynamic_cast<TranslationUnitNode*>(node)) ? 1 : 0;
  for (ASTNode* child : node->children) {
    accum += count_tu_nodes(child);
  }
  return accum;
}

void ASTBuilder::Split(int parallel_comp) {
  if (parallel_comp <= 0) {
    LOG(INFO) << "Parallel compilation disabled; all member trees will be "
              << "dumped to a single source file. This may increase "
              << "compilation time and memory usage.";
    return;
  }
  LOG(INFO) << "Parallel compilation enabled; member trees will be "
            << "divided into " << parallel_comp << " translation units.";
  CHECK_EQ(this->main_node->children.size(), 1);
  ASTNode* top_ac_node = this->main_node->children[0];
  CHECK(dynamic_cast<AccumulatorContextNode*>(top_ac_node));

  /* tree_head[i] stores reference to head of tree i */
  std::vector<ASTNode*> tree_head;
  for (ASTNode* node : top_ac_node->children) {
    CHECK(dynamic_cast<ConditionNode*>(node) || dynamic_cast<OutputNode*>(node));
    tree_head.push_back(node);
  }
  /* dynamic_cast<> is used here to check node types. This is to ensure
     that we don't accidentally call Split() twice. */

  const int ntree = static_cast<int>(tree_head.size());
  const int nunit = parallel_comp;
  const int unit_size = (ntree + nunit - 1) / nunit;
  std::vector<ASTNode*> tu_list;  // list of translation units
  const int current_num_tu = count_tu_nodes(this->main_node);
  for (int unit_id = 0; unit_id < nunit; ++unit_id) {
    const int tree_begin = unit_id * unit_size;
    const int tree_end = std::min((unit_id + 1) * unit_size, ntree);
    if (tree_begin < tree_end) {
      TranslationUnitNode* tu
        = AddNode<TranslationUnitNode>(top_ac_node, current_num_tu + unit_id);
      tu_list.push_back(tu);
      AccumulatorContextNode* ac = AddNode<AccumulatorContextNode>(tu);
      tu->children.push_back(ac);
      for (int tree_id = tree_begin; tree_id < tree_end; ++tree_id) {
        ASTNode* tree_head_node = tree_head[tree_id];
        tree_head_node->parent = ac;
        ac->children.push_back(tree_head_node);
      }
    }
  }
  top_ac_node->children = tu_list;
}

}  // namespace compiler
}  // namespace treelite
