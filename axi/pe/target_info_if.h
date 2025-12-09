/*
 * target_info_if.h
 *
 *  Created on: Aug 23, 2023
 *      Author: eyckj
 */

#ifndef _AXI_PE_TARGET_INFO_IF_H_
#define _AXI_PE_TARGET_INFO_IF_H_

#include <cstddef>

namespace axi {
namespace pe {

class target_info_if {
public:
    virtual ~target_info_if() = default;

    virtual size_t get_outstanding_tx_count() = 0;
};

} /* namespace pe */
} /* namespace axi */

#endif /* _AXI_PE_TARGET_INFO_IF_H_ */
