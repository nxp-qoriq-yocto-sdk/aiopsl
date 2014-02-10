#include "common/types.h"
#include "common/fsl_stdio.h"
#include "common/fsl_string.h"
#include "fsl_dpni.h"
#include "dplib/dpni_drv.h"
#include "dpni/drv.h"


#pragma push
#pragma section code_type ".receivecb_func"
#pragma force_active on
#pragma function_align 256  
#pragma require_prototypes off

void app_receive_cb (void) 
{    
    receive_cb();
}

#pragma pop