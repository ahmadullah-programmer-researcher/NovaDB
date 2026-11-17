#include "novadb/db.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "novadb-test-store";
    std::filesystem::remove_all(path);
    {
        auto db = novadb::DB::open(path);
        db.put("alpha", "one");
        db.put("beta", "two");
        assert(db.get("alpha").value() == "one");
        novadb::WriteBatch batch;
        batch.put("alpha", "updated").erase("beta").put("gamma", "three");
        db.write(batch);
        assert(db.get("alpha").value() == "updated");
        assert(!db.get("beta").has_value());
        assert(db.size() == 2);
        db.flush();
    }
    {
        auto db = novadb::DB::open(path);
        assert(db.get("alpha").value() == "updated");
        assert(!db.get("beta").has_value());
        assert(db.get("gamma").value() == "three");
        db.put("delta", "four"); // WAL recovery path on next open
    }
    {
        auto db = novadb::DB::open(path);
        assert(db.get("delta").value() == "four");
        assert(db.size() == 3);
    }
    std::filesystem::remove_all(path);
    std::cout << "All NovaDB tests passed.\n";
}
