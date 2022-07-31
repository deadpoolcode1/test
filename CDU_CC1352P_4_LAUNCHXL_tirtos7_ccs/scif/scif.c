/// \addtogroup module_scif_driver_setup
//@{
#include "scif.h"
#include "scif_framework.h"
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_aon_event.h)
#include DeviceFamily_constructPath(inc/hw_aon_rtc.h)
#include DeviceFamily_constructPath(inc/hw_aon_pmctl.h)
#include DeviceFamily_constructPath(inc/hw_aux_sce.h)
#include DeviceFamily_constructPath(inc/hw_aux_smph.h)
#include DeviceFamily_constructPath(inc/hw_aux_spim.h)
#include DeviceFamily_constructPath(inc/hw_aux_evctl.h)
#include DeviceFamily_constructPath(inc/hw_aux_aiodio.h)
#include DeviceFamily_constructPath(inc/hw_aux_timer01.h)
#include DeviceFamily_constructPath(inc/hw_aux_sysif.h)
#include DeviceFamily_constructPath(inc/hw_event.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)
#include DeviceFamily_constructPath(inc/hw_ioc.h)
#include <string.h>
#if defined(__IAR_SYSTEMS_ICC__)
    #include <intrinsics.h>
#endif


// OSAL function prototypes
uint32_t scifOsalEnterCriticalSection(void);
void scifOsalLeaveCriticalSection(uint32_t key);




