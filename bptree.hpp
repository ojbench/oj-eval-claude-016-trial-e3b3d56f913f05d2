#ifndef BPTREE_HPP
#define BPTREE_HPP

#include <fstream>
#include <cstring>
#include <algorithm>
#include <vector>

const int MAX_KEY_LEN = 65;
const int ORDER = 50;  // B+ tree order
const int MIN_KEYS = ORDER / 2;

struct Key {
    char index[MAX_KEY_LEN];
    int value;

    Key() : value(0) {
        memset(index, 0, sizeof(index));
    }

    Key(const char* idx, int val) : value(val) {
        memset(index, 0, sizeof(index));
        strncpy(index, idx, MAX_KEY_LEN - 1);
    }

    bool operator<(const Key& other) const {
        int cmp = strcmp(index, other.index);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Key& other) const {
        return strcmp(index, other.index) == 0 && value == other.value;
    }

    bool indexEqual(const char* idx) const {
        return strcmp(index, idx) == 0;
    }
};

struct Node {
    bool isLeaf;
    int keyCount;
    Key keys[ORDER + 1];  // Extra space for temporary overflow before split
    int children[ORDER + 2];  // File offsets for internal nodes
    int next;  // Next leaf node (for leaf nodes only)

    Node() : isLeaf(true), keyCount(0), next(-1) {
        memset(children, -1, sizeof(children));
    }
};

class BPTree {
private:
    std::fstream file;
    std::string filename;
    int rootOffset;
    int freeOffset;

    int allocateNode() {
        int offset = freeOffset;
        freeOffset += sizeof(Node);
        writeHeader();  // Update header to persist freeOffset
        return offset;
    }

    void writeNode(int offset, const Node& node) {
        file.seekp(offset);
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
        file.flush();
    }

    Node readNode(int offset) {
        Node node;
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        return node;
    }

    void writeHeader() {
        file.seekp(0);
        file.write(reinterpret_cast<const char*>(&rootOffset), sizeof(int));
        file.write(reinterpret_cast<const char*>(&freeOffset), sizeof(int));
        file.flush();
    }

    void readHeader() {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&rootOffset), sizeof(int));
        file.read(reinterpret_cast<char*>(&freeOffset), sizeof(int));
    }

    int findChild(const Node& node, const Key& key) {
        int i = 0;
        while (i < node.keyCount && !(key < node.keys[i])) {
            i++;
        }
        return i;
    }

    void insertInLeaf(Node& leaf, const Key& key) {
        int i = leaf.keyCount - 1;
        while (i >= 0 && key < leaf.keys[i]) {
            leaf.keys[i + 1] = leaf.keys[i];
            i--;
        }
        leaf.keys[i + 1] = key;
        leaf.keyCount++;
    }

    int splitLeaf(int leafOffset, Node& leaf, Key& upKey) {
        int newLeafOffset = allocateNode();
        Node newLeaf;
        newLeaf.isLeaf = true;

        int mid = (ORDER + 1) / 2;
        newLeaf.keyCount = leaf.keyCount - mid;
        for (int i = 0; i < newLeaf.keyCount; i++) {
            newLeaf.keys[i] = leaf.keys[mid + i];
        }
        leaf.keyCount = mid;

        newLeaf.next = leaf.next;
        leaf.next = newLeafOffset;

        upKey = newLeaf.keys[0];

        writeNode(leafOffset, leaf);
        writeNode(newLeafOffset, newLeaf);

        return newLeafOffset;
    }

    int splitInternal(int nodeOffset, Node& node, Key& upKey) {
        int newNodeOffset = allocateNode();
        Node newNode;
        newNode.isLeaf = false;

        int mid = ORDER / 2;
        upKey = node.keys[mid];

        newNode.keyCount = node.keyCount - mid - 1;
        for (int i = 0; i < newNode.keyCount; i++) {
            newNode.keys[i] = node.keys[mid + 1 + i];
            newNode.children[i] = node.children[mid + 1 + i];
        }
        newNode.children[newNode.keyCount] = node.children[mid + 1 + newNode.keyCount];

        node.keyCount = mid;

        writeNode(nodeOffset, node);
        writeNode(newNodeOffset, newNode);

        return newNodeOffset;
    }

    bool insertRecursive(int nodeOffset, const Key& key, Key& upKey, int& newChildOffset) {
        Node node = readNode(nodeOffset);

        if (node.isLeaf) {
            // Check if key already exists
            for (int i = 0; i < node.keyCount; i++) {
                if (node.keys[i] == key) {
                    return false;  // Duplicate, don't split
                }
            }

            insertInLeaf(node, key);

            if (node.keyCount > ORDER) {
                newChildOffset = splitLeaf(nodeOffset, node, upKey);
                return true;
            } else {
                writeNode(nodeOffset, node);
                return false;
            }
        } else {
            int childIndex = findChild(node, key);
            int childOffset = node.children[childIndex];

            Key childUpKey;
            int childNewOffset;
            bool childSplit = insertRecursive(childOffset, key, childUpKey, childNewOffset);

            if (!childSplit) {
                return false;
            }

            // Insert the new key and child
            for (int i = node.keyCount; i > childIndex; i--) {
                node.keys[i] = node.keys[i - 1];
                node.children[i + 1] = node.children[i];
            }
            node.keys[childIndex] = childUpKey;
            node.children[childIndex + 1] = childNewOffset;
            node.keyCount++;

            if (node.keyCount > ORDER) {
                newChildOffset = splitInternal(nodeOffset, node, upKey);
                return true;
            } else {
                writeNode(nodeOffset, node);
                return false;
            }
        }
    }

    void deleteFromLeaf(Node& leaf, const Key& key) {
        int i = 0;
        while (i < leaf.keyCount && !(leaf.keys[i] == key)) {
            i++;
        }
        if (i < leaf.keyCount) {
            for (int j = i; j < leaf.keyCount - 1; j++) {
                leaf.keys[j] = leaf.keys[j + 1];
            }
            leaf.keyCount--;
        }
    }

    void deleteRecursive(int nodeOffset, const Key& key) {
        Node node = readNode(nodeOffset);

        if (node.isLeaf) {
            deleteFromLeaf(node, key);
            writeNode(nodeOffset, node);
        } else {
            int childIndex = findChild(node, key);
            deleteRecursive(node.children[childIndex], key);
        }
    }

    void findInLeaves(int nodeOffset, const char* index, std::vector<int>& result) {
        if (nodeOffset == -1) return;

        Node node = readNode(nodeOffset);
        bool foundAny = false;
        bool stillMatching = false;

        for (int i = 0; i < node.keyCount; i++) {
            if (node.keys[i].indexEqual(index)) {
                result.push_back(node.keys[i].value);
                foundAny = true;
                stillMatching = true;
            } else if (foundAny) {
                // We've passed all matching keys in this node
                stillMatching = false;
                break;
            }
        }

        // Continue to next leaf only if we found matches at the end
        if (stillMatching && node.next != -1) {
            findInLeaves(node.next, index, result);
        }
    }

    void findRecursive(int nodeOffset, const char* index, std::vector<int>& result) {
        Node node = readNode(nodeOffset);

        if (node.isLeaf) {
            findInLeaves(nodeOffset, index, result);
        } else {
            // Navigate to the correct child
            Key searchKey(index, 0);
            int childIndex = findChild(node, searchKey);
            findRecursive(node.children[childIndex], index, result);
        }
    }

public:
    BPTree(const std::string& fname) : filename(fname) {
        std::ifstream testFile(filename);
        bool exists = testFile.good();
        testFile.close();

        if (exists) {
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
            readHeader();
        } else {
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
            rootOffset = 2 * sizeof(int);
            freeOffset = rootOffset + sizeof(Node);

            Node root;
            root.isLeaf = true;
            writeNode(rootOffset, root);
            writeHeader();
        }
    }

    ~BPTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* index, int value) {
        Key key(index, value);
        Key upKey;
        int newChildOffset;

        if (insertRecursive(rootOffset, key, upKey, newChildOffset)) {
            // Root split
            int newRootOffset = allocateNode();
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.keyCount = 1;
            newRoot.keys[0] = upKey;
            newRoot.children[0] = rootOffset;
            newRoot.children[1] = newChildOffset;

            writeNode(newRootOffset, newRoot);
            rootOffset = newRootOffset;
            writeHeader();
        }
    }

    void remove(const char* index, int value) {
        Key key(index, value);
        deleteRecursive(rootOffset, key);
    }

    std::vector<int> find(const char* index) {
        std::vector<int> result;
        findRecursive(rootOffset, index, result);
        std::sort(result.begin(), result.end());
        return result;
    }
};

#endif
