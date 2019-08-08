#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <windows.h>
class mutex_section {
public:
  mutex_section(CRITICAL_SECTION *sc) : sc_(sc) {
    if (sc_) {
      EnterCriticalSection(sc_);
    }
  }

  virtual ~mutex_section() {
    if (sc_) {
      LeaveCriticalSection(sc_);
    }
  }

protected:
  CRITICAL_SECTION *sc_;
};

#endif /* __MUTEX_H__ */