/// Firmware image to be uploaded to the AUX RAM
static const uint16_t pAuxRamImage[] = {
    /*0x0000*/ 0x140E, 0x0417, 0x140E, 0x0440, 0x140E, 0x044A, 0x140E, 0x0467, 0x140E, 0x0470, 0x140E, 0x0479, 0x140E, 0x0483, 0x8953, 0x9954, 
    /*0x0020*/ 0x8D29, 0xBEFD, 0x4553, 0x2554, 0xAEFE, 0x445C, 0xADB7, 0x745B, 0x545B, 0x7000, 0x7CAD, 0x68B6, 0x00A8, 0x1439, 0x68B7, 0x00A9, 
    /*0x0040*/ 0x1439, 0x68B8, 0x00AA, 0x1439, 0x78AD, 0xF801, 0xFA01, 0xBEF2, 0x78B4, 0x68B6, 0xFD0E, 0x68B8, 0xED92, 0xFD06, 0x7CB4, 0x78B3, 
    /*0x0060*/ 0xFA01, 0xBE05, 0x7002, 0x7CB3, 0x78B3, 0xFA00, 0xBEFD, 0x6440, 0x0488, 0x78AD, 0x8F1F, 0xED8F, 0xEC01, 0xBE01, 0xADB7, 0x8DB7, 
    /*0x0080*/ 0x755B, 0x555B, 0x78B2, 0x60BF, 0xEF27, 0xE240, 0xEF27, 0x7000, 0x7CB2, 0x0488, 0x6477, 0x0000, 0x18B4, 0x9D88, 0x9C01, 0xB60E, 
    /*0x00A0*/ 0x10A7, 0xAF19, 0xAA00, 0xB60A, 0xA8FF, 0xAF39, 0xBE07, 0x0CAD, 0x8600, 0x88A9, 0x8F08, 0xFD47, 0x9DB7, 0x08AD, 0x8801, 0x8A01, 
    /*0x00C0*/ 0xBEEB, 0x254F, 0xAEFE, 0x645B, 0x445B, 0x4477, 0x0488, 0x5656, 0x655B, 0x455B, 0x0000, 0x0CAD, 0x0001, 0x0CAE, 0x160A, 0x0488, 
    /*0x00E0*/ 0x5657, 0x665B, 0x465B, 0x0000, 0x0CAD, 0x0002, 0x0CAE, 0x164D, 0x0488, 0x5658, 0x675B, 0x475B, 0x0000, 0x0CAD, 0x0004, 0x0CAE, 
    /*0x0100*/ 0x8604, 0x1463, 0x0488, 0x765B, 0x565B, 0x86FF, 0x03FF, 0x0CB0, 0x645C, 0x78AF, 0x68B0, 0xED37, 0xB605, 0x0000, 0x0CAF, 0x7CB5, 
    /*0x0120*/ 0x6540, 0x0CB0, 0x78B0, 0x68B1, 0xFD0E, 0xF801, 0xE95A, 0xFD0E, 0xBE01, 0x6553, 0xBDB7, 0x700B, 0xFB96, 0x4453, 0x2454, 0xAEFE, 
    /*0x0140*/ 0xADB7, 0x6453, 0x2454, 0xA6FE, 0x7000, 0xFB96, 0xADB7, 0x0000, 0x01FD, 0x0209, 0x0673, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0160*/ 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0019, 0x001A, 0x0018, 0x0017, 0x0016, 0x0000, 
    /*0x0180*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0200*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0220*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0240*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0260*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0280*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x02E0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0300*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0320*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0340*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0360*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0380*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0002, 0x0000, 0x0000, 0x0001, 0x8B81, 0x8623, 
    /*0x0400*/ 0x0302, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0xADB7, 0xADB7, 0x09FA, 0x8A02, 0xBE06, 0x08BF, 0x0DFB, 0x08B9, 
    /*0x0420*/ 0x0DF7, 0x08C0, 0x0DFC, 0x09FB, 0x8A00, 0xBE2B, 0x09F9, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x09F9, 0x8A64, 0xAE04, 0x54BB, 0x47BB, 
    /*0x0440*/ 0x0000, 0x0DF9, 0x09F9, 0x8801, 0x0DF9, 0x09FC, 0x8A00, 0xBE05, 0x7003, 0x8606, 0x167D, 0xF401, 0x062E, 0x78C5, 0xFA01, 0xBE05, 
    /*0x0460*/ 0x0004, 0x8B57, 0x665B, 0x7657, 0x465B, 0x0064, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 
    /*0x0480*/ 0x064C, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B57, 0x665B, 0x465B, 0x7657, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 
    /*0x04A0*/ 0x09FB, 0x8A02, 0xBE02, 0x8604, 0x0457, 0x09FB, 0x8A00, 0xBE06, 0x09F9, 0x8A32, 0xA602, 0x54BB, 0x67BB, 0x0660, 0x54BB, 0x67BB, 
    /*0x04C0*/ 0x09FA, 0x10C1, 0x8F19, 0x8801, 0x8B49, 0x8DB1, 0x0020, 0x8B49, 0x8DB1, 0x0041, 0x8B9F, 0x863F, 0x0301, 0x6467, 0x2567, 0xA6FE, 
    /*0x04E0*/ 0x8B8E, 0x0078, 0x8BA6, 0x0018, 0x8B9E, 0x0003, 0x8B9E, 0x8B9E, 0x8B9E, 0x8B9E, 0x1000, 0x2000, 0x5000, 0x6000, 0x0000, 0x8A28, 
    /*0x0500*/ 0xA669, 0x9A04, 0xA665, 0x29F7, 0xAA00, 0xBE03, 0xAD41, 0xA801, 0x068A, 0x29F7, 0x39FC, 0xBA00, 0xBE06, 0x78BD, 0x8606, 0x1686, 
    /*0x0520*/ 0x78BE, 0x8606, 0x1686, 0x39FC, 0xBA01, 0xBE06, 0x78BD, 0x8606, 0x168F, 0x78BE, 0x8606, 0x168F, 0xDD42, 0xD401, 0xD001, 0xDA01, 
    /*0x0540*/ 0xBE03, 0x78BD, 0x8606, 0x1698, 0xAA03, 0xEE03, 0x78BE, 0x8606, 0x1698, 0x29FC, 0xAA00, 0xBE04, 0x78BA, 0x8606, 0x16A1, 0x06B3, 
    /*0x0560*/ 0x78BB, 0x8606, 0x16A1, 0x6491, 0xEDB1, 0xA990, 0x31CE, 0xAF3B, 0x78BC, 0x8606, 0x16A1, 0x29FC, 0xAA01, 0xBE06, 0x78BD, 0x8606, 
    /*0x0580*/ 0x1698, 0x78BE, 0x8606, 0x1698, 0x6491, 0xEDB1, 0xA990, 0x31A6, 0xAF3B, 0x29FC, 0xAA01, 0xBE06, 0x78BD, 0x8606, 0x1698, 0x78BE, 
    /*0x05A0*/ 0x8606, 0x1698, 0xAD40, 0x31CE, 0xDF1B, 0x31A6, 0xEF1B, 0x8D41, 0x30D6, 0xBF1B, 0xBD25, 0x70D6, 0xBF3F, 0x8D41, 0x8808, 0x30D6, 
    /*0x05C0*/ 0xBF1B, 0xBD26, 0x70D6, 0xBF3F, 0x8D42, 0x8801, 0x9801, 0x0681, 0x1000, 0x067F, 0x8606, 0x16AC, 0x7000, 0x3000, 0x1000, 0x2000, 
    /*0x05E0*/ 0x5000, 0x0000, 0xBA04, 0xAE01, 0x0780, 0x8A0A, 0xAE01, 0x077D, 0xDD40, 0x1004, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D23, 0x11CE, 
    /*0x0600*/ 0x9F19, 0x2003, 0xAA00, 0xEE19, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x6146, 0xEF1E, 0xED29, 0xA60D, 0x4146, 0x9F3C, 
    /*0x0620*/ 0xAA00, 0xFE08, 0x88FF, 0xA8FF, 0x4146, 0xFF1C, 0x4146, 0xEF3C, 0xED47, 0x0710, 0x2000, 0xA8FF, 0x0702, 0x2000, 0xAA04, 0xE619, 
    /*0x0640*/ 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4186, 0xEF1C, 0xED29, 0x9E0D, 0x4186, 0x9F3C, 0xAA03, 0xE608, 0x8801, 0xA801, 
    /*0x0660*/ 0x4186, 0xFF1C, 0x4186, 0xEF3C, 0xED47, 0x072C, 0x2004, 0xA801, 0x071E, 0x8D45, 0x1004, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D23, 
    /*0x0680*/ 0x11A6, 0x9F19, 0x2003, 0xAA00, 0xEE19, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4126, 0xEF1C, 0xED29, 0xA60D, 0x4126, 
    /*0x06A0*/ 0x9F3C, 0xAA00, 0xFE08, 0x88FF, 0xA8FF, 0x4126, 0xFF1C, 0x4126, 0xEF3C, 0xED47, 0x0751, 0x2000, 0xA8FF, 0x0743, 0x2000, 0xAA04, 
    /*0x06C0*/ 0xE619, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4166, 0xEF1C, 0xED29, 0x9E0D, 0x4166, 0x9F3C, 0xAA03, 0xE608, 0x8801, 
    /*0x06E0*/ 0xA801, 0x4166, 0xFF1C, 0x4166, 0xEF3C, 0xED47, 0x076D, 0x2004, 0xA801, 0x075F, 0x8D45, 0x8801, 0x06F5, 0x0000, 0xB801, 0x06F2, 
    /*0x0700*/ 0x09FA, 0x88FF, 0x0DFA, 0x0000, 0x1000, 0x2000, 0x5000, 0x6000, 0x3000, 0x79FA, 0xFA00, 0xB602, 0x8604, 0x0435, 0x8A04, 0xA60E, 
    /*0x0720*/ 0x4000, 0x70FA, 0xCF3F, 0x4000, 0x70EA, 0xCF3F, 0x4000, 0x711A, 0xCF3F, 0x4000, 0x710A, 0xCF3F, 0x8801, 0x078E, 0x0000, 0xBA04, 
    /*0x0740*/ 0xA645, 0x9A04, 0xA640, 0xAD40, 0x4146, 0xDF1C, 0x4126, 0xEF1C, 0x8D43, 0x9A03, 0xA60B, 0x40FA, 0xCF1C, 0xCD25, 0x50FA, 0xCF3D, 
    /*0x0760*/ 0x40EA, 0xCF1C, 0xCD26, 0x50EA, 0xCF3D, 0x07C1, 0x40D6, 0xCF1C, 0xCD1D, 0x50D6, 0xCF3D, 0x8808, 0x40D6, 0xCF1C, 0xCD1E, 0x50D6, 
    /*0x0780*/ 0xCF3D, 0x8D42, 0x4186, 0xDF1C, 0x4166, 0xEF1C, 0x8D43, 0x9A03, 0xA60B, 0x411A, 0xCF1C, 0xCD25, 0x711A, 0xCF3F, 0x410A, 0xCF1C, 
    /*0x07A0*/ 0xCD26, 0x710A, 0xCF3F, 0x07DF, 0x40D6, 0xCF1C, 0xCD1D, 0x70D6, 0xCF3F, 0x8808, 0x40D6, 0xCF1C, 0xCD1E, 0x70D6, 0xCF3F, 0x8D42, 
    /*0x07C0*/ 0x9801, 0x8801, 0x07A1, 0x1000, 0xB801, 0x079F, 0x0000, 0x8A10, 0xA60E, 0x1000, 0x2146, 0x9F3A, 0x1000, 0x2126, 0x9F3A, 0x13FF, 
    /*0x07E0*/ 0x2186, 0x9F3A, 0x13FF, 0x2166, 0x9F3A, 0x8801, 0x07E7, 0x0000, 0x8A04, 0xA612, 0x10FA, 0x9F19, 0x20F6, 0x9F3A, 0x10EA, 0x9F19, 
    /*0x0800*/ 0x20E6, 0x9F3A, 0x111A, 0x9F19, 0x2116, 0x9F3A, 0x110A, 0x9F19, 0x2106, 0x9F3A, 0x8801, 0x07F8, 0x0000, 0x1000, 0x8A04, 0xA614, 
    /*0x0820*/ 0x9D40, 0x20D6, 0xAF1A, 0x30C6, 0xAF3B, 0x2000, 0x30D6, 0xAF3B, 0x8808, 0x20D6, 0xAF1A, 0x30C6, 0xAF3B, 0x2000, 0x30D6, 0xAF3B, 
    /*0x0840*/ 0x8D41, 0x8801, 0x8604, 0x040E, 0x09FB, 0x8A01, 0xBE0E, 0x0002, 0x0DFA, 0x0004, 0x18AC, 0x8D01, 0x0CAB, 0x09F6, 0x8A01, 0xBE05, 
    /*0x0860*/ 0x0000, 0x0DF6, 0x6540, 0x0000, 0x0CB0, 0x09FB, 0x8A00, 0xBE12, 0x0084, 0x8B58, 0x675B, 0x7658, 0x475B, 0x0064, 0x8B81, 0x8623, 
    /*0x0880*/ 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x8604, 0x0455, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 
    /*0x08A0*/ 0x8B58, 0x675B, 0x475B, 0x7658, 0x647F, 0x8604, 0x0462, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B58, 0x675B, 0x475B, 
    /*0x08C0*/ 0x7658, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 0x09FB, 0x8A01, 0xBE02, 0x8606, 0x0667, 0x09FB, 0x8A00, 0xBE07, 0x09F9, 0x8A32, 
    /*0x08E0*/ 0x9E02, 0x47BB, 0x74BB, 0x8604, 0x0477, 0x47BB, 0x74BB, 0x09FA, 0x10C3, 0x8F19, 0x8801, 0x8B49, 0x8DB1, 0x0020, 0x8B49, 0x8DB1, 
    /*0x0900*/ 0x0041, 0x8B9F, 0x863F, 0x0301, 0x6467, 0x2567, 0xA6FE, 0x8B8E, 0x0078, 0x8BA6, 0x0018, 0x8B9E, 0x0003, 0x8B9E, 0x8B9E, 0x8B9E, 
    /*0x0920*/ 0x8B9E, 0x1000, 0x2000, 0x5000, 0x6000, 0x0000, 0x8A28, 0xA66E, 0x9A04, 0xA669, 0x29F7, 0xAA00, 0xBE04, 0xAD41, 0xA801, 0x8604, 
    /*0x0940*/ 0x04A2, 0x29F7, 0x39FC, 0xBA01, 0xBE06, 0x78BD, 0x8606, 0x1686, 0x78BE, 0x8606, 0x1686, 0x39FC, 0xBA00, 0xBE06, 0x78BD, 0x8606, 
    /*0x0960*/ 0x168F, 0x78BE, 0x8606, 0x168F, 0xDD42, 0xD401, 0xD001, 0xDA01, 0xBE03, 0x78BD, 0x8606, 0x1698, 0xAA03, 0xEE03, 0x78BE, 0x8606, 
    /*0x0980*/ 0x1698, 0x29FC, 0xAA00, 0xBE05, 0x78BB, 0x8606, 0x16A1, 0x8604, 0x04CC, 0x78BA, 0x8606, 0x16A1, 0x6491, 0xEDB1, 0xA990, 0x31CE, 
    /*0x09A0*/ 0xAF3B, 0x78BC, 0x8606, 0x16A1, 0x29FC, 0xAA00, 0xBE06, 0x78BD, 0x8606, 0x1698, 0x78BE, 0x8606, 0x1698, 0x6491, 0xEDB1, 0xA990, 
    /*0x09C0*/ 0x31A6, 0xAF3B, 0x29FC, 0xAA00, 0xBE06, 0x78BD, 0x8606, 0x1698, 0x78BE, 0x8606, 0x1698, 0xAD40, 0x31CE, 0xDF1B, 0x31A6, 0xEF1B, 
    /*0x09E0*/ 0x8D41, 0x8804, 0x30D6, 0xBF1B, 0xBD25, 0x70D6, 0xBF3F, 0x8D41, 0x880C, 0x30D6, 0xBF1B, 0xBD26, 0x70D6, 0xBF3F, 0x8D42, 0x8801, 
    /*0x0A00*/ 0x9801, 0x8604, 0x0498, 0x1000, 0x8604, 0x0496, 0x8606, 0x16AC, 0x7000, 0x3000, 0x1000, 0x2000, 0x5000, 0x0000, 0xBA04, 0xAE02, 
    /*0x0A20*/ 0x8605, 0x05A8, 0x8A0A, 0xAE02, 0x8605, 0x05A4, 0xDD40, 0x1004, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D23, 0x11CE, 0x9F19, 0x2003, 
    /*0x0A40*/ 0xAA00, 0xEE1B, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x6156, 0xEF1E, 0xED29, 0xA60E, 0x4156, 0x9F3C, 0xAA00, 0xFE09, 
    /*0x0A60*/ 0x88FF, 0xA8FF, 0x4156, 0xFF1C, 0x4156, 0xEF3C, 0xED47, 0x8605, 0x052E, 0x2000, 0xA8FF, 0x8605, 0x0520, 0x2000, 0xAA04, 0xE61B, 
    /*0x0A80*/ 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4196, 0xEF1C, 0xED29, 0x9E0E, 0x4196, 0x9F3C, 0xAA03, 0xE609, 0x8801, 0xA801, 
    /*0x0AA0*/ 0x4196, 0xFF1C, 0x4196, 0xEF3C, 0xED47, 0x8605, 0x054C, 0x2004, 0xA801, 0x8605, 0x053E, 0x8D45, 0x1004, 0x8B09, 0x9B0C, 0x8620, 
    /*0x0AC0*/ 0x8938, 0x8D23, 0x11A6, 0x9F19, 0x2003, 0xAA00, 0xEE1B, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4136, 0xEF1C, 0xED29, 
    /*0x0AE0*/ 0xA60E, 0x4136, 0x9F3C, 0xAA00, 0xFE09, 0x88FF, 0xA8FF, 0x4136, 0xFF1C, 0x4136, 0xEF3C, 0xED47, 0x8605, 0x0573, 0x2000, 0xA8FF, 
    /*0x0B00*/ 0x8605, 0x0565, 0x2000, 0xAA04, 0xE61B, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4176, 0xEF1C, 0xED29, 0x9E0E, 0x4176, 
    /*0x0B20*/ 0x9F3C, 0xAA03, 0xE609, 0x8801, 0xA801, 0x4176, 0xFF1C, 0x4176, 0xEF3C, 0xED47, 0x8605, 0x0591, 0x2004, 0xA801, 0x8605, 0x0583, 
    /*0x0B40*/ 0x8D45, 0x8801, 0x8605, 0x0512, 0x0000, 0xB801, 0x8605, 0x050E, 0x09FB, 0x8A02, 0xBE03, 0x09FA, 0x88FF, 0x0DFA, 0x0000, 0x1000, 
    /*0x0B60*/ 0x2000, 0x5000, 0x6000, 0x3000, 0x79FA, 0xFA00, 0xB602, 0x8606, 0x0667, 0x8A04, 0xA60F, 0x4000, 0x7102, 0xCF3F, 0x4000, 0x70F2, 
    /*0x0B80*/ 0xCF3F, 0x4000, 0x7122, 0xCF3F, 0x4000, 0x7112, 0xCF3F, 0x8801, 0x8605, 0x05B9, 0x0000, 0xBA04, 0xA64B, 0x9A04, 0xA645, 0xAD40, 
    /*0x0BA0*/ 0x4156, 0xDF1C, 0x4136, 0xEF1C, 0x8D43, 0x9A03, 0xA60C, 0x4102, 0xCF1C, 0xCD25, 0x5102, 0xCF3D, 0x40F2, 0xCF1C, 0xCD26, 0x50F2, 
    /*0x0BC0*/ 0xCF3D, 0x8605, 0x05EF, 0x8804, 0x40D6, 0xCF1C, 0xCD1D, 0x50D6, 0xCF3D, 0x8808, 0x40D6, 0xCF1C, 0xCD1E, 0x50D6, 0xCF3D, 0x8D42, 
    /*0x0BE0*/ 0x4196, 0xDF1C, 0x4176, 0xEF1C, 0x8D43, 0x9A03, 0xA60C, 0x4122, 0xCF1C, 0xCD25, 0x7122, 0xCF3F, 0x4112, 0xCF1C, 0xCD26, 0x7112, 
    /*0x0C00*/ 0xCF3F, 0x8606, 0x060F, 0x8804, 0x40D6, 0xCF1C, 0xCD1D, 0x70D6, 0xCF3F, 0x8808, 0x40D6, 0xCF1C, 0xCD1E, 0x70D6, 0xCF3F, 0x8D42, 
    /*0x0C20*/ 0x9801, 0x8801, 0x8605, 0x05CD, 0x1000, 0xB801, 0x8605, 0x05CB, 0x0000, 0x8A10, 0xA60F, 0x1000, 0x2156, 0x9F3A, 0x1000, 0x2136, 
    /*0x0C40*/ 0x9F3A, 0x13FF, 0x2196, 0x9F3A, 0x13FF, 0x2176, 0x9F3A, 0x8801, 0x8606, 0x0619, 0x0000, 0x8A04, 0xA613, 0x1102, 0x9F19, 0x20FE, 
    /*0x0C60*/ 0x9F3A, 0x10F2, 0x9F19, 0x20EE, 0x9F3A, 0x1122, 0x9F19, 0x211E, 0x9F3A, 0x1112, 0x9F19, 0x210E, 0x9F3A, 0x8801, 0x8606, 0x062B, 
    /*0x0C80*/ 0x0000, 0x1000, 0x8A04, 0xA615, 0x9D40, 0x8804, 0x20D6, 0xAF1A, 0x30C6, 0xAF3B, 0x2000, 0x30D6, 0xAF3B, 0x8808, 0x20D6, 0xAF1A, 
    /*0x0CA0*/ 0x30C6, 0xAF3B, 0x2000, 0x30D6, 0xAF3B, 0x8D41, 0x8801, 0x8606, 0x0642, 0x0002, 0x0DFA, 0x0004, 0x18AC, 0x8D01, 0x0CAB, 0x09F6, 
    /*0x0CC0*/ 0x8A01, 0xBE05, 0x0000, 0x0DF6, 0x6540, 0x0000, 0x0CB0, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 
    /*0x0CE0*/ 0x7656, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 0x5657, 0x665B, 0x465B, 0x5658, 0x675B, 0x475B, 0xADB7, 0xED47, 0xE007, 0xFDAB, 
    /*0x0D00*/ 0x8600, 0xF8BE, 0xFF07, 0xFD8E, 0xF001, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8C2, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 0xED47, 
    /*0x0D20*/ 0xEDAB, 0x8600, 0xE8C6, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8CA, 0xF007, 0x5001, 0xDD87, 0xDF26, 
    /*0x0D40*/ 0xADB7, 0xF8ED, 0xF007, 0x86FF, 0x63F8, 0xEBA3, 0x8680, 0x6000, 0xED8F, 0xEB9B, 0xEB9B, 0xADB7, 0x7079, 0xFBA7, 0x71FB, 0xFBA6, 
    /*0x0D60*/ 0xFBA6, 0x4467, 0x448E, 0xADB7
};


