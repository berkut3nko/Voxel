module;
#include <chrono>
export module VoxelGame.Utils;

export namespace VoxelGame::Utils {
    struct Profiler {
        using Clock = std::chrono::high_resolution_clock;
        std::chrono::time_point<Clock> start;
        void begin() { start = Clock::now(); }
        double end() { 
            auto dur = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
            return dur.count() / 1000.0; 
        }
    };
}