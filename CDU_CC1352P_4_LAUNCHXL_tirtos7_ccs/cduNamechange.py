import shutil
# for server/collector:
original = r'CDU_CC1352P_4_LAUNCHXL_tirtos7_ccs.bin'
target = r'CDU_003.bin'

#for client/sensor:
#original = r'CRU_CC1352P_4_LAUNCHXL_tirtos7_ccs.bin'
#target = r'CRU_003.bin'

shutil.copyfile(original, target)