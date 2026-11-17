#include "novadb/db.hpp"
#include <iostream>

int main() {
    auto db = novadb::DB::open("./example-data");
    db.put("language", "C++");
    db.put("database", "NovaDB");

    if (auto value = db.get("language"))
        std::cout << "language = " << *value << '\n';

    novadb::WriteBatch batch;
    batch.put("city", "Peshawar").put("country", "Pakistan");
    db.write(batch);

    std::cout << "\nAll current records:\n";
    for (const auto& [key, value] : db.scan())
        std::cout << key << " => " << value << '\n';

    db.flush();
    std::cout << "\nData flushed to an SSTable.\n";
}
