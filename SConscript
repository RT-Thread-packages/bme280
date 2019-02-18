from building import *
Import('rtconfig')

src   = []
cwd   = GetCurrentDir()

# add bme280 src files.
src += Glob('sensor_bosch_bme280.c')
src += Glob('libraries/bme280.c')

# add bme280 include path.
path  = [cwd, cwd + '/libraries']

# add src and include to group.
group = DefineGroup('bme280', src, depend = ['PKG_USING_BME280'], CPPPATH = path)

Return('group')
