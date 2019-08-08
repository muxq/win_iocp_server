#ifndef _THREAD_SAFE_STACK_H_
#define _THREAD_SAFE_STACK_H_
/*!
 * \file safe_stack.h
 *
 * \author bujingshen
 * \date 2019-05- 17
 *
 */
#include "mutex.h"
#include <algorithm>
#include <list>

template <typename T> class safe_stack {
public:
  safe_stack() { InitializeCriticalSection(&cs_); }
  ~safe_stack() { DeleteCriticalSection(&cs_); }

  bool empty() {
    mutex_section mc_guard(&cs_);
    return stack_.empty();
  }

  void push_back(T t) {
    if (!t) {
      return;
    }
    mutex_section mc_guard(&cs_);
    stack_.push_back(t);
  }

  void pop_front(T *t) {
    if (!t) {
      return;
    }
    mutex_section mc_guard(&cs_);
    t = NULL;
    if (stack_.empty()) {
      return;
    }

    t = stack_.front();
    stack_.pop_front();
  }

  void remove(T t) {
    mutex_section mc_guard(&cs_);
    if (stack_.empty() || !t) {
      return;
    }
    std::list<T>::iterator iter = std::find(stack_.begin(), stack_.end(), t);
    if (iter == stack_.end()) {
      return;
    }
    stack_.remove(t);
  }

  T pop_front() {
    T t = NULL;
    mutex_section mc_guard(&cs_);
    if (stack_.empty()) {
      return t;
    }
    t = stack_.front();
    stack_.pop_front();
    return t;
  }

protected:
private:
  std::list<T> stack_;
  CRITICAL_SECTION cs_;
};

#endif /* _THREAD_SAFE_STACK_H_ */