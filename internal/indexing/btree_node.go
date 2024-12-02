package indexing

import (
	"bytes"
	"encoding/binary"
)

const (
	NodeTypeLeaf     = 1
	NodeTypeInternal = 2
	PageSize         = 4096 // 4KB
)

/*
BNode Structure
| type | nkeys | pointers | offsets | key-values | unused |
| 2B | 2B | nkeys * 8B | nkeys * 2B |
|
| klen | vlen | key | val |
| 2B | 2B | ... | ... |
*/

type BNode []byte

const (
	HeaderSize  = 4
	PointerSize = 8
	KeySize     = 2
)

func (node BNode) NodeType() uint16 {
	return binary.LittleEndian.Uint16(node[0:2])
}

func (node BNode) NumKeys() uint16 {
	return binary.LittleEndian.Uint16(node[2:4])
}

func (node BNode) SetHeader(nodeType, numKeys uint16) {
	binary.LittleEndian.PutUint16(node[0:2], nodeType)
	binary.LittleEndian.PutUint16(node[2:4], numKeys)
}

// Child node pointers
// 
// returns the position of the pointer for the child node at index idx
func (node BNode) GetPointer(idx uint16) uint64 {
	if idx >= node.NumKeys() {
		return 0
	}
	offset := HeaderSize + idx*PointerSize
	return binary.LittleEndian.Uint64(node[offset : offset+PointerSize])
}

// sets the pointer for the child node at index idx
func (node BNode) SetPointer(idx uint16, value uint64) {
	offset := HeaderSize + idx*PointerSize
	binary.LittleEndian.PutUint64(node[offset:offset+PointerSize], value)
}

// offset list

// returns the position of the offset for the key-value pair at index idx
func (node BNode) OffsetPos(idx uint16) uint16 {
	if idx >= 1 && idx <= node.NumKeys() {
		return HeaderSize + node.NumKeys()*PointerSize + (idx-1)*KeySize
	}
	return 0
}

// returns the offset of the key-value pair at index idx
func (node BNode) GetOffset(idx uint16) uint16 {
	if idx == 0 {
		return 0
	}
	return binary.LittleEndian.Uint16(node[node.OffsetPos(idx):])
}

// sets the offset of the key-value pair at index idx
func (node BNode) SetOffset(idx uint16, value uint16) {
	if idx == 0 {
		return
	}
	binary.LittleEndian.PutUint16(node[node.OffsetPos(idx):], value)
}

// Key values

// returns the position of the key-value pair
func (node BNode) KvPos(idx uint16) uint16 {
	if idx <= node.NumKeys() {
		return HeaderSize + node.NumKeys()*PointerSize + node.NumKeys()*KeySize + node.GetOffset(idx)
	}
	return 0
}

// returns the Key of a kv pair at index idx
func (node BNode) GetKey(idx uint16) []byte {
	if idx <= node.NumKeys() {
		pos := node.KvPos(idx)
		keyLen := binary.LittleEndian.Uint16(node[pos:])
		return node[pos+4:][:keyLen] // klen and vlen are both 2 bytes so added 4 to move to key (see BNode structure)
	}
	return nil
}

// returns the Value of a kv pair at index idx
func (node BNode) GetValue(idx uint16) []byte {
	if idx <= node.NumKeys() {
		pos := node.KvPos(idx)
		keyLen := binary.LittleEndian.Uint16(node[pos:])
		valLen := binary.LittleEndian.Uint16(node[pos+2:])
		return node[pos+4+keyLen:][:valLen]
	}
	return nil
}

// returns number of bytes used by the node
func (node BNode) NumBytes() uint16 {
	return node.KvPos(node.NumKeys())
}

// returns the first child node whose range intersects with the given key
// i.e (child[i] <= key)
// TODO: Binary Search
func (node BNode) nodeLookupLE(key []byte) uint16 {
	numKeys := node.NumKeys()
	found := uint16(0)
	// The first key is always a copy of the parent node
	// thus its always <= key
	for i := uint16(1); i <= numKeys; i++ {
		cmp := bytes.Compare(node.GetKey(i), key)
		if cmp <= 0 {
			found = i
		}
		if cmp > 0 {
			break
		}
	}
	return found
}
