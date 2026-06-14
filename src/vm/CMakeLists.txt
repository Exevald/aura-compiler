find_path(
        MYSQL_INCLUDE_DIR
        NAMES mysql/mysql.h
        PATHS
        /opt/homebrew/opt/mysql-client/include
        /usr/local/opt/mysql-client/include
)

find_library(
        MYSQL_CLIENT_LIBRARY
        NAMES mysqlclient
        PATHS
        /opt/homebrew/opt/mysql-client/lib
        /usr/local/opt/mysql-client/lib
)

find_path(
        RABBITMQ_INCLUDE_DIR
        NAMES rabbitmq-c/amqp.h amqp.h
        PATHS
        /opt/homebrew/include
        /usr/local/include
        /opt/homebrew/opt/rabbitmq-c/include
        /usr/local/opt/rabbitmq-c/include
)

find_library(
        RABBITMQ_CLIENT_LIBRARY
        NAMES rabbitmq librabbitmq
        PATHS
        /opt/homebrew/lib
        /usr/local/lib
        /opt/homebrew/opt/rabbitmq-c/lib
        /usr/local/opt/rabbitmq-c/lib
)

if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_CLIENT_LIBRARY)
    message(FATAL_ERROR "mysql-client headers and libmysqlclient are required. Install mysql-client and ensure Homebrew paths are available.")
endif()

if (NOT RABBITMQ_INCLUDE_DIR OR NOT RABBITMQ_CLIENT_LIBRARY)
    message(FATAL_ERROR "rabbitmq-c headers and librabbitmq are required. Install rabbitmq-c and ensure Homebrew paths are available.")
endif()

add_library(
        vm_lib
        core/values/ValueHelper.cpp
        runtime/StringPool.cpp
        runtime/SharedRuntimeCore.cpp
        runtime/SharedRuntimeSync.cpp
        runtime/SharedRuntimeActors.cpp
        runtime/SharedRuntimeTasks.cpp
        runtime/SharedRuntimeChannels.cpp
        runtime/ExecutionContextCore.cpp
        runtime/ExecutionContextFrames.cpp
        runtime/ExecutionContextSync.cpp
        runtime/ExecutionContextTransactions.cpp
        runtime/ExecutionContextEffects.cpp
        runtime/stdlib/channel/ChannelModule.cpp
        runtime/stdlib/base64/Base64Module.cpp
        runtime/stdlib/ctx/ContextModule.cpp
        runtime/stdlib/crypto/CryptoModule.cpp
        runtime/stdlib/string/StringModule.cpp
        runtime/stdlib/io/IOModule.cpp
        runtime/stdlib/log/LogModule.cpp
        runtime/stdlib/map/MapModule.cpp
        runtime/stdlib/core/CoreModule.cpp
        runtime/stdlib/memory/MemoryModule.cpp
        runtime/stdlib/sync/SyncModule.cpp
        runtime/stdlib/task/TaskModule.cpp
        runtime/stdlib/env/EnvModule.cpp
        runtime/stdlib/net/NetModule.cpp
        runtime/stdlib/mysql/MySqlModule.cpp
        runtime/stdlib/mysql/MySqlConnectionOps.cpp
        runtime/stdlib/mysql/MySqlHandleCleanup.cpp
        runtime/stdlib/mysql/MySqlModuleInstall.cpp
        runtime/stdlib/mysql/MySqlModuleInstallConnections.cpp
        runtime/stdlib/mysql/MySqlModuleInstallStatements.cpp
        runtime/stdlib/mysql/MySqlModuleInstallResults.cpp
        runtime/stdlib/rabbitmq/RabbitMqModule.cpp
        runtime/stdlib/rabbitmq/RabbitMqHandleCleanup.cpp
        runtime/stdlib/http/HttpRawModule.cpp
        runtime/stdlib/http/HttpServerModule.cpp
        runtime/stdlib/http/HttpServerModuleSocket.cpp
        runtime/stdlib/http/HttpServerModuleInstall.cpp
        runtime/stdlib/json/JsonModule.cpp
        runtime/stdlib/json/JsonModuleParser.cpp
        runtime/stdlib/json/JsonModuleInstall.cpp
        runtime/stdlib/service/ServiceModule.cpp
        runtime/stdlib/time/TimeModule.cpp
        runtime/stdlib/uuid/UuidModule.cpp
        runtime/stdlib/BuiltinModuleInstaller.cpp
        execution/chunk/Chunk.cpp
        execution/VirtualMachineCallsAndActors.cpp
        execution/VirtualMachineCore.cpp
        execution/VirtualMachineDataOps.cpp
        execution/VirtualMachineDebug.cpp
)

target_include_directories(vm_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/execution
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_include_directories(vm_lib PRIVATE
        ${MYSQL_INCLUDE_DIR}
        ${RABBITMQ_INCLUDE_DIR}
)

target_link_libraries(vm_lib PUBLIC ${MYSQL_CLIENT_LIBRARY} ${RABBITMQ_CLIENT_LIBRARY})

add_subdirectory(tests)
