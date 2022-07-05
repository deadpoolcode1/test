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
    /*0x00C0*/ 0xBEEB, 0x254F, 0xAEFE, 0x645B, 0x445B, 0x4477, 0x0488, 0x5656, 0x655B, 0x455B, 0x0000, 0x0CAD, 0x0001, 0x0CAE, 0x1722, 0x0488, 
    /*0x00E0*/ 0x5657, 0x665B, 0x465B, 0x0000, 0x0CAD, 0x0002, 0x0CAE, 0x175E, 0x0488, 0x5658, 0x675B, 0x475B, 0x0000, 0x0CAD, 0x0004, 0x0CAE, 
    /*0x0100*/ 0x8605, 0x1583, 0x0488, 0x765B, 0x565B, 0x86FF, 0x03FF, 0x0CB0, 0x645C, 0x78AF, 0x68B0, 0xED37, 0xB605, 0x0000, 0x0CAF, 0x7CB5, 
    /*0x0120*/ 0x6540, 0x0CB0, 0x78B0, 0x68B1, 0xFD0E, 0xF801, 0xE95A, 0xFD0E, 0xBE01, 0x6553, 0xBDB7, 0x700B, 0xFB96, 0x4453, 0x2454, 0xAEFE, 
    /*0x0140*/ 0xADB7, 0x6453, 0x2454, 0xA6FE, 0x7000, 0xFB96, 0xADB7, 0x0000, 0x0313, 0x0321, 0x0790, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0160*/ 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0004, 0x0000, 0x0014, 0x000C, 0x0019, 0x001A, 0x0018, 
    /*0x0180*/ 0x0017, 0x0016, 0x0003, 0x0003, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x01E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0200*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0220*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0240*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0260*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0280*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x02E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0300*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0320*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0340*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0360*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0380*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x03A0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x03C0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x03E0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0400*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0420*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0440*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0460*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0480*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x04A0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x04C0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 
    /*0x04E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0500*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0520*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0540*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0560*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0580*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x05A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x05C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x05E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0600*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 
    /*0x0620*/ 0x0000, 0x0000, 0x0000, 0x08C4, 0x0F12, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 
    /*0x0640*/ 0xADB7, 0xADB7, 0x08C4, 0x0F12, 0x0B12, 0x8A02, 0xBE2B, 0x0B10, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x0B10, 0x8A32, 0xAE04, 0x54BB, 
    /*0x0660*/ 0x47BB, 0x0000, 0x0F10, 0x0B10, 0x8801, 0x0F10, 0x08C5, 0x8A00, 0xBE05, 0x7003, 0x8607, 0x1791, 0xF401, 0x073F, 0x78CC, 0xFA01, 
    /*0x0680*/ 0xBE05, 0x0004, 0x8B57, 0x665B, 0x7657, 0x465B, 0x0064, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 
    /*0x06A0*/ 0x647F, 0x075D, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B57, 0x665B, 0x465B, 0x7657, 0x647F, 0xADB7, 0x0B12, 0x8A01, 
    /*0x06C0*/ 0xBE02, 0x8605, 0x0577, 0x0B12, 0x8A02, 0xBE06, 0x0B10, 0x8A19, 0xA602, 0x54BB, 0x67BB, 0x076E, 0x54BB, 0x67BB, 0x08C2, 0x10C6, 
    /*0x06E0*/ 0x8F19, 0x8801, 0x8B49, 0x8DB1, 0x0020, 0x8B49, 0x8DB1, 0x0041, 0x8B9F, 0x863F, 0x0301, 0x6467, 0x2567, 0xA6FE, 0x8B8E, 0x0078, 
    /*0x0700*/ 0x8BA6, 0x0018, 0x8B9E, 0x0003, 0x8B9E, 0x8B9E, 0x8B9E, 0x8B9E, 0x1000, 0x2000, 0x3000, 0x0000, 0x8A50, 0xAE02, 0x8604, 0x043C, 
    /*0x0720*/ 0x48C5, 0xCA01, 0xBE07, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x179A, 0x07A0, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x17A3, 
    /*0x0740*/ 0x48BA, 0xCA02, 0xBE0A, 0x48C5, 0xCA01, 0xBE04, 0x78C0, 0x8607, 0x17A3, 0x07AD, 0x78C0, 0x8607, 0x179A, 0x48BA, 0xCA03, 0xBE0A, 
    /*0x0760*/ 0x48C5, 0xCA01, 0xBE04, 0x78C1, 0x8607, 0x17A3, 0x07BA, 0x78C1, 0x8607, 0x179A, 0x48BA, 0xCA04, 0xBE10, 0x48C5, 0xCA01, 0xBE07, 
    /*0x0780*/ 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x17A3, 0x07CD, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x179A, 0x48B9, 0x9D2C, 0xA66A, 
    /*0x07A0*/ 0x48C5, 0xCA00, 0xBE04, 0x78BD, 0x8607, 0x17AC, 0x07DA, 0x78BE, 0x8607, 0x17AC, 0x6491, 0xEDB1, 0xC990, 0x8602, 0x52BD, 0xCF3D, 
    /*0x07C0*/ 0x78BF, 0x8607, 0x17AC, 0x48C5, 0xCA01, 0xBE06, 0x78C0, 0x8607, 0x17B7, 0x78C1, 0x8607, 0x17B7, 0x6491, 0xEDB1, 0xC990, 0x8602, 
    /*0x07E0*/ 0x526D, 0xCF3D, 0x48C5, 0xCA01, 0xBE06, 0x78C0, 0x8607, 0x17B7, 0x78C1, 0x8607, 0x17B7, 0x8801, 0x9801, 0x48BA, 0xCA00, 0xBE39, 
    /*0x0800*/ 0x9A01, 0xBE11, 0x48C5, 0xCA01, 0xBE08, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x179A, 0x8604, 0x0413, 0x78C0, 0x8607, 0x179A, 
    /*0x0820*/ 0x78C1, 0x8607, 0x17A3, 0x9A02, 0xBE11, 0x48C5, 0xCA01, 0xBE08, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x17A3, 0x8604, 0x0426, 
    /*0x0840*/ 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x179A, 0x9A03, 0xBE11, 0x48C5, 0xCA01, 0xBE08, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 
    /*0x0860*/ 0x17A3, 0x8604, 0x0439, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x179A, 0x07CD, 0x1000, 0x078C, 0x8607, 0x17C0, 0x1000, 0x5000, 
    /*0x0880*/ 0x6000, 0x0000, 0x7000, 0x48B9, 0xFD2C, 0xAE02, 0x8604, 0x04E4, 0x8A14, 0xAE02, 0x8604, 0x04E0, 0xED40, 0x18B9, 0x8B09, 0x9B0C, 
    /*0x08A0*/ 0x8620, 0x8938, 0x8D27, 0x8602, 0x12BD, 0x9F19, 0x500B, 0xDA00, 0xEE1B, 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x214D, 
    /*0x08C0*/ 0xAF1A, 0xAD29, 0xA60E, 0x414D, 0x9F3C, 0xDA00, 0xFE09, 0x88FF, 0xD8FF, 0x314D, 0xBF1B, 0x414D, 0xAF3C, 0xAD43, 0x8604, 0x0465, 
    /*0x08E0*/ 0x5000, 0xD8FF, 0x8604, 0x0457, 0x5000, 0xDA0C, 0xE61F, 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x8602, 0x220D, 0xAF1A, 
    /*0x0900*/ 0xAD29, 0x9E11, 0x8602, 0x420D, 0x9F3C, 0xDA0B, 0xE60B, 0x8801, 0xD801, 0x8602, 0x320D, 0xBF1B, 0x8602, 0x420D, 0xAF3C, 0xAD43, 
    /*0x0920*/ 0x8604, 0x0485, 0x500C, 0xD801, 0x8604, 0x0475, 0x8D46, 0x18B9, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D27, 0x8602, 0x126D, 0x9F19, 
    /*0x0940*/ 0x500B, 0xDA00, 0xEE1B, 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x20ED, 0xAF1A, 0xAD29, 0xA60E, 0x40ED, 0x9F3C, 0xDA00, 
    /*0x0960*/ 0xFE09, 0x88FF, 0xD8FF, 0x30ED, 0xBF1B, 0x40ED, 0xAF3C, 0xAD43, 0x8604, 0x04AF, 0x5000, 0xD8FF, 0x8604, 0x04A1, 0x5000, 0xDA0C, 
    /*0x0980*/ 0xE61B, 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x21AD, 0xAF1A, 0xAD29, 0x9E0E, 0x41AD, 0x9F3C, 0xDA0B, 0xE609, 0x8801, 
    /*0x09A0*/ 0xD801, 0x31AD, 0xBF1B, 0x41AD, 0xAF3C, 0xAD43, 0x8604, 0x04CD, 0x500C, 0xD801, 0x8604, 0x04BF, 0x8D46, 0x8801, 0x8604, 0x0448, 
    /*0x09C0*/ 0x0000, 0xF801, 0x8604, 0x0443, 0x08C2, 0x88FF, 0x0CC2, 0x0000, 0x1000, 0x5000, 0x6000, 0x2000, 0x7000, 0x38C2, 0xBA00, 0xBE65, 
    /*0x09E0*/ 0x8A04, 0xA60F, 0x3000, 0x40D5, 0xBF3C, 0x3000, 0x40CD, 0xBF3C, 0x3000, 0x40E5, 0xBF3C, 0x3000, 0x40DD, 0xBF3C, 0x8801, 0x8604, 
    /*0x0A00*/ 0x04F0, 0x0000, 0x38B9, 0xFD2B, 0xA62C, 0x9A0C, 0xA626, 0xDD40, 0x214D, 0xEF1A, 0x20ED, 0xAF1A, 0x8D47, 0x30D5, 0xBF1B, 0xBD26, 
    /*0x0A20*/ 0x40D5, 0xBF3C, 0x30CD, 0xBF1B, 0xBD22, 0x20CD, 0xBF3A, 0x8D45, 0x8602, 0x220D, 0xEF1A, 0x21AD, 0xAF1A, 0x8D47, 0x30E5, 0xBF1B, 
    /*0x0A40*/ 0xBD26, 0x40E5, 0xBF3C, 0x30DD, 0xBF1B, 0xBD22, 0x40DD, 0xBF3C, 0x8D45, 0x9801, 0x8801, 0x8605, 0x0505, 0x1000, 0xF801, 0x8605, 
    /*0x0A60*/ 0x0502, 0x0000, 0x8A30, 0xA610, 0x1000, 0x214D, 0x9F3A, 0x1000, 0x20ED, 0x9F3A, 0x13FF, 0x8602, 0x220D, 0x9F3A, 0x13FF, 0x21AD, 
    /*0x0A80*/ 0x9F3A, 0x8801, 0x8605, 0x0532, 0x0B12, 0x8A00, 0xBE0E, 0x0003, 0x0CC2, 0x0004, 0x18AC, 0x8D01, 0x0CAB, 0x0B0D, 0x8A01, 0xBE05, 
    /*0x0AA0*/ 0x0000, 0x0F0D, 0x6540, 0x0000, 0x0CB0, 0x0B12, 0x8A02, 0xBE12, 0x0084, 0x8B58, 0x675B, 0x7658, 0x475B, 0x0064, 0x8B81, 0x8623, 
    /*0x0AC0*/ 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x8605, 0x0575, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 
    /*0x0AE0*/ 0x8B58, 0x675B, 0x475B, 0x7658, 0x647F, 0x8605, 0x0582, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B58, 0x675B, 0x475B, 
    /*0x0B00*/ 0x7658, 0x647F, 0xADB7, 0x0B12, 0x8A00, 0x9602, 0x8607, 0x0784, 0x0B12, 0x8A02, 0xBE07, 0x0B10, 0x8A19, 0x9E02, 0x47BB, 0x74BB, 
    /*0x0B20*/ 0x8605, 0x0594, 0x47BB, 0x74BB, 0x08C2, 0x10C9, 0x8F19, 0x8801, 0x8B49, 0x8DB1, 0x0020, 0x8B49, 0x8DB1, 0x0041, 0x8B9F, 0x863F, 
    /*0x0B40*/ 0x0301, 0x6467, 0x2567, 0xA6FE, 0x8B8E, 0x0078, 0x8BA6, 0x0018, 0x8B9E, 0x0003, 0x8B9E, 0x8B9E, 0x8B9E, 0x8B9E, 0x1000, 0x2000, 
    /*0x0B60*/ 0x3000, 0x0000, 0x8A50, 0xAE02, 0x8606, 0x0669, 0x48C5, 0xCA00, 0xBE08, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x179A, 0x8605, 
    /*0x0B80*/ 0x05C7, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x17A3, 0x48BA, 0xCA02, 0xBE0B, 0x48C5, 0xCA00, 0xBE05, 0x78C0, 0x8607, 0x17A3, 
    /*0x0BA0*/ 0x8605, 0x05D5, 0x78C0, 0x8607, 0x179A, 0x48BA, 0xCA03, 0xBE0B, 0x48C5, 0xCA00, 0xBE05, 0x78C1, 0x8607, 0x17A3, 0x8605, 0x05E3, 
    /*0x0BC0*/ 0x78C1, 0x8607, 0x179A, 0x48BA, 0xCA04, 0xBE11, 0x48C5, 0xCA00, 0xBE08, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x17A3, 0x8605, 
    /*0x0BE0*/ 0x05F7, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x179A, 0x48B9, 0x9D2C, 0xA66C, 0x48C5, 0xCA00, 0xBE05, 0x78BE, 0x8607, 0x17AC, 
    /*0x0C00*/ 0x8606, 0x0605, 0x78BD, 0x8607, 0x17AC, 0x6491, 0xEDB1, 0xC990, 0x8602, 0x52BD, 0xCF3D, 0x78BF, 0x8607, 0x17AC, 0x48C5, 0xCA00, 
    /*0x0C20*/ 0xBE06, 0x78C0, 0x8607, 0x17B7, 0x78C1, 0x8607, 0x17B7, 0x6491, 0xEDB1, 0xC990, 0x8602, 0x526D, 0xCF3D, 0x48C5, 0xCA00, 0xBE06, 
    /*0x0C40*/ 0x78C0, 0x8607, 0x17B7, 0x78C1, 0x8607, 0x17B7, 0x8801, 0x9801, 0x48BA, 0xCA00, 0xBE39, 0x9A01, 0xBE11, 0x48C5, 0xCA00, 0xBE08, 
    /*0x0C60*/ 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x179A, 0x8606, 0x063E, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x17A3, 0x9A02, 0xBE11, 
    /*0x0C80*/ 0x48C5, 0xCA00, 0xBE08, 0x78C0, 0x8607, 0x179A, 0x78C1, 0x8607, 0x17A3, 0x8606, 0x0651, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 
    /*0x0CA0*/ 0x179A, 0x9A03, 0xBE11, 0x48C5, 0xCA00, 0xBE08, 0x78C0, 0x8607, 0x17A3, 0x78C1, 0x8607, 0x17A3, 0x8606, 0x0664, 0x78C0, 0x8607, 
    /*0x0CC0*/ 0x179A, 0x78C1, 0x8607, 0x179A, 0x8605, 0x05F7, 0x1000, 0x8605, 0x05B2, 0x8607, 0x17C0, 0x1000, 0x5000, 0x6000, 0x0000, 0x7000, 
    /*0x0CE0*/ 0x48B9, 0xFD2C, 0xAE02, 0x8607, 0x0711, 0x8A14, 0xAE02, 0x8607, 0x070D, 0xED40, 0x18B9, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D27, 
    /*0x0D00*/ 0x8602, 0x12BD, 0x9F19, 0x500B, 0xDA00, 0xEE1B, 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x217D, 0xAF1A, 0xAD29, 0xA60E, 
    /*0x0D20*/ 0x417D, 0x9F3C, 0xDA00, 0xFE09, 0x88FF, 0xD8FF, 0x317D, 0xBF1B, 0x417D, 0xAF3C, 0xAD43, 0x8606, 0x0692, 0x5000, 0xD8FF, 0x8606, 
    /*0x0D40*/ 0x0684, 0x5000, 0xDA0C, 0xE61F, 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x8602, 0x223D, 0xAF1A, 0xAD29, 0x9E11, 0x8602, 
    /*0x0D60*/ 0x423D, 0x9F3C, 0xDA0B, 0xE60B, 0x8801, 0xD801, 0x8602, 0x323D, 0xBF1B, 0x8602, 0x423D, 0xAF3C, 0xAD43, 0x8606, 0x06B2, 0x500C, 
    /*0x0D80*/ 0xD801, 0x8606, 0x06A2, 0x8D46, 0x18B9, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D27, 0x8602, 0x126D, 0x9F19, 0x500B, 0xDA00, 0xEE1B, 
    /*0x0DA0*/ 0x000C, 0xFB09, 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x211D, 0xAF1A, 0xAD29, 0xA60E, 0x411D, 0x9F3C, 0xDA00, 0xFE09, 0x88FF, 0xD8FF, 
    /*0x0DC0*/ 0x311D, 0xBF1B, 0x411D, 0xAF3C, 0xAD43, 0x8606, 0x06DC, 0x5000, 0xD8FF, 0x8606, 0x06CE, 0x5000, 0xDA0C, 0xE61B, 0x000C, 0xFB09, 
    /*0x0DE0*/ 0x8B0C, 0x8620, 0x8938, 0x8D25, 0x21DD, 0xAF1A, 0xAD29, 0x9E0E, 0x41DD, 0x9F3C, 0xDA0B, 0xE609, 0x8801, 0xD801, 0x31DD, 0xBF1B, 
    /*0x0E00*/ 0x41DD, 0xAF3C, 0xAD43, 0x8606, 0x06FA, 0x500C, 0xD801, 0x8606, 0x06EC, 0x8D46, 0x8801, 0x8606, 0x0675, 0x0000, 0xF801, 0x8606, 
    /*0x0E20*/ 0x0670, 0x0B12, 0x8A01, 0xBE03, 0x08C2, 0x88FF, 0x0CC2, 0x0000, 0x1000, 0x5000, 0x6000, 0x2000, 0x7000, 0x38C2, 0xBA00, 0xBE62, 
    /*0x0E40*/ 0x8A04, 0xA60F, 0x3000, 0x40D9, 0xBF3C, 0x3000, 0x40D1, 0xBF3C, 0x3000, 0x40E9, 0xBF3C, 0x3000, 0x40E1, 0xBF3C, 0x8801, 0x8607, 
    /*0x0E60*/ 0x0720, 0x0000, 0x38B9, 0xFD2B, 0xA62C, 0x9A0C, 0xA626, 0xDD40, 0x217D, 0xEF1A, 0x211D, 0xAF1A, 0x8D47, 0x30D9, 0xBF1B, 0xBD26, 
    /*0x0E80*/ 0x40D9, 0xBF3C, 0x30D1, 0xBF1B, 0xBD22, 0x20D1, 0xBF3A, 0x8D45, 0x8602, 0x223D, 0xEF1A, 0x21DD, 0xAF1A, 0x8D47, 0x30E9, 0xBF1B, 
    /*0x0EA0*/ 0xBD26, 0x40E9, 0xBF3C, 0x30E1, 0xBF1B, 0xBD22, 0x40E1, 0xBF3C, 0x8D45, 0x9801, 0x8801, 0x8607, 0x0735, 0x1000, 0xF801, 0x8607, 
    /*0x0EC0*/ 0x0732, 0x0000, 0x8A30, 0xA610, 0x1000, 0x217D, 0x9F3A, 0x1000, 0x211D, 0x9F3A, 0x13FF, 0x8602, 0x223D, 0x9F3A, 0x13FF, 0x21DD, 
    /*0x0EE0*/ 0x9F3A, 0x8801, 0x8607, 0x0762, 0x0003, 0x0CC2, 0x0004, 0x18AC, 0x8D01, 0x0CAB, 0x0B0D, 0x8A01, 0xBE05, 0x0000, 0x0F0D, 0x6540, 
    /*0x0F00*/ 0x0000, 0x0CB0, 0x8607, 0x17C0, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0xADB7, 
    /*0x0F20*/ 0xADB7, 0xED47, 0xE007, 0xFDAB, 0x8600, 0xF8BE, 0xFF07, 0xFD8E, 0xF001, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8C6, 0xF007, 0x5001, 
    /*0x0F40*/ 0xDD87, 0xDF26, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8C2, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 0xF8ED, 0xF007, 0x86FF, 0x63F8, 
    /*0x0F60*/ 0xEBA3, 0x8680, 0x6000, 0xED8F, 0xEB9B, 0xEB9B, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8CA, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 
    /*0x0F80*/ 0x7079, 0xFBA7, 0x71FB, 0xFBA6, 0xFBA6, 0x4467, 0x448E, 0xADB7
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
    0x00D01172, 0x0070118C, 0x2400119A, 0x0060161A  // System AGC
};




/// Run-time logging signatures (CRC-16) for each data structure for each task
static const uint16_t pRtlTaskStructSignatures[] = {
    0x7EFB, 0xE7AC, 0x9131, 0x686F
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


// Generated by LAPTOP-2C67QHF3 at 2022-07-05 15:30:32.910
