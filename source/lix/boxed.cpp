#include "boxed.hpp"

#include <lix/value.hpp>

lix::value lix::boxed::get_member(const std::string_view member_name) const {
    auto rt          = type_info();
    auto member_info = rt.get_member_info(member_name);
    if (!member_info) {
        throw std::runtime_error{"Boxed object of type <" + rt.name() + "> has no member '"
                                 + std::string(member_name) + "'"};
    }
    return member_info->get_unsafe(get_dataptr());
}