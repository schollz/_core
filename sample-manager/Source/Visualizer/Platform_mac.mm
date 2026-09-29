#include "Platform.h"
#import <AppKit/AppKit.h>
namespace zv
{
bool systemReducedMotion()
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}
} // namespace zv
