#include "FRANK/util/timer.h"

#include "FRANK/util/print.h"
#include "FRANK/util/global_key_value.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <vector>


namespace FRANK
{
namespace timing
{

// Global recursive state machine
Timer GlobalTimer;
// Timer into recursive state machine
Timer* current_timer = &GlobalTimer;

Timer& start(const std::string event) {
  if(getGlobalValue("FRANK_DISABLE_TIMER") != "1") {
    current_timer->start_subtimer(event);
    current_timer = &(*current_timer)[event];
  }
  return *current_timer;
}

// TODO Refactor so this doesn't need event?
double stop([[maybe_unused]] const std::string event) {
  if(getGlobalValue("FRANK_DISABLE_TIMER") != "1") {
    assert(current_timer->get_name() == event);
    const double duration = current_timer->stop();
    if (current_timer->get_parent() != nullptr) {
      current_timer = current_timer->get_parent();
    }
    return duration;
  }
  return 0;
}

void clearTimers() { GlobalTimer.clear(); }

void stopAndPrint(const std::string event, const int depth) {
  if(getGlobalValue("FRANK_DISABLE_TIMER") != "1") {
    stop(event);
    printTime(event, depth);
  }
}

void printTime(const std::string event, const int depth) {
  (*current_timer)[event].print_to_depth(depth);
}

double getTotalTime(const std::string event) {
  return (*current_timer)[event].get_total_time();
}

unsigned int getNRuns(const std::string event){
  return (*current_timer)[event].get_n_runs();
}

Timer::Timer() {
  name = "";
  parent = nullptr;
  total_time = seconds::zero();
  running = false;
}

Timer::Timer(const std::string name, Timer* parent)
: name(name), parent(parent) {
  total_time = seconds::zero();
  running = false;
}

void Timer::start() {
  assert(!running);
  running = true;
  start_time = clock::now();
}

void Timer::start_subtimer(const std::string event) {
  if (subtimers.find(event) == subtimers.end()) {
    subtimers[event] = Timer(event, this);
  }
  subtimers[event].start();
}

double Timer::stop() {
  assert(running);
  const time_point end_time = clock::now();
  running = false;
  const seconds time = end_time - start_time;
  times.push_back(time);
  total_time += time;
  return time.count();
}

void Timer::clear() {
  assert(!running);
  assert(parent == nullptr && name.empty());
  total_time = seconds::zero();
  subtimers.clear();
}

std::string Timer::get_name() const { return name; }

Timer* Timer::get_parent() const { return parent; }

double Timer::get_total_time() const { return total_time.count(); }

std::vector<double> Timer::get_times() const {
  std::vector<double> times_list;
  for (const seconds& time : times) {
    times_list.push_back(time.count());
  }
  return times_list;
}

size_t Timer::get_n_runs() const { return times.size(); }

const std::map<std::string, double> Timer::get_subtimers() const {
  std::map<std::string, double> subtimer_list;
  for (const auto& pair : subtimers) {
    subtimer_list[pair.first] = pair.second.get_total_time();
  }
  return subtimer_list;
}

const Timer& Timer::operator[](const std::string event) const {
  assert(subtimers.find(event) != subtimers.end());
  return subtimers.at(event);
}

Timer& Timer::operator[](const std::string event) {
  assert(subtimers.find(event) != subtimers.end());
  return subtimers[event];
}

void Timer::print_to_depth(const int depth) const {
  assert(!running);
  print_to_depth(depth, 0);
}

void Timer::print_to_depth(const int depth, const int at_depth, const std::string tag_pre) const {
  const std::string tag = tag_pre;
  print(at_depth == 0 ? name : tag+"--"+name, total_time.count());
  if (depth > 0) {
    std::vector<const Timer*> time_sorted;
    for (const auto& pair : subtimers) {
      time_sorted.push_back(&pair.second);
    }
    std::sort(
      time_sorted.begin(), time_sorted.end(),
      [](const Timer* a, const Timer* b) {
        return a->total_time > b->total_time;
      }
    );
    for (const Timer* ptr : time_sorted) {
      const std::string child_tag = tag_pre + " |";
      ptr->print_to_depth(depth-1, at_depth+1, child_tag);
    }
  }
  if (depth > 0 && subtimers.size() > 0) {
    double subcounter_sum = 0;
    for (const auto& pair : subtimers) {
      subcounter_sum += pair.second.total_time.count();
    }
    print(
      tag+" |_Subcounters [%]",
      int(std::round(subcounter_sum/total_time.count()*100))
    );
  }
}

} // namespace timing
} // namespace FRANK
