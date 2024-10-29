#include "desktop.hpp"

#include "cocoa.utils.hpp"

#include <filesystem>

#import <Cocoa/Cocoa.h>

namespace saucer
{
    namespace fs = std::filesystem;

    void desktop::open(const std::string &uri)
    {
        const utils::autorelease_guard guard{};

        auto *const workspace = [NSWorkspace sharedWorkspace];
        auto *const str       = [NSString stringWithUTF8String:uri.c_str()];
        auto *const url       = fs::exists(uri) ? [NSURL fileURLWithPath:str] : [NSURL URLWithString:str];

        [workspace openURL:url];
    }
} // namespace saucer
