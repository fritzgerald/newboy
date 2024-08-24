#import <Cocoa/Cocoa.h>
#import "appdelegate.h"

int main(int argc, const char * argv[])
{
    NSLog(@"Entered main");
    NSApplication * application = [NSApplication sharedApplication];

    AppDelegate * appDelegate = [[AppDelegate alloc] init];

    [application setDelegate:appDelegate];
    [application run];

    return EXIT_SUCCESS;
}