/// Look-up table that converts from AUX I/O index to MCU IOCFG offset
static const uint8_t pAuxIoIndexToMcuIocfgOffsetLut[] = {
    0, 68, 64, 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 0, 0, 0, 0, 0, 120, 116, 112, 108, 104, 100, 96, 92, 88, 84, 80, 76, 72
};


/** \brief Look-up table of data structure information for each task
  *
  * There is one entry per data structure (\c cfg, \c input, \c output and \c state) per task:
  * - [31:20] Data structure size (number of 16-bit words)
  * - [19:12] Buffer count (when 2+, first data structure is preceded by buffering control variables)
  * - [11:0] Address of the first data structure
  */
static const uint32_t pScifTaskDataStructInfoLut[] = {
//  cfg         input       output      state       
    0x00801172, 0x00501182, 0x1300118C, 0x007013EC  // System AGC
};




/// Run-time logging signatures (CRC-16) for each data structure for each task
static const uint16_t pRtlTaskStructSignatures[] = {
    0x6D2D, 0x3324, 0x5DCA, 0x1BD1
};




// No task-specific initialization functions




// No task-specific uninitialization functions




/** \brief Performs driver setup dependent hardware initialization
  *
  * This function is called by the internal driver initialization function, \ref scifInit().
  */
static void scifDriverSetupInit(void) {

    // Select SCE clock frequency in active mode
    HWREG(AON_PMCTL_BASE + AON_PMCTL_O_AUXSCECLK) = AON_PMCTL_AUXSCECLK_SRC_SCLK_HFDIV2;

    // Set the default power mode
    scifSetSceOpmode(AUX_SYSIF_OPMODEREQ_REQ_A);

    // Initialize task resource dependencies
    scifInitIo(25, AUXIOMODE_ANALOG, -1, 0);
    scifInitIo(26, AUXIOMODE_ANALOG, -1, 0);
    scifInitIo(24, AUXIOMODE_ANALOG, -1, 0);
    scifInitIo(3, AUXIOMODE_INPUT, -1, 0);
    scifInitIo(4, AUXIOMODE_INPUT, 0, 0);
    scifInitIo(23, AUXIOMODE_OUTPUT | (0 << BI_AUXIOMODE_OUTPUT_DRIVE_STRENGTH), -1, 0);
    scifInitIo(22, AUXIOMODE_OUTPUT | (0 << BI_AUXIOMODE_OUTPUT_DRIVE_STRENGTH), -1, 0);
    scifInitIo(12, AUXIOMODE_OUTPUT | (0 << BI_AUXIOMODE_OUTPUT_DRIVE_STRENGTH), -1, 0);
    scifInitIo(11, AUXIOMODE_OUTPUT | (0 << BI_AUXIOMODE_OUTPUT_DRIVE_STRENGTH), -1, 0);
    HWREG(AON_PMCTL_BASE + AON_PMCTL_O_AUXSCECLK) = (HWREG(AON_PMCTL_BASE + AON_PMCTL_O_AUXSCECLK) & (~AON_PMCTL_AUXSCECLK_PD_SRC_M)) | AON_PMCTL_AUXSCECLK_PD_SRC_SCLK_LF;
    HWREG(AON_RTC_BASE + AON_RTC_O_CTL) |= AON_RTC_CTL_RTC_4KHZ_EN;

} // scifDriverSetupInit




