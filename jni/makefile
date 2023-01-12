

ifndef NDKPATH
    $(error NDKPATH not set)
endif

all:
	${NDKPATH}/ndk-build 
	-adb push ../libs/armeabi-v7a/hqx /data/local/tmp/
