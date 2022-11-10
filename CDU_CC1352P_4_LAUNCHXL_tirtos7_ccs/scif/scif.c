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
    /*0x0000*/ 0x140E, 0x0417, 0x140E, 0x0438, 0x140E, 0x0442, 0x140E, 0x045F, 0x140E, 0x0468, 0x140E, 0x0471, 0x140E, 0x047B, 0x8953, 0x9954, 
    /*0x0020*/ 0x8D29, 0xBEFD, 0x4553, 0x2554, 0xAEFE, 0x445C, 0xADB7, 0x745B, 0x545B, 0x7000, 0x7CA3, 0x68AC, 0x00A0, 0x1431, 0x68AD, 0x00A1, 
    /*0x0040*/ 0x1431, 0x68AE, 0x00A2, 0x1431, 0x78A3, 0xF801, 0xFA01, 0xBEF2, 0x78AA, 0x68AC, 0xFD0E, 0x68AE, 0xED92, 0xFD06, 0x7CAA, 0x6440, 
    /*0x0060*/ 0x0480, 0x78A3, 0x8F1F, 0xED8F, 0xEC01, 0xBE01, 0xADB7, 0x8DB7, 0x755B, 0x555B, 0x78A8, 0x60BF, 0xEF27, 0xE240, 0xEF27, 0x7000, 
    /*0x0080*/ 0x7CA8, 0x0480, 0x6477, 0x0000, 0x18AA, 0x9D88, 0x9C01, 0xB60E, 0x109F, 0xAF19, 0xAA00, 0xB60A, 0xA8FF, 0xAF39, 0xBE07, 0x0CA3, 
    /*0x00A0*/ 0x8600, 0x88A1, 0x8F08, 0xFD47, 0x9DB7, 0x08A3, 0x8801, 0x8A01, 0xBEEB, 0x254F, 0xAEFE, 0x645B, 0x445B, 0x4477, 0x0480, 0x5656, 
    /*0x00C0*/ 0x655B, 0x455B, 0x0000, 0x0CA3, 0x0001, 0x0CA4, 0x167A, 0x0480, 0x5657, 0x665B, 0x465B, 0x0000, 0x0CA3, 0x0002, 0x0CA4, 0x16F4, 
    /*0x00E0*/ 0x0480, 0x5658, 0x675B, 0x475B, 0x0000, 0x0CA3, 0x0004, 0x0CA4, 0x8605, 0x1558, 0x0480, 0x765B, 0x565B, 0x86FF, 0x03FF, 0x0CA6, 
    /*0x0100*/ 0x645C, 0x78A5, 0x68A6, 0xED37, 0xB605, 0x0000, 0x0CA5, 0x7CAB, 0x6540, 0x0CA6, 0x78A6, 0x68A7, 0xFD0E, 0xF801, 0xE95A, 0xFD0E, 
    /*0x0120*/ 0xBE01, 0x6553, 0xBDB7, 0x700B, 0xFB96, 0x4453, 0x2454, 0xAEFE, 0xADB7, 0x6453, 0x2454, 0xA6FE, 0x7000, 0xFB96, 0xADB7, 0x0000, 
    /*0x0140*/ 0x026B, 0x0279, 0x07C3, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0160*/ 0x0000, 0x0000, 0x0000, 0x0019, 0x001A, 0x0018, 0x0017, 0x0016, 0x0000, 0x0000, 0x0000, 0x0082, 0x0082, 0x0082, 0x0082, 0x0000, 
    /*0x0180*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0200*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0220*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 
    /*0x0240*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0260*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0280*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 
    /*0x02E0*/ 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0300*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0320*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0340*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0360*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0380*/ 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0400*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0420*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0440*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0460*/ 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0480*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x04A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x04C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x54BB, 0x47BB, 0x0001, 0x8B81, 0x8623, 
    /*0x04E0*/ 0x0302, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0xADB7, 0xADB7, 0x5657, 0x665B, 0x465B, 0x08AF, 0x8A01, 0xBE04, 
    /*0x0500*/ 0x78B6, 0x8607, 0x17CF, 0x0687, 0x78B6, 0x8607, 0x17D8, 0x08B0, 0x8A01, 0xBE04, 0x78B7, 0x8607, 0x17CF, 0x0691, 0x78B7, 0x8607, 
    /*0x0520*/ 0x17D8, 0x0A3E, 0x8A00, 0xBE04, 0x08B8, 0x0E69, 0x08B9, 0x0E6A, 0x0A69, 0x18B8, 0x8D29, 0xBE03, 0x0000, 0x0E46, 0x06B1, 0x0001, 
    /*0x0540*/ 0x0E46, 0x004A, 0x0E3E, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B57, 0x665B, 0x465B, 0x7657, 0x647F, 0x0036, 0x8B3C, 
    /*0x0560*/ 0xFDB1, 0x0A69, 0x8A00, 0xBE34, 0x0A47, 0x8A00, 0xBE04, 0x74BB, 0x67BB, 0x0000, 0x0E12, 0x0A47, 0x8A64, 0xAE06, 0x74BB, 0x67BB, 
    /*0x0580*/ 0x0000, 0x0E12, 0x0000, 0x0E47, 0x0A47, 0x8801, 0x0E47, 0x0A48, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x0A6A, 0x8A00, 0xBE05, 0x7003, 
    /*0x05A0*/ 0x8607, 0x17E1, 0xF401, 0x06D5, 0x78C3, 0xFA01, 0xBE05, 0x0004, 0x8B57, 0x665B, 0x7657, 0x465B, 0x0064, 0x8B81, 0x8623, 0x0322, 
    /*0x05C0*/ 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x06F3, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B57, 0x665B, 
    /*0x05E0*/ 0x465B, 0x7657, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 0x0A69, 0x8A02, 0xBE02, 0x8605, 0x054C, 0x0A69, 0x8A00, 0xBE08, 0x0A47, 
    /*0x0600*/ 0x8A32, 0xA604, 0x54BB, 0x67BB, 0x0001, 0x0E12, 0x0709, 0x54BB, 0x67BB, 0x0A48, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x0A34, 0x8801, 
    /*0x0620*/ 0x8A04, 0xBE01, 0x0000, 0x0E34, 0x08B1, 0x8A00, 0xBE27, 0x0A34, 0x8A00, 0xBE06, 0x78B6, 0x8607, 0x17CF, 0x78B7, 0x8607, 0x17CF, 
    /*0x0640*/ 0x0A34, 0x8A01, 0xBE06, 0x78B6, 0x8607, 0x17D8, 0x78B7, 0x8607, 0x17CF, 0x0A34, 0x8A02, 0xBE06, 0x78B6, 0x8607, 0x17CF, 0x78B7, 
    /*0x0660*/ 0x8607, 0x17D8, 0x0A34, 0x8A03, 0xBE06, 0x78B6, 0x8607, 0x17D8, 0x78B7, 0x8607, 0x17D8, 0x0A34, 0x0E11, 0x0742, 0x08B1, 0x0E11, 
    /*0x0680*/ 0x08B1, 0x0E34, 0x8602, 0x1213, 0x8602, 0x0249, 0x9D18, 0x8602, 0x2249, 0x0A34, 0x8A00, 0xBE02, 0x8602, 0x2251, 0x0A34, 0x8A01, 
    /*0x06A0*/ 0xBE02, 0x8602, 0x2253, 0x0A34, 0x8A02, 0xBE02, 0x8602, 0x2255, 0x0A34, 0x8A03, 0xBE02, 0x8602, 0x2257, 0x0000, 0x6000, 0x3A34, 
    /*0x06C0*/ 0xBA00, 0xBE05, 0x3A3E, 0xBA00, 0xBE02, 0x6416, 0x6001, 0x3A34, 0xBA01, 0xBE05, 0x3A3F, 0xBA00, 0xBE02, 0x6416, 0x6001, 0x3A34, 
    /*0x06E0*/ 0xBA02, 0xBE05, 0x3A40, 0xBA00, 0xBE02, 0x6416, 0x6001, 0x3A34, 0xBA03, 0xBE05, 0x3A41, 0xBA00, 0xBE02, 0x6416, 0x6001, 0xEA00, 
    /*0x0700*/ 0xBE06, 0xBF12, 0xCF12, 0x6416, 0xBB0A, 0xCB12, 0xA8FE, 0x38BC, 0xB801, 0xBB49, 0x8DB1, 0x3A6A, 0xBA00, 0xBE04, 0x78B3, 0x8607, 
    /*0x0720*/ 0x17EA, 0x0795, 0x78B4, 0x8607, 0x17EA, 0x3020, 0xBB49, 0x8DB1, 0x3041, 0xBB9F, 0x8635, 0x3101, 0x6467, 0x2567, 0xA6FE, 0xBB8E, 
    /*0x0740*/ 0x3078, 0xBBA6, 0x3018, 0xBB9E, 0x3003, 0xBB9E, 0xBB9E, 0xBB9E, 0xBB9E, 0x658E, 0x458E, 0x30F0, 0xBB82, 0x3001, 0xBB7E, 0x6480, 
    /*0x0760*/ 0xDDB1, 0x3A34, 0xBA00, 0xBE01, 0x0A3E, 0x3A34, 0xBA01, 0xBE01, 0x0A3F, 0x3A34, 0xBA02, 0xBE01, 0x0A40, 0x3A34, 0xBA03, 0xBE01, 
    /*0x0780*/ 0x0A41, 0xEDB1, 0xB990, 0x6172, 0xBF3E, 0x3172, 0xBF1B, 0x3E10, 0x3A10, 0xBB10, 0xF918, 0xE917, 0x3000, 0x4A34, 0xCA00, 0xBE04, 
    /*0x07A0*/ 0x4A3E, 0xCA4A, 0xB601, 0x3001, 0x4A34, 0xCA01, 0xBE04, 0x4A3F, 0xCA4A, 0xB601, 0x3001, 0x4A34, 0xCA02, 0xBE04, 0x4A40, 0xCA4A, 
    /*0x07C0*/ 0xB601, 0x3001, 0x4A34, 0xCA03, 0xBE04, 0x4A41, 0xCA4A, 0xB601, 0x3001, 0xBA01, 0xBE02, 0xEF32, 0xFF32, 0xBA00, 0xBE04, 0xAD21, 
    /*0x07E0*/ 0xEF32, 0xFF32, 0xAD19, 0x3A34, 0xBA00, 0xBE06, 0xBD40, 0xB801, 0xBA4B, 0xBE01, 0x3000, 0x3E3E, 0x3A34, 0xBA01, 0xBE06, 0xBD40, 
    /*0x0800*/ 0xB801, 0xBA4B, 0xBE01, 0x3000, 0x3E3F, 0x3A34, 0xBA02, 0xBE06, 0xBD40, 0xB801, 0xBA4B, 0xBE01, 0x3000, 0x3E40, 0x3A34, 0xBA03, 
    /*0x0820*/ 0xBE05, 0x8801, 0x8A4B, 0xBE01, 0x0000, 0x0E41, 0x0A34, 0x3A10, 0x416E, 0xCF1C, 0xBD2C, 0xA603, 0x3A10, 0x416E, 0xBF3C, 0x3A10, 
    /*0x0840*/ 0x416A, 0xCF1C, 0xBD2C, 0x9E03, 0x3A10, 0x416A, 0xBF3C, 0x3A33, 0xBA01, 0xBE10, 0x8602, 0x323E, 0xBF1B, 0xBA4B, 0xBE0B, 0x6540, 
    /*0x0860*/ 0x3000, 0x3CA6, 0x3000, 0x416A, 0xBF3C, 0x33FF, 0x416E, 0xBF3C, 0x3000, 0x3E33, 0x8607, 0x17F5, 0x3A34, 0xBA00, 0xBE02, 0x8602, 
    /*0x0880*/ 0x2259, 0x3A34, 0xBA01, 0xBE02, 0x8602, 0x225B, 0x3A34, 0xBA02, 0xBE02, 0x8602, 0x225D, 0x3A34, 0xBA03, 0xBE02, 0x8602, 0x225F, 
    /*0x08A0*/ 0x6000, 0x3A34, 0xBA00, 0xBE05, 0x3A36, 0xBA00, 0xBE02, 0x6416, 0x6001, 0x3A34, 0xBA01, 0xBE05, 0x3A37, 0xBA00, 0xBE02, 0x6416, 
    /*0x08C0*/ 0x6001, 0x3A34, 0xBA02, 0xBE05, 0x3A38, 0xBA00, 0xBE02, 0x6416, 0x6001, 0x3A34, 0xBA03, 0xBE05, 0x3A39, 0xBA00, 0xBE02, 0x6416, 
    /*0x08E0*/ 0x6001, 0xEA00, 0xBE06, 0xEF12, 0xFF12, 0x6416, 0xEB0A, 0xFB12, 0xA8FE, 0x78B5, 0x8607, 0x17EA, 0x3020, 0xBB49, 0x8DB1, 0x3041, 
    /*0x0900*/ 0xBB9F, 0x8635, 0x3101, 0x6467, 0x2567, 0xA6FE, 0xBB8E, 0x3078, 0xBBA6, 0x3018, 0xBB9E, 0x3003, 0xBB9E, 0xBB9E, 0xBB9E, 0xBB9E, 
    /*0x0920*/ 0x658E, 0x458E, 0xDDB1, 0x658E, 0x458E, 0x38BB, 0xB801, 0xBB49, 0x8DB1, 0x3A34, 0xBA00, 0xBE01, 0x0A36, 0x3A34, 0xBA01, 0xBE01, 
    /*0x0940*/ 0x0A37, 0x3A34, 0xBA02, 0xBE01, 0x0A38, 0x3A34, 0xBA03, 0xBE01, 0x0A39, 0xEDB1, 0xB990, 0x40CC, 0xBF3C, 0x30CC, 0xBF1B, 0x3E10, 
    /*0x0960*/ 0x3A10, 0xBB10, 0xF918, 0xE917, 0x3000, 0x4A34, 0xCA00, 0xBE04, 0x4A36, 0xCA4A, 0xB601, 0x3001, 0x4A34, 0xCA01, 0xBE04, 0x4A37, 
    /*0x0980*/ 0xCA4A, 0xB601, 0x3001, 0x4A34, 0xCA02, 0xBE04, 0x4A38, 0xCA4A, 0xB601, 0x3001, 0x4A34, 0xCA03, 0xBE04, 0x4A39, 0xCA4A, 0xB601, 
    /*0x09A0*/ 0x3001, 0xBA01, 0xBE02, 0xEF32, 0xFF32, 0xBA00, 0xBE03, 0xAD21, 0xEF32, 0xFF32, 0x1A34, 0x9A00, 0xBE06, 0x9D40, 0x9801, 0x9A4B, 
    /*0x09C0*/ 0xBE01, 0x1000, 0x1E36, 0x1A34, 0x9A01, 0xBE06, 0x9D40, 0x9801, 0x9A4B, 0xBE01, 0x1000, 0x1E37, 0x1A34, 0x9A02, 0xBE06, 0x9D40, 
    /*0x09E0*/ 0x9801, 0x9A4B, 0xBE01, 0x1000, 0x1E38, 0x1A34, 0x9A03, 0xBE05, 0x8801, 0x8A4B, 0xBE01, 0x0000, 0x0E39, 0x0A34, 0x1A10, 0x20C8, 
    /*0x0A00*/ 0xAF1A, 0x9D2A, 0xA603, 0x1A10, 0x20C8, 0x9F3A, 0x1A10, 0x20C4, 0xAF1A, 0x9D2A, 0x9E03, 0x1A10, 0x20C4, 0x9F3A, 0x1A33, 0x9A01, 
    /*0x0A20*/ 0xBE10, 0x8602, 0x1236, 0x9F19, 0x9A4B, 0xBE0B, 0x6540, 0x1000, 0x1CA6, 0x1000, 0x20C4, 0x9F3A, 0x13FF, 0x20C8, 0x9F3A, 0x0000, 
    /*0x0A40*/ 0x0E33, 0x4480, 0x0003, 0x8801, 0x8B49, 0x8DB1, 0x658E, 0x458E, 0x8607, 0x17F5, 0x0A69, 0x8A00, 0xBE12, 0x0084, 0x8B58, 0x675B, 
    /*0x0A60*/ 0x7658, 0x475B, 0x0064, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x8605, 0x054A, 0x0001, 
    /*0x0A80*/ 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x8605, 0x0557, 0x0001, 0x8B81, 0x8623, 0x0322, 
    /*0x0AA0*/ 0x8B7D, 0x0036, 0x8B58, 0x675B, 0x475B, 0x7658, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 0x0A69, 0x8A00, 0xBE09, 0x0A47, 0x8A32, 
    /*0x0AC0*/ 0x9E04, 0x47BB, 0x74BB, 0x0001, 0x0E12, 0x8605, 0x0569, 0x47BB, 0x74BB, 0x0A48, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x0A69, 0x8A01, 
    /*0x0AE0*/ 0xBE02, 0x8607, 0x07B7, 0x08B1, 0x8A00, 0xBE28, 0x0A34, 0x8A03, 0xBE06, 0x78B6, 0x8607, 0x17CF, 0x78B7, 0x8607, 0x17CF, 0x0A34, 
    /*0x0B00*/ 0x8A02, 0xBE06, 0x78B6, 0x8607, 0x17D8, 0x78B7, 0x8607, 0x17CF, 0x0A34, 0x8A01, 0xBE06, 0x78B6, 0x8607, 0x17CF, 0x78B7, 0x8607, 
    /*0x0B20*/ 0x17D8, 0x0A34, 0x8A00, 0xBE06, 0x78B6, 0x8607, 0x17D8, 0x78B7, 0x8607, 0x17D8, 0x0A34, 0x0E11, 0x8605, 0x05A2, 0x08B1, 0x0E11, 
    /*0x0B40*/ 0x08B1, 0x0E34, 0x8602, 0x1213, 0x8602, 0x0249, 0x9D18, 0x8602, 0x2249, 0x0A34, 0x8A00, 0xBE02, 0x8602, 0x2249, 0x0A34, 0x8A01, 
    /*0x0B60*/ 0xBE02, 0x8602, 0x224B, 0x0A34, 0x8A02, 0xBE02, 0x8602, 0x224D, 0x0A34, 0x8A03, 0xBE02, 0x8602, 0x224F, 0x0000, 0x5000, 0x6A34, 
    /*0x0B80*/ 0xEA00, 0xBE05, 0x3A42, 0xBA00, 0xBE02, 0x6416, 0x5001, 0x6A34, 0xEA01, 0xBE05, 0x3A43, 0xBA00, 0xBE02, 0x6416, 0x5001, 0x6A34, 
    /*0x0BA0*/ 0xEA02, 0xBE05, 0x3A44, 0xBA00, 0xBE02, 0x6416, 0x5001, 0x6A34, 0xEA03, 0xBE05, 0x3A45, 0xBA00, 0xBE02, 0x6416, 0x5001, 0xDA00, 
    /*0x0BC0*/ 0xBE06, 0xBF12, 0xDF12, 0x6416, 0xBB0A, 0xDB12, 0xA8FE, 0x5A6A, 0xDA00, 0xBE05, 0x78B4, 0x8607, 0x17EA, 0x8605, 0x05F2, 0x78B3, 
    /*0x0BE0*/ 0x8607, 0x17EA, 0x58BB, 0xD801, 0xDB49, 0x8DB1, 0x5020, 0xDB49, 0x8DB1, 0x5041, 0xDB9F, 0x8635, 0x5101, 0x6467, 0x2567, 0xA6FE, 
    /*0x0C00*/ 0xDB8E, 0x5078, 0xDBA6, 0x5018, 0xDB9E, 0x5003, 0xDB9E, 0xDB9E, 0xDB9E, 0xDB9E, 0x658E, 0x458E, 0x50F0, 0xDB82, 0x5001, 0xDB7E, 
    /*0x0C20*/ 0x6480, 0x5A34, 0xDA00, 0xBE01, 0x0A42, 0x5A34, 0xDA01, 0xBE01, 0x0A43, 0x5A34, 0xDA02, 0xBE01, 0x0A44, 0x5A34, 0xDA03, 0xBE01, 
    /*0x0C40*/ 0x0A45, 0xEDB1, 0xD990, 0x61C5, 0xDF3E, 0x51C5, 0xDF1D, 0x5E10, 0x5A10, 0xDB10, 0xE918, 0xD917, 0x7000, 0x3A34, 0xBA00, 0xBE04, 
    /*0x0C60*/ 0x3A42, 0xBA4A, 0xB601, 0x7001, 0x3A34, 0xBA01, 0xBE04, 0x3A43, 0xBA4A, 0xB601, 0x7001, 0x3A34, 0xBA02, 0xBE04, 0x3A44, 0xBA4A, 
    /*0x0C80*/ 0xB601, 0x7001, 0x3A34, 0xBA03, 0xBE04, 0x3A45, 0xBA4A, 0xB601, 0x7001, 0xFA01, 0xBE02, 0xDF32, 0xEF32, 0xFA00, 0xBE04, 0xAD21, 
    /*0x0CA0*/ 0xDF32, 0xEF32, 0xAD19, 0x3A34, 0xBA00, 0xBE06, 0xBD40, 0xB801, 0xBA4B, 0xBE01, 0x3000, 0x3E42, 0x3A34, 0xBA01, 0xBE06, 0xBD40, 
    /*0x0CC0*/ 0xB801, 0xBA4B, 0xBE01, 0x3000, 0x3E43, 0x3A34, 0xBA02, 0xBE06, 0xBD40, 0xB801, 0xBA4B, 0xBE01, 0x3000, 0x3E44, 0x3A34, 0xBA03, 
    /*0x0CE0*/ 0xBE05, 0x8801, 0x8A4B, 0xBE01, 0x0000, 0x0E45, 0x0A34, 0x3A10, 0x51C1, 0xDF1D, 0xBD2D, 0xA603, 0x3A10, 0x51C1, 0xBF3D, 0x3A10, 
    /*0x0D00*/ 0x51BD, 0xDF1D, 0xBD2D, 0x9E03, 0x3A10, 0x51BD, 0xBF3D, 0x3A33, 0xBA01, 0xBE10, 0x8602, 0x3242, 0xBF1B, 0xBA4B, 0xBE0B, 0x6540, 
    /*0x0D20*/ 0x3000, 0x3CA6, 0x3000, 0x51BD, 0xBF3D, 0x33FF, 0x51C1, 0xBF3D, 0x3000, 0x3E33, 0x8607, 0x17F5, 0x38B1, 0xBA00, 0xBE28, 0x3A34, 
    /*0x0D40*/ 0xBA00, 0xBE06, 0x78B6, 0x8607, 0x17CF, 0x78B7, 0x8607, 0x17CF, 0x3A34, 0xBA01, 0xBE06, 0x78B6, 0x8607, 0x17D8, 0x78B7, 0x8607, 
    /*0x0D60*/ 0x17CF, 0x3A34, 0xBA02, 0xBE06, 0x78B6, 0x8607, 0x17CF, 0x78B7, 0x8607, 0x17D8, 0x3A34, 0xBA03, 0xBE06, 0x78B6, 0x8607, 0x17D8, 
    /*0x0D80*/ 0x78B7, 0x8607, 0x17D8, 0x3A34, 0x3E11, 0x8606, 0x06C9, 0x38B1, 0x3E11, 0x3A34, 0xBA00, 0xBE02, 0x8602, 0x2261, 0x3A34, 0xBA01, 
    /*0x0DA0*/ 0xBE02, 0x8602, 0x2263, 0x3A34, 0xBA02, 0xBE02, 0x8602, 0x2265, 0x3A34, 0xBA03, 0xBE02, 0x8602, 0x2267, 0x5000, 0x3A34, 0xBA00, 
    /*0x0DC0*/ 0xBE05, 0x3A3A, 0xBA00, 0xBE02, 0x6416, 0x5001, 0x3A34, 0xBA01, 0xBE05, 0x3A3B, 0xBA00, 0xBE02, 0x6416, 0x5001, 0x3A34, 0xBA02, 
    /*0x0DE0*/ 0xBE05, 0x3A3C, 0xBA00, 0xBE02, 0x6416, 0x5001, 0x3A34, 0xBA03, 0xBE05, 0x3A3D, 0xBA00, 0xBE02, 0x6416, 0x5001, 0xDA00, 0xBE06, 
    /*0x0E00*/ 0xBF12, 0xDF12, 0x6416, 0xBB0A, 0xDB12, 0xA8FE, 0x78B5, 0x8607, 0x17EA, 0x3020, 0xBB49, 0x8DB1, 0x3041, 0xBB9F, 0x8635, 0x3101, 
    /*0x0E20*/ 0x6467, 0x2567, 0xA6FE, 0xBB8E, 0x3078, 0xBBA6, 0x3018, 0xBB9E, 0x3003, 0xBB9E, 0xBB9E, 0xBB9E, 0xBB9E, 0x658E, 0x458E, 0xDDB1, 
    /*0x0E40*/ 0x658E, 0x458E, 0x38BB, 0xB801, 0xBB49, 0x8DB1, 0x3A34, 0xBA00, 0xBE01, 0x0A3A, 0x3A34, 0xBA01, 0xBE01, 0x0A3B, 0x3A34, 0xBA02, 
    /*0x0E60*/ 0xBE01, 0x0A3C, 0x3A34, 0xBA03, 0xBE01, 0x0A3D, 0xEDB1, 0xB990, 0x511F, 0xBF3D, 0x311F, 0xBF1B, 0x3E10, 0x3A10, 0xBB10, 0xE918, 
    /*0x0E80*/ 0xD917, 0x7000, 0x3A34, 0xBA00, 0xBE04, 0x3A3A, 0xBA4A, 0xB601, 0x7001, 0x3A34, 0xBA01, 0xBE04, 0x3A3B, 0xBA4A, 0xB601, 0x7001, 
    /*0x0EA0*/ 0x3A34, 0xBA02, 0xBE04, 0x3A3C, 0xBA4A, 0xB601, 0x7001, 0x3A34, 0xBA03, 0xBE04, 0x3A3D, 0xBA4A, 0xB601, 0x7001, 0xFA01, 0xBE02, 
    /*0x0EC0*/ 0xDF32, 0xEF32, 0xFA00, 0xBE03, 0xAD21, 0xDF32, 0xEF32, 0x1A34, 0x9A00, 0xBE06, 0x9D40, 0x9801, 0x9A4B, 0xBE01, 0x1000, 0x1E3A, 
    /*0x0EE0*/ 0x1A34, 0x9A01, 0xBE06, 0x9D40, 0x9801, 0x9A4B, 0xBE01, 0x1000, 0x1E3B, 0x1A34, 0x9A02, 0xBE06, 0x9D40, 0x9801, 0x9A4B, 0xBE01, 
    /*0x0F00*/ 0x1000, 0x1E3C, 0x1A34, 0x9A03, 0xBE05, 0x8801, 0x8A4B, 0xBE01, 0x0000, 0x0E3D, 0x0A34, 0x1A10, 0x211B, 0xAF1A, 0x9D2A, 0xA603, 
    /*0x0F20*/ 0x1A10, 0x211B, 0x9F3A, 0x1A10, 0x2117, 0xAF1A, 0x9D2A, 0x9E03, 0x1A10, 0x2117, 0x9F3A, 0x1A33, 0x9A01, 0xBE10, 0x8602, 0x123A, 
    /*0x0F40*/ 0x9F19, 0x9A4B, 0xBE0B, 0x6540, 0x1000, 0x1CA6, 0x1000, 0x2117, 0x9F3A, 0x13FF, 0x211B, 0x9F3A, 0x0000, 0x0E33, 0x4480, 0x0003, 
    /*0x0F60*/ 0x8801, 0x8B49, 0x8DB1, 0x658E, 0x458E, 0x8607, 0x17F5, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 
    /*0x0F80*/ 0x7656, 0x647F, 0xADB7, 0x54BB, 0x47BB, 0x5656, 0x655B, 0x455B, 0x5657, 0x665B, 0x465B, 0x5658, 0x675B, 0x475B, 0xADB7, 0xED47, 
    /*0x0FA0*/ 0xEDAB, 0x8600, 0xE8C2, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8C6, 0xF007, 0x5001, 0xDD87, 0xDF26, 
    /*0x0FC0*/ 0xADB7, 0xED47, 0xE007, 0xFDAB, 0x8600, 0xF8BE, 0xFF07, 0xFD8E, 0xF001, 0xADB7, 0xF8ED, 0xF007, 0x86FF, 0x63F8, 0xEBA3, 0x8680, 
    /*0x0FE0*/ 0x6000, 0xED8F, 0xEB9B, 0xEB9B, 0xADB7, 0x7079, 0xFBA7, 0x71FB, 0xFBA6, 0xFBA6, 0x4467, 0x448E, 0xADB7
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
    0x00B0115E, 0x00A01174, 0x16F01188, 0x03801466  // System AGC
};




// No run-time logging task data structure signatures needed in this project




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
    (volatile SCIF_INT_DATA_T*) 0x400E0146,
    (volatile SCIF_TASK_CTRL_T*) 0x400E0154,
    (volatile uint16_t*) 0x400E013E,
    0x0000,
    sizeof(pAuxRamImage),
    pAuxRamImage,
    pScifTaskDataStructInfoLut,
    pAuxIoIndexToMcuIocfgOffsetLut,
    0x0000,
    24,
    scifDriverSetupInit,
    scifDriverSetupUninit,
    (volatile uint16_t*) NULL,
    (volatile uint16_t*) NULL,
    NULL
};




// No task-specific API available


//@}


// Generated by NIZAN-CELLIUM-L at 2022-11-09 15:40:03.076
