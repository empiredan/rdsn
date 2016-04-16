# pragma once
# include <dsn/service_api_cpp.h>

# include <dsn/tool/nfs/nfs_code_definition.h>
# include <dsn/tool/nfs/nfs_types.h>
# include <dsn/cpp/serialization_helper/thrift_helper.h>

namespace dsn {
    namespace service {
        GENERATED_TYPE_SERIALIZATION(copy_request)
        GENERATED_TYPE_SERIALIZATION(copy_response)
        GENERATED_TYPE_SERIALIZATION(get_file_size_request)
        GENERATED_TYPE_SERIALIZATION(get_file_size_response)
    }
}