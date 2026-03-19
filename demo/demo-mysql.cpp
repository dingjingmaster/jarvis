/*************************************************************************
> FileName: demo-mysql.cpp
> Author  : DingJing
> Mail    : dingjing@live.cn
> Created Time: 2026年03月19日 星期四 14时10分06秒
 ************************************************************************/

#include "ormpp/mysql.hpp"

#include <iostream>

#include "ormpp/dbng.hpp"
#include "ormpp/connection_pool.hpp"

using namespace ormpp;
const char *password = "";
const char *ip = "127.0.0.1";
const char *username = "root";
const char *db = "test_ormppdb";

struct person
{
    std::optional<std::string> name;
    std::optional<int> age;
    int id;
};
REGISTER_AUTO_KEY(person, id)
YLT_REFL(person, id, name, age)

struct student
{
    std::string name;
    int age;
    int id;
    static constexpr std::string_view get_alias_struct_name(student *) {
        return "t_student";
    }
};
REGISTER_AUTO_KEY(student, id)
YLT_REFL(student, id, name, age)

int main() {
    {
        dbng<mysql> mysql;
        if (mysql.connect(ip, username, password, db)) {
            mysql.create_datatable<person>(ormpp_auto_key{"id"});
            mysql.delete_records<person>();
            mysql.insert<person>({"purecpp"});
            mysql.insert<person>({"purecpp", 6});
        }
        else {
            std::cout << "connect fail" << std::endl;
        }
    }

    {
        auto &pool = connection_pool<dbng<mysql>>::instance();
        pool.init(4, ip, username, password, db, 5, 3306);
        size_t init_size = pool.size();
        assert(init_size == 4);
        {
            auto conn = pool.get();
            init_size = pool.size();
            assert(init_size == 3);
        }
        init_size = pool.size();
        assert(init_size == 4);
    }

    return 0;
}
