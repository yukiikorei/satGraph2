#pragma once

#include <vector>

namespace satgraf {

class UnionFind {
public:
    explicit UnionFind(size_t n)
        : parent_(n), rank_(n, 0), count_(n) {
        for (size_t i = 0; i < n; ++i) {
            parent_[i] = i;
        }
    }

    size_t find(size_t x) {
        if (parent_[x] != x) {
            parent_[x] = find(parent_[x]);
        }
        return parent_[x];
    }

    void unite(size_t x, size_t y) {
        size_t rx = find(x);
        size_t ry = find(y);
        if (rx == ry) return;

        if (rank_[rx] < rank_[ry]) {
            parent_[rx] = ry;
        } else if (rank_[rx] > rank_[ry]) {
            parent_[ry] = rx;
        } else {
            parent_[ry] = rx;
            rank_[rx]++;
        }
        count_--;
    }

    bool connected(size_t x, size_t y) {
        return find(x) == find(y);
    }

    size_t count_components() const noexcept { return count_; }

private:
    std::vector<size_t> parent_;
    std::vector<size_t> rank_;
    size_t count_;
};

} // namespace satgraf
