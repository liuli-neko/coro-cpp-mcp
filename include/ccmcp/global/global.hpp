#pragma once

#include "config.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <ilias/task/scope.hpp>
#include <ilias/io/error.hpp>
#include <nekoproto/jsonrpc/jsonrpc.hpp>
#include <nekoproto/serialization/json/schema.hpp>
#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/types/types.hpp>
#include <optional>


#ifndef CCMCP_NAMESPACE
#define CCMCP_NAMESPACE ccmcp
#endif
#define CCMCP_BN            namespace CCMCP_NAMESPACE {
#define CCMCP_EN            }
#define CCMCP_USE_NAMESPACE using namespace CCMCP_NAMESPACE;
