#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include "qtimer.h"

static const double multi = 1000000000; //1000000000 s to ns

struct timekeep{
    std::chrono::_V2::system_clock::time_point start;
    std::chrono::_V2::system_clock::time_point end;
    std::vector<double> results;
};

std::map<int, timekeep> timers = std::map<int, timekeep>();

void startT(int key){
    const auto [it, success] = timers.insert({key, timekeep{}});
    timekeep* timek = &it->second;
    timek->start = std::chrono::system_clock::now();
}
void endT(int key){
    std::chrono::_V2::system_clock::time_point end = std::chrono::system_clock::now();
    const auto it = timers.find(key);
    timekeep* timek = &it->second;
    timek->end = end;
    std::chrono::duration<double> elapsed_seconds = (timek->end-timek->start);
    timek->results.insert(timek->results.end(), elapsed_seconds.count());    
}
void showStats(bool showInNs){
    int multiplier = 1;
    for(const auto& timer : timers){
        std::cout << "Key value : " << timer.first << "\n"; 
        const timekeep& timek = timer.second;
        int count = 0;
        double total = 0;
        for(const double& result : timek.results){
            total+=result;
            count++;
        }
        std::cout << "Average : " << total/count << "s in " << count << " entries \n"; 
        //std::cout << "Average : " << total/count*multi << "ns in " << count << " entries \n"; 
    }
}