#pragma once

#include <CXORM/Core/SharedPointer.hpp>
#include <CXORM/Modules/SQL/Base/QueryResult.hpp>
namespace CXORM::Base {
    class QueryBuilder;
    class AbstractDriver {
        public:
          virtual QueryResult query(SharedPointer<QueryBuilder> query) = 0;
    };
}