/** \brief Performs driver setup dependent hardware uninitialization
  *
  * This function is called by the internal driver uninitialization function, \ref scifUninit().
  */
static void scifDriverSetupUninit(void) {

    // Uninitialize task resource dependencies
    scifUninitIo(25, -1);
    scifUninitIo(26, -1);
    scifUninitIo(24, -1);
    scifUninitIo(3, -1);
    scifUninitIo(4, -1);
    scifUninitIo(23, -1);
    scifUninitIo(22, -1);
    scifUninitIo(12, -1);
    scifUninitIo(11, -1);
    HWREG(AON_PMCTL_BASE + AON_PMCTL_O_AUXSCECLK) = (HWREG(AON_PMCTL_BASE + AON_PMCTL_O_AUXSCECLK) & (~AON_PMCTL_AUXSCECLK_PD_SRC_M)) | AON_PMCTL_AUXSCECLK_PD_SRC_NO_CLOCK;
    HWREG(AON_RTC_BASE + AON_RTC_O_CTL) &= ~AON_RTC_CTL_RTC_4KHZ_EN;

} // scifDriverSetupUninit




/** \brief Re-initializes I/O pins used by the specified tasks
  *
  * It is possible to stop a Sensor Controller task and let the System CPU borrow and operate its I/O
  * pins. For example, the Sensor Controller can operate an SPI interface in one application state while
  * the System CPU with SSI operates the SPI interface in another application state.
  *
  * This function must be called before \ref scifExecuteTasksOnceNbl() or \ref scifStartTasksNbl() if
  * I/O pins belonging to Sensor Controller tasks have been borrowed System CPU peripherals.
  *
  * \param[in]      bvTaskIds
  *     Bit-vector of task IDs for the task I/Os to be re-initialized
  */
