//--------------------------------------------------------------------------------------------------
// _logic_item_remove_and_reinsert
//--------------------------------------------------------------------------------------------------
void _logic_item_remove_and_reinsert(uint32_t p_ref_id) {
	ItemRef &ref = _refs[p_ref_id];

	// Early exit conditions combined for clarity
	if (!ref.is_active() || ref.item_id == BVHCommon::INVALID) {
		return;
	}

	// Validate preconditions
	BVH_ASSERT(ref.tnode_id != BVHCommon::INVALID);

	// Simplified tree ID retrieval
	BVHHandle temp_handle;
	temp_handle.set_id(p_ref_id);
	const uint32_t tree_id = _handle_get_tree_id(temp_handle);

	// Remove item and capture its AABB
	BVHABB_CLASS abb;
	node_remove_item(p_ref_id, tree_id, &abb);

	// Reinsert using optimized node selection
	ref.tnode_id = _logic_choose_item_add_node(_root_node_id[tree_id], abb);
	_node_add_item(ref.tnode_id, p_ref_id, abb);

	// Propagate changes upward
	refit_upward_and_balance(ref.tnode_id, tree_id);
}

//--------------------------------------------------------------------------------------------------
// _logic_abb_merge
//--------------------------------------------------------------------------------------------------
// Creates a new AABB encompassing both input volumes
BVHABB_CLASS _logic_abb_merge(const BVHABB_CLASS &a, const BVHABB_CLASS &b) {
	BVHABB_CLASS merged = a;
	merged.merge(b);
	return merged;
}

//--------------------------------------------------------------------------------------------------
// _logic_balance
//--------------------------------------------------------------------------------------------------
int32_t _logic_balance(int32_t iA, uint32_t p_tree_id) {
	TNode *A = &_nodes[iA];

	// Early exit for leaves and shallow nodes
	if (A->is_leaf() || A->height < 2) {
		return iA;
	}

	// Structural validation
	CRASH_COND(A->num_children != 2);

	const int32_t iB = A->children[0];
	const int32_t iC = A->children[1];
	TNode *B = &_nodes[iB];
	TNode *C = &_nodes[iC];

	const int32_t balance = C->height - B->height;

	// Right-heavy (C needs promotion)
	if (balance > 1) {
		return _balance_rotate(iA, iB, iC, C->children[0], C->children[1], p_tree_id, false);
	}
	// Left-heavy (B needs promotion)
	else if (balance < -1) {
		return _balance_rotate(iA, iC, iB, B->children[0], B->children[1], p_tree_id, true);
	}

	return iA;
}

// Helper function for rotation logic
int32_t _balance_rotate(int32_t iA, int32_t iLightChild, int32_t iHeavyChild,
						int32_t iHChild0, int32_t iHChild1, uint32_t tree_id, bool is_left_rotation) {
	TNode *A = &_nodes[iA];
	TNode *Heavy = &_nodes[iHeavyChild];
	TNode *HChild0 = &_nodes[iHChild0];
	TNode *HChild1 = &_nodes[iHChild1];

	// Update grandparent linkage
	if (A->parent_id != BVHCommon::INVALID) {
		TNode &grandparent = _nodes[A->parent_id];
		const int child_idx = (grandparent.children[0] == iA) ? 0 : 1;
		grandparent.children[child_idx] = iHeavyChild;
	} else {
		change_root_node(iHeavyChild, tree_id);
	}

	// Reconfigure parent-child relationships
	Heavy->parent_id = A->parent_id;
	Heavy->children[!is_left_rotation] = iA;
	A->parent_id = iHeavyChild;

	// Select optimal subtree configuration
	const bool keep_first_child = (HChild0->height > HChild1->height) ^ is_left_rotation;
	const int32_t iKeep = keep_first_child ? iHChild0 : iHChild1;
	const int32_t iMove = keep_first_child ? iHChild1 : iHChild0;

	A->children[is_left_rotation] = iMove;
	Heavy->children[is_left_rotation] = iKeep;
	_nodes[iMove].parent_id = iA;

	// Update AABBs and heights
	A->aabb = _logic_abb_merge(_nodes[iLightChild].aabb, _nodes[iMove].aabb);
	Heavy->aabb = _logic_abb_merge(A->aabb, _nodes[iKeep].aabb);

	A->height = 1 + MAX(_nodes[iLightChild].height, _nodes[iMove].height);
	Heavy->height = 1 + MAX(A->height, _nodes[iKeep].height);

	return iHeavyChild;
						}

						//--------------------------------------------------------------------------------------------------
						// _logic_choose_item_add_node
						//--------------------------------------------------------------------------------------------------
						uint32_t _logic_choose_item_add_node(uint32_t p_node_id, const BVHABB_CLASS &p_aabb) {
							while (true) {
								TNode &tnode = _nodes[p_node_id];
								BVH_ASSERT(tnode.num_children <= 2);

								if (tnode.is_leaf()) {
									if (!node_is_leaf_full(tnode)) {
										return p_node_id;
									}
									// Split overfull leaf and retry
									p_node_id = split_leaf(p_node_id, p_aabb);
									continue;
								}

								// Handle malformed nodes (should be extremely rare)
								if (tnode.num_children == 1) {
									// Use Godot's built-in warning system
									WARN_PRINT_ONCE("BVH: Degenerate node with single child detected during insertion");
									p_node_id = tnode.children[0];
									continue;
								}

								// Standard binary tree descent
								const TNode &child0 = _nodes[tnode.children[0]];
								const TNode &child1 = _nodes[tnode.children[1]];
								const int which = p_aabb.select_by_proximity(child0.aabb, child1.aabb);
								p_node_id = tnode.children[which];
							}
						}
