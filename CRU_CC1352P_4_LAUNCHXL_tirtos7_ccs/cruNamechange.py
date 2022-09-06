import shutil
# for server/collector:
#original = r'CDU_CC1352P_4_LAUNCHXL_tirtos7_ccs.bin'
#target = r'CDU_003.bin'



#for client/sensor:
original = r'CRU_CC1352P_4_LAUNCHXL_tirtos7_ccs.bin'

versionTextFile = open('../oad/native_oad/oad_image_header_app.c', 'r')
Lines = versionTextFile.readlines()
for line in Lines:
    if('SOFTWARE_VER' in line):
        version=(line[-4])
        break

target = r'CRU_00'+version+r'.bin'


shutil.copyfile(original, target)
