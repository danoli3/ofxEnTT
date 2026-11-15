#include "ofxEnTTParallel.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
int main() {
    ofxEnTT::ParallelExecutor pool;
    for (size_t threads: {1, 2, 4, 3, 1, 8, 4}) {
        pool.setThreadCount(threads);
        std::vector<int> visits(10007);
        for (int iteration=0; iteration<100; ++iteration) {
            pool.forEachRange(visits.size(), [&](size_t begin,size_t end) {
                for(size_t i=begin;i<end;++i) ++visits[i];
            }, 1);
            if(iteration%11==0) {
                bool caught=false;
                try { pool.forEachRange(10007,[](size_t begin,size_t end) {
                    if(begin<=9500 && end>9500) throw std::runtime_error("expected");
                },1); } catch(const std::runtime_error&) { caught=true; }
                if(!caught) return 1;
            }
        }
        if(!std::all_of(visits.begin(),visits.end(),[](int v){return v==100;})) return 2;
    }
    std::cout << "700 uneven jobs, resizing and worker exceptions passed.\n";
}
