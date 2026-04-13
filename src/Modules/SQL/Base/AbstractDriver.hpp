#pragma once

#include <Core/SharedPointer.hpp>
#include <Modules/SQL/Base/QueryResult.hpp>
namespace CXORM::Base {
    class QueryBuilder;
    class AbstractDriver {
        public:
          virtual QueryResult query(SharedPointer<QueryBuilder> query) = 0;
    };
}