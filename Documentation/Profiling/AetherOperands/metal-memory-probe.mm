#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
static void* SampleMemory(void*) {
    sleep(25);
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        const char* path = getenv("AETHER_MEMORY_OUTPUT");
        FILE* output = fopen(path ? path : "/tmp/aether-metal-memory.jsonl", "w");
        if (!output) return nullptr;
        for (int sample = 0; sample < 15; ++sample) {
            fprintf(output, "{\"sample\":%d,\"bytes\":%lu,\"unified\":%s,\"device\":\"%s\"}\n", sample, (unsigned long)device.currentAllocatedSize, device.hasUnifiedMemory ? "true" : "false", device.name.UTF8String);
            fflush(output);
            sleep(1);
        }
        fclose(output);
    }
    return nullptr;
}
__attribute__((constructor)) static void StartProbe() {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, SampleMemory, nullptr) == 0) pthread_detach(thread);
}
