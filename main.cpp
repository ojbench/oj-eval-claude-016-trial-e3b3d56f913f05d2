#include <iostream>
#include <vector>
#include "bptree.hpp"

int main() {
    BPTree tree("database.dat");

    int n;
    std::cin >> n;

    for (int i = 0; i < n; i++) {
        std::string cmd;
        std::cin >> cmd;

        if (cmd == "insert") {
            char index[MAX_KEY_LEN];
            int value;
            std::cin >> index >> value;
            tree.insert(index, value);
        } else if (cmd == "delete") {
            char index[MAX_KEY_LEN];
            int value;
            std::cin >> index >> value;
            tree.remove(index, value);
        } else if (cmd == "find") {
            char index[MAX_KEY_LEN];
            std::cin >> index;
            std::vector<int> result = tree.find(index);
            if (result.empty()) {
                std::cout << "null\n";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) std::cout << " ";
                    std::cout << result[j];
                }
                std::cout << "\n";
            }
        }
    }

    return 0;
}
