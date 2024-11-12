#ifndef _JTCP_COMMON_COMMON_DEFINE_H
#define _JTCP_COMMON_COMMON_DEFINE_H

#include <string>
#include <string_view>

namespace JTCP::Types
{
    using PortType = uint16_t;
    using IPStrType = std::string;
    using IPStrVType = std::string_view;
}

#endif