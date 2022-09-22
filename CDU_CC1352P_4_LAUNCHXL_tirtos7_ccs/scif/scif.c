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
    /*0x00C0*/ 0xBEEB, 0x254F, 0xAEFE, 0x645B, 0x445B, 0x4477, 0x0488, 0x5656, 0x655B, 0x455B, 0x0000, 0x0CAD, 0x0001, 0x0CAE, 0x1610, 0x0488, 
    /*0x00E0*/ 0x5657, 0x665B, 0x465B, 0x0000, 0x0CAD, 0x0002, 0x0CAE, 0x1678, 0x0488, 0x5658, 0x675B, 0x475B, 0x0000, 0x0CAD, 0x0004, 0x0CAE, 
    /*0x0100*/ 0x8604, 0x14B9, 0x0488, 0x765B, 0x565B, 0x86FF, 0x03FF, 0x0CB0, 0x645C, 0x78AF, 0x68B0, 0xED37, 0xB605, 0x0000, 0x0CAF, 0x7CB5, 
    /*0x0120*/ 0x6540, 0x0CB0, 0x78B0, 0x68B1, 0xFD0E, 0xF801, 0xE95A, 0xFD0E, 0xBE01, 0x6553, 0xBDB7, 0x700B, 0xFB96, 0x4453, 0x2454, 0xAEFE, 
    /*0x0140*/ 0xADB7, 0x6453, 0x2454, 0xA6FE, 0x7000, 0xFB96, 0xADB7, 0x0000, 0x0201, 0x020F, 0x06F1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0160*/ 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0019, 0x001A, 0x0018, 0x0017, 0x0016, 
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
    /*0x02C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x02E0*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0300*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0320*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
    /*0x0340*/ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0360*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x0380*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03A0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03C0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    /*0x03E0*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0000, 
    /*0x0400*/ 0x0000, 0x54BB, 0x47BB, 0x0001, 0x8B81, 0x8623, 0x0302, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0xADB7, 0xADB7, 
    /*0x0420*/ 0x5657, 0x665B, 0x465B, 0x09FE, 0x8A02, 0xBE06, 0x08C0, 0x0DFF, 0x08B9, 0x0DF9, 0x08C1, 0x0E00, 0x09FF, 0x18C0, 0x8D29, 0xBE03, 
    /*0x0440*/ 0x0000, 0x0DFB, 0x0635, 0x0001, 0x0DFB, 0x0001, 0x0DFE, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B57, 0x665B, 0x465B, 
    /*0x0460*/ 0x7657, 0x647F, 0x0036, 0x8B3C, 0xFDB1, 0x09FF, 0x8A00, 0xBE34, 0x09FC, 0x8A00, 0xBE04, 0x74BB, 0x67BB, 0x0000, 0x0D27, 0x09FC, 
    /*0x0480*/ 0x8A64, 0xAE06, 0x74BB, 0x67BB, 0x0000, 0x0D27, 0x0000, 0x0DFC, 0x09FC, 0x8801, 0x0DFC, 0x09FD, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 
    /*0x04A0*/ 0x0A00, 0x8A00, 0xBE05, 0x7003, 0x8606, 0x16FD, 0xF401, 0x0659, 0x78C6, 0xFA01, 0xBE05, 0x0004, 0x8B57, 0x665B, 0x7657, 0x465B, 
    /*0x04C0*/ 0x0064, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x0677, 0x0001, 0x8B81, 0x8623, 0x0322, 
    /*0x04E0*/ 0x8B7D, 0x0036, 0x8B57, 0x665B, 0x465B, 0x7657, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 0x08BA, 0x8801, 0x0CBA, 0x09FF, 0x8A02, 
    /*0x0500*/ 0xBE02, 0x8604, 0x04AD, 0x09FF, 0x8A00, 0xBE08, 0x09FC, 0x8A32, 0xA604, 0x54BB, 0x67BB, 0x0001, 0x0D27, 0x0690, 0x54BB, 0x67BB, 
    /*0x0520*/ 0x09FD, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x09FE, 0x10C2, 0x8F19, 0x8801, 0x8B49, 0x8DB1, 0x0020, 0x8B49, 0x8DB1, 0x0041, 0x8B9F, 
    /*0x0540*/ 0x863F, 0x0301, 0x6467, 0x2567, 0xA6FE, 0x8B8E, 0x0078, 0x8BA6, 0x0018, 0x8B9E, 0x0003, 0x8B9E, 0x8B9E, 0x8B9E, 0x8B9E, 0x1000, 
    /*0x0560*/ 0x2000, 0x5000, 0x6000, 0x0000, 0x8A28, 0xA669, 0x9A04, 0xA665, 0x29F9, 0xAA00, 0xBE03, 0xAD41, 0xA801, 0x06BF, 0x29F9, 0x3A00, 
    /*0x0580*/ 0xBA00, 0xBE06, 0x78BE, 0x8607, 0x1706, 0x78BF, 0x8607, 0x1706, 0x3A00, 0xBA01, 0xBE06, 0x78BE, 0x8607, 0x170F, 0x78BF, 0x8607, 
    /*0x05A0*/ 0x170F, 0xDD42, 0xD401, 0xD001, 0xDA01, 0xBE03, 0x78BE, 0x8607, 0x1718, 0xAA03, 0xEE03, 0x78BF, 0x8607, 0x1718, 0x2A00, 0xAA00, 
    /*0x05C0*/ 0xBE04, 0x78BB, 0x8607, 0x1721, 0x06E8, 0x78BC, 0x8607, 0x1721, 0x6491, 0xEDB1, 0xA990, 0x31D0, 0xAF3B, 0x78BD, 0x8607, 0x1721, 
    /*0x05E0*/ 0x2A00, 0xAA01, 0xBE06, 0x78BE, 0x8607, 0x1718, 0x78BF, 0x8607, 0x1718, 0x6491, 0xEDB1, 0xA990, 0x31A8, 0xAF3B, 0x2A00, 0xAA01, 
    /*0x0600*/ 0xBE06, 0x78BE, 0x8607, 0x1718, 0x78BF, 0x8607, 0x1718, 0xAD40, 0x31D0, 0xDF1B, 0x31A8, 0xEF1B, 0x8D41, 0x30D7, 0xBF1B, 0xBD25, 
    /*0x0620*/ 0x70D7, 0xBF3F, 0x8D41, 0x8808, 0x30D7, 0xBF1B, 0xBD26, 0x70D7, 0xBF3F, 0x8D42, 0x8801, 0x9801, 0x06B6, 0x1000, 0x06B4, 0x8607, 
    /*0x0640*/ 0x172C, 0x7000, 0x3000, 0x1000, 0x2000, 0x5000, 0x0000, 0xBA04, 0xAE01, 0x07B5, 0x8A0A, 0xAE01, 0x07B2, 0xDD40, 0x1004, 0x8B09, 
    /*0x0660*/ 0x9B0C, 0x8620, 0x8938, 0x8D23, 0x11D0, 0x9F19, 0x2003, 0xAA00, 0xEE19, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x6148, 
    /*0x0680*/ 0xEF1E, 0xED29, 0xA60D, 0x4148, 0x9F3C, 0xAA00, 0xFE08, 0x88FF, 0xA8FF, 0x4148, 0xFF1C, 0x4148, 0xEF3C, 0xED47, 0x0745, 0x2000, 
    /*0x06A0*/ 0xA8FF, 0x0737, 0x2000, 0xAA04, 0xE619, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4188, 0xEF1C, 0xED29, 0x9E0D, 0x4188, 
    /*0x06C0*/ 0x9F3C, 0xAA03, 0xE608, 0x8801, 0xA801, 0x4188, 0xFF1C, 0x4188, 0xEF3C, 0xED47, 0x0761, 0x2004, 0xA801, 0x0753, 0x8D45, 0x1004, 
    /*0x06E0*/ 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D23, 0x11A8, 0x9F19, 0x2003, 0xAA00, 0xEE19, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 
    /*0x0700*/ 0x4128, 0xEF1C, 0xED29, 0xA60D, 0x4128, 0x9F3C, 0xAA00, 0xFE08, 0x88FF, 0xA8FF, 0x4128, 0xFF1C, 0x4128, 0xEF3C, 0xED47, 0x0786, 
    /*0x0720*/ 0x2000, 0xA8FF, 0x0778, 0x2000, 0xAA04, 0xE619, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4168, 0xEF1C, 0xED29, 0x9E0D, 
    /*0x0740*/ 0x4168, 0x9F3C, 0xAA03, 0xE608, 0x8801, 0xA801, 0x4168, 0xFF1C, 0x4168, 0xEF3C, 0xED47, 0x07A2, 0x2004, 0xA801, 0x0794, 0x8D45, 
    /*0x0760*/ 0x8801, 0x072A, 0x0000, 0xB801, 0x0727, 0x09FE, 0x88FF, 0x0DFE, 0x0000, 0x1000, 0x2000, 0x5000, 0x6000, 0x3000, 0x79FE, 0xFA00, 
    /*0x0780*/ 0xB602, 0x8604, 0x048B, 0x8A04, 0xA60E, 0x4000, 0x70FB, 0xCF3F, 0x4000, 0x70EB, 0xCF3F, 0x4000, 0x711B, 0xCF3F, 0x4000, 0x710B, 
    /*0x07A0*/ 0xCF3F, 0x8801, 0x07C3, 0x0000, 0xBA04, 0xA646, 0x9A04, 0xA641, 0xAD40, 0x4148, 0xDF1C, 0x4128, 0xEF1C, 0x8D43, 0x9A03, 0xA60B, 
    /*0x07C0*/ 0x40FB, 0xCF1C, 0xCD25, 0x50FB, 0xCF3D, 0x40EB, 0xCF1C, 0xCD26, 0x50EB, 0xCF3D, 0x07F6, 0x40D7, 0xCF1C, 0xCD1D, 0x50D7, 0xCF3D, 
    /*0x07E0*/ 0x8808, 0x40D7, 0xCF1C, 0xCD1E, 0x50D7, 0xCF3D, 0x8D42, 0x4188, 0xDF1C, 0x4168, 0xEF1C, 0x8D43, 0x9A03, 0xA60C, 0x411B, 0xCF1C, 
    /*0x0800*/ 0xCD25, 0x711B, 0xCF3F, 0x410B, 0xCF1C, 0xCD26, 0x710B, 0xCF3F, 0x8604, 0x0415, 0x40D7, 0xCF1C, 0xCD1D, 0x70D7, 0xCF3F, 0x8808, 
    /*0x0820*/ 0x40D7, 0xCF1C, 0xCD1E, 0x70D7, 0xCF3F, 0x8D42, 0x9801, 0x8801, 0x07D6, 0x1000, 0xB801, 0x07D4, 0x0000, 0x8A10, 0xA60F, 0x1000, 
    /*0x0840*/ 0x2148, 0x9F3A, 0x1000, 0x2128, 0x9F3A, 0x13FF, 0x2188, 0x9F3A, 0x13FF, 0x2168, 0x9F3A, 0x8801, 0x8604, 0x041D, 0x09FB, 0x8A01, 
    /*0x0860*/ 0xB616, 0x0000, 0x8A04, 0xA613, 0x10FB, 0x9F19, 0x20F7, 0x9F3A, 0x10EB, 0x9F19, 0x20E7, 0x9F3A, 0x111B, 0x9F19, 0x2117, 0x9F3A, 
    /*0x0880*/ 0x110B, 0x9F19, 0x2107, 0x9F3A, 0x8801, 0x8604, 0x0432, 0x0000, 0x1000, 0x8A04, 0xA61A, 0x9D40, 0x29FB, 0xAA01, 0xB604, 0x20D7, 
    /*0x08A0*/ 0xAF1A, 0x30C7, 0xAF3B, 0x2000, 0x30D7, 0xAF3B, 0x8808, 0x29FB, 0xAA01, 0xB604, 0x20D7, 0xAF1A, 0x30C7, 0xAF3B, 0x2000, 0x30D7, 
    /*0x08C0*/ 0xAF3B, 0x8D41, 0x8801, 0x8604, 0x0449, 0x09FB, 0x8A01, 0xB613, 0x09FF, 0x8A01, 0xBE0E, 0x0002, 0x0DFE, 0x0004, 0x18AC, 0x8D01, 
    /*0x08E0*/ 0x0CAB, 0x09F8, 0x8A01, 0xBE05, 0x0000, 0x0DF8, 0x6540, 0x0000, 0x0CB0, 0x8604, 0x048B, 0x0002, 0x0DFE, 0x0001, 0x8B81, 0x8623, 
    /*0x0900*/ 0x0322, 0x8B7D, 0x0036, 0x8B58, 0x675B, 0x475B, 0x7658, 0x647F, 0x0036, 0x8B3C, 0xFDB1, 0x09FF, 0x8A00, 0xBE12, 0x0084, 0x8B58, 
    /*0x0920*/ 0x675B, 0x7658, 0x475B, 0x0064, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 0x8604, 0x04AB, 
    /*0x0940*/ 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B58, 0x675B, 0x475B, 0x7658, 0x647F, 0x8604, 0x04B8, 0x0001, 0x8B81, 0x8623, 
    /*0x0960*/ 0x0322, 0x8B7D, 0x0036, 0x8B58, 0x675B, 0x475B, 0x7658, 0x647F, 0xADB7, 0x5656, 0x655B, 0x455B, 0x08BA, 0x88FF, 0x0CBA, 0x09FF, 
    /*0x0980*/ 0x8A01, 0xBE02, 0x8606, 0x06E5, 0x09FF, 0x8A00, 0xBE09, 0x09FC, 0x8A32, 0x9E04, 0x47BB, 0x74BB, 0x0001, 0x0D27, 0x8604, 0x04D2, 
    /*0x09A0*/ 0x47BB, 0x74BB, 0x09FD, 0x8A00, 0xBE02, 0x54BB, 0x47BB, 0x09FE, 0x10C4, 0x8F19, 0x8801, 0x8B49, 0x8DB1, 0x0020, 0x8B49, 0x8DB1, 
    /*0x09C0*/ 0x0041, 0x8B9F, 0x863F, 0x0301, 0x6467, 0x2567, 0xA6FE, 0x8B8E, 0x0078, 0x8BA6, 0x0018, 0x8B9E, 0x0003, 0x8B9E, 0x8B9E, 0x8B9E, 
    /*0x09E0*/ 0x8B9E, 0x1000, 0x2000, 0x5000, 0x6000, 0x0000, 0x8A28, 0xA66E, 0x9A04, 0xA669, 0x29F9, 0xAA00, 0xBE04, 0xAD41, 0xA801, 0x8605, 
    /*0x0A00*/ 0x0502, 0x29F9, 0x3A00, 0xBA01, 0xBE06, 0x78BE, 0x8607, 0x1706, 0x78BF, 0x8607, 0x1706, 0x3A00, 0xBA00, 0xBE06, 0x78BE, 0x8607, 
    /*0x0A20*/ 0x170F, 0x78BF, 0x8607, 0x170F, 0xDD42, 0xD401, 0xD001, 0xDA01, 0xBE03, 0x78BE, 0x8607, 0x1718, 0xAA03, 0xEE03, 0x78BF, 0x8607, 
    /*0x0A40*/ 0x1718, 0x2A00, 0xAA00, 0xBE05, 0x78BC, 0x8607, 0x1721, 0x8605, 0x052C, 0x78BB, 0x8607, 0x1721, 0x6491, 0xEDB1, 0xA990, 0x31D0, 
    /*0x0A60*/ 0xAF3B, 0x78BD, 0x8607, 0x1721, 0x2A00, 0xAA00, 0xBE06, 0x78BE, 0x8607, 0x1718, 0x78BF, 0x8607, 0x1718, 0x6491, 0xEDB1, 0xA990, 
    /*0x0A80*/ 0x31A8, 0xAF3B, 0x2A00, 0xAA00, 0xBE06, 0x78BE, 0x8607, 0x1718, 0x78BF, 0x8607, 0x1718, 0xAD40, 0x31D0, 0xDF1B, 0x31A8, 0xEF1B, 
    /*0x0AA0*/ 0x8D41, 0x8804, 0x30D7, 0xBF1B, 0xBD25, 0x70D7, 0xBF3F, 0x8D41, 0x880C, 0x30D7, 0xBF1B, 0xBD26, 0x70D7, 0xBF3F, 0x8D42, 0x8801, 
    /*0x0AC0*/ 0x9801, 0x8604, 0x04F8, 0x1000, 0x8604, 0x04F6, 0x8607, 0x172C, 0x7000, 0x3000, 0x1000, 0x2000, 0x5000, 0x0000, 0xBA04, 0xAE02, 
    /*0x0AE0*/ 0x8606, 0x0608, 0x8A0A, 0xAE02, 0x8606, 0x0604, 0xDD40, 0x1004, 0x8B09, 0x9B0C, 0x8620, 0x8938, 0x8D23, 0x11D0, 0x9F19, 0x2003, 
    /*0x0B00*/ 0xAA00, 0xEE1B, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x6158, 0xEF1E, 0xED29, 0xA60E, 0x4158, 0x9F3C, 0xAA00, 0xFE09, 
    /*0x0B20*/ 0x88FF, 0xA8FF, 0x4158, 0xFF1C, 0x4158, 0xEF3C, 0xED47, 0x8605, 0x058E, 0x2000, 0xA8FF, 0x8605, 0x0580, 0x2000, 0xAA04, 0xE61B, 
    /*0x0B40*/ 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4198, 0xEF1C, 0xED29, 0x9E0E, 0x4198, 0x9F3C, 0xAA03, 0xE609, 0x8801, 0xA801, 
    /*0x0B60*/ 0x4198, 0xFF1C, 0x4198, 0xEF3C, 0xED47, 0x8605, 0x05AC, 0x2004, 0xA801, 0x8605, 0x059E, 0x8D45, 0x1004, 0x8B09, 0x9B0C, 0x8620, 
    /*0x0B80*/ 0x8938, 0x8D23, 0x11A8, 0x9F19, 0x2003, 0xAA00, 0xEE1B, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4138, 0xEF1C, 0xED29, 
    /*0x0BA0*/ 0xA60E, 0x4138, 0x9F3C, 0xAA00, 0xFE09, 0x88FF, 0xA8FF, 0x4138, 0xFF1C, 0x4138, 0xEF3C, 0xED47, 0x8605, 0x05D3, 0x2000, 0xA8FF, 
    /*0x0BC0*/ 0x8605, 0x05C5, 0x2000, 0xAA04, 0xE61B, 0x0004, 0xBB09, 0x8B0C, 0x8620, 0x8938, 0x8D22, 0x4178, 0xEF1C, 0xED29, 0x9E0E, 0x4178, 
    /*0x0BE0*/ 0x9F3C, 0xAA03, 0xE609, 0x8801, 0xA801, 0x4178, 0xFF1C, 0x4178, 0xEF3C, 0xED47, 0x8605, 0x05F1, 0x2004, 0xA801, 0x8605, 0x05E3, 
    /*0x0C00*/ 0x8D45, 0x8801, 0x8605, 0x0572, 0x0000, 0xB801, 0x8605, 0x056E, 0x09FF, 0x8A02, 0xBE03, 0x09FE, 0x88FF, 0x0DFE, 0x0000, 0x1000, 
    /*0x0C20*/ 0x2000, 0x5000, 0x6000, 0x3000, 0x79FE, 0xFA00, 0xB602, 0x8606, 0x06E5, 0x8A04, 0xA60F, 0x4000, 0x7103, 0xCF3F, 0x4000, 0x70F3, 
    /*0x0C40*/ 0xCF3F, 0x4000, 0x7123, 0xCF3F, 0x4000, 0x7113, 0xCF3F, 0x8801, 0x8606, 0x0619, 0x0000, 0xBA04, 0xA64B, 0x9A04, 0xA645, 0xAD40, 
    /*0x0C60*/ 0x4158, 0xDF1C, 0x4138, 0xEF1C, 0x8D43, 0x9A03, 0xA60C, 0x4103, 0xCF1C, 0xCD25, 0x5103, 0xCF3D, 0x40F3, 0xCF1C, 0xCD26, 0x50F3, 
    /*0x0C80*/ 0xCF3D, 0x8606, 0x064F, 0x8804, 0x40D7, 0xCF1C, 0xCD1D, 0x50D7, 0xCF3D, 0x8808, 0x40D7, 0xCF1C, 0xCD1E, 0x50D7, 0xCF3D, 0x8D42, 
    /*0x0CA0*/ 0x4198, 0xDF1C, 0x4178, 0xEF1C, 0x8D43, 0x9A03, 0xA60C, 0x4123, 0xCF1C, 0xCD25, 0x7123, 0xCF3F, 0x4113, 0xCF1C, 0xCD26, 0x7113, 
    /*0x0CC0*/ 0xCF3F, 0x8606, 0x066F, 0x8804, 0x40D7, 0xCF1C, 0xCD1D, 0x70D7, 0xCF3F, 0x8808, 0x40D7, 0xCF1C, 0xCD1E, 0x70D7, 0xCF3F, 0x8D42, 
    /*0x0CE0*/ 0x9801, 0x8801, 0x8606, 0x062D, 0x1000, 0xB801, 0x8606, 0x062B, 0x0000, 0x8A10, 0xA60F, 0x1000, 0x2158, 0x9F3A, 0x1000, 0x2138, 
    /*0x0D00*/ 0x9F3A, 0x13FF, 0x2198, 0x9F3A, 0x13FF, 0x2178, 0x9F3A, 0x8801, 0x8606, 0x0679, 0x09FB, 0x8A01, 0xB616, 0x0000, 0x8A04, 0xA613, 
    /*0x0D20*/ 0x1103, 0x9F19, 0x20FF, 0x9F3A, 0x10F3, 0x9F19, 0x20EF, 0x9F3A, 0x1123, 0x9F19, 0x211F, 0x9F3A, 0x1113, 0x9F19, 0x210F, 0x9F3A, 
    /*0x0D40*/ 0x8801, 0x8606, 0x068E, 0x0000, 0x1000, 0x8A04, 0xA61B, 0x9D40, 0x8804, 0x29FB, 0xAA01, 0xB604, 0x20D7, 0xAF1A, 0x30C7, 0xAF3B, 
    /*0x0D60*/ 0x2000, 0x30D7, 0xAF3B, 0x8808, 0x29FB, 0xAA01, 0xB604, 0x20D7, 0xAF1A, 0x30C7, 0xAF3B, 0x2000, 0x30D7, 0xAF3B, 0x8D41, 0x8801, 
    /*0x0D80*/ 0x8606, 0x06A5, 0x09FB, 0x8A01, 0xB610, 0x0002, 0x0DFE, 0x0004, 0x18AC, 0x8D01, 0x0CAB, 0x09F8, 0x8A01, 0xBE05, 0x0000, 0x0DF8, 
    /*0x0DA0*/ 0x6540, 0x0000, 0x0CB0, 0x8606, 0x06E5, 0x0002, 0x0DFE, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 
    /*0x0DC0*/ 0x7656, 0x647F, 0x0036, 0x8B3C, 0xFDB1, 0x0001, 0x8B81, 0x8623, 0x0322, 0x8B7D, 0x0036, 0x8B56, 0x655B, 0x455B, 0x7656, 0x647F, 
    /*0x0DE0*/ 0xADB7, 0x54BB, 0x47BB, 0x5656, 0x655B, 0x455B, 0x5657, 0x665B, 0x465B, 0x5658, 0x675B, 0x475B, 0xADB7, 0xED47, 0xE007, 0xFDAB, 
    /*0x0E00*/ 0x8600, 0xF8BE, 0xFF07, 0xFD8E, 0xF001, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8C2, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 0xED47, 
    /*0x0E20*/ 0xEDAB, 0x8600, 0xE8C6, 0xF007, 0x5001, 0xDD87, 0xDF26, 0xADB7, 0xED47, 0xEDAB, 0x8600, 0xE8CA, 0xF007, 0x5001, 0xDD87, 0xDF26, 
    /*0x0E40*/ 0xADB7, 0xF8ED, 0xF007, 0x86FF, 0x63F8, 0xEBA3, 0x8680, 0x6000, 0xED8F, 0xEB9B, 0xEB9B, 0xADB7, 0x7079, 0xFBA7, 0x71FB, 0xFBA6, 
    /*0x0E60*/ 0xFBA6, 0x4467, 0x448E, 0xADB7
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
    0x00901172, 0x00501184, 0x1310118E, 0x009013F0  // System AGC
};




/// Run-time logging signatures (CRC-16) for each data structure for each task
static const uint16_t pRtlTaskStructSignatures[] = {
    0x1109, 0x3324, 0x3D98, 0x2647
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


// Generated by NIZAN-CELLIUM-L at 2022-09-22 12:03:18.464
