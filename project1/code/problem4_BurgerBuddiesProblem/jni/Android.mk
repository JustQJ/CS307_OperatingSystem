LOCAL_PATH := $(call my-dir) 

include $(CLEAR_VARS) 
LOCAL_SRC_FILES := BurgerBuddies.c  #souce code
LOCAL_MODULE := BBCARM   #outfile name
LOCAL_CFLAGS += -pie -fPIE 
LOCAL_LDFLAGS += -pie -fPIE 
LOCAL_FORCE_STATIC_EXECUTABLE := true 
include $(BUILD_EXECUTABLE)
