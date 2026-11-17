#include "novadb/db.hpp"
#include <iostream>

int main() {
    auto db = novadb::DB::open("./my_database");

    db.put("name", "Ahmad");
    db.put("field", "Computer Science");

    auto result = db.get("name");

    if (result) {
        std::cout << *result << std::endl;
    }

    db.erase("field");
    db.flush();

    return 0;
}
