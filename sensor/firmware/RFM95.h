#ifndef _KITELIB_RFM95_H
#define _KITELIB_RFM95_H

#include "TypeDef.h"
#include "Module.h"
#include "SX127x.h"
#include "SX1276.h"

// SX127X_REG_VERSION
#define RFM95_CHIP_VERSION                            0x11

class RFM95: public SX1276 {
  public:
    // constructor
    RFM95(Module* mod);
    
  private:
  
};

#endif
