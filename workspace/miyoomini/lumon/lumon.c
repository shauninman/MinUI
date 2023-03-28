#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <mi_sys.h>
#include <mi_common.h>
#include <mi_disp.h>

int main(void) {
	MI_DISP_DEV dev = 0;
	
	MI_DISP_PubAttr_t attrs;
	memset(&attrs,0,sizeof(MI_DISP_PubAttr_t));
	MI_DISP_GetPubAttr(dev,&attrs);
	
	attrs.eIntfType = E_MI_DISP_INTF_LCD;
	attrs.eIntfSync = E_MI_DISP_OUTPUT_USER;
	MI_DISP_SetPubAttr(dev,&attrs);
	
	MI_DISP_Enable(dev);
	
	MI_DISP_LcdParam_t params;
	memset(&params,0,sizeof(MI_DISP_LcdParam_t));
	MI_DISP_GetLcdParam(dev, &params);

	params.stCsc.u32Luma = 45;
	params.stCsc.u32Contrast = 50;
	params.stCsc.u32Hue = 50;
	params.stCsc.u32Saturation = 45;
	params.u32Sharpness = 0;
	
	MI_DISP_SetLcdParam(dev,&params);
	
	while (1) pause();
	
	// MI_DISP_Disable(dev);
}
