#import "GBView.h"
#include <objc/objc.h>


@interface GBView()
    
@end

@implementation GBView

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor blueColor] setFill];
    NSRectFill(dirtyRect);
    [super drawRect: dirtyRect];
}

@end