void scifReinitTaskIo(uint32_t bvTaskIds) {
    if (bvTaskIds & (1 << SCIF_SYSTEM_AGC_TASK_ID)) {
        scifReinitIo(25, -1, 0);
        scifReinitIo(26, -1, 0);
        scifReinitIo(24, -1, 0);
        scifReinitIo(3, -1, 0);
        scifReinitIo(4, 0, 0);
        scifReinitIo(23, -1, 0);
        scifReinitIo(22, -1, 0);
        scifReinitIo(12, -1, 0);
        scifReinitIo(11, -1, 0);
    }
} // scifReinitTaskIo




/// Driver setup data, to be used in the call to \ref scifInit()
const SCIF_DATA_T scifDriverSetup = {
    (volatile SCIF_INT_DATA_T*) 0x400E015A,
    (volatile SCIF_TASK_CTRL_T*) 0x400E0168,
    (volatile uint16_t*) 0x400E014E,
    0x0000,
    sizeof(pAuxRamImage),
    pAuxRamImage,
    pScifTaskDataStructInfoLut,
    pAuxIoIndexToMcuIocfgOffsetLut,
    0x0000,
    24,
    scifDriverSetupInit,
    scifDriverSetupUninit,
    (volatile uint16_t*) 0x400E0156,
    (volatile uint16_t*) 0x400E0158,
    pRtlTaskStructSignatures
};




// No task-specific API available


//@}


// Generated by LAPTOP-2C67QHF3 at 2022-07-31 15:13:00.429
