package indexing

type BTree struct {
	root *BTreeNode
}

func NewBTree() *BTree {
	return &BTree{
		root: NewNode(NodeTypeLeaf),
	}
}

func (tree *BTree) Insert(key int, value []byte) {
	if tree.root == nil {
		tree.root = NewNode(NodeTypeLeaf)
	}

	newkey, sibling := tree.insertRecursive(tree.root, key, value)

	if sibling != nil {
		newRoot := NewNode(NodeTypeInternal)
		newRoot.keys = append(newRoot.keys, newkey)
		newRoot.values = append(newRoot.values, []byte{}, []byte{})
		tree.root = newRoot
	}
}

func (tree *BTree) insertRecursive(node *BTreeNode, key int, value []byte) (int, *BTreeNode) {
	if node.IsLeaf() {
		node.InsertKey(key, value)
		if node.numKeys < PageSize {
			return 0, nil
		}
		return node.Split()
	}

	idx := 0
	for idx < int(node.numKeys) && node.keys[idx] < key {
		idx++
	}

	newKey, sibling := tree.insertRecursive(
		node.values[idx].(*BTreeNode),
		key,
		value,
	)

	if sibling != nil {
		node.InsertKey(newKey, sibling)
		if len(node.keys) > PageSize {
			return node.Split()
		}
	}
	return 0, nil
}
