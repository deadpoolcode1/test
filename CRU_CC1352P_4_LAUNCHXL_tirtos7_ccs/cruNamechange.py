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
        version3=(line[-4])
        version2=(line[-9])
        version1=(line[-14])
        break

target = r'CDU_'+version1+version2+version3+r'.bin'





shutil.copyfile(original, target)
