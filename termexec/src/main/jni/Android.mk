LOCAL_PATH := $(call my-dir)

# Python-for-Android paths
PYTHON_FOR_ANDROID_PATH := /home/anwar/.local/share/python-for-android
GUEST_PYTHON_PATH := $(PYTHON_FOR_ANDROID_PATH)/build/other_builds/python3
PYTHON_PATH := $(GUEST_PYTHON_PATH)/arm64-v8a__ndk_target_21/python3

include $(CLEAR_VARS)
LOCAL_MODULE := termexec
LOCAL_LDFLAGS := -Wl,--build-id
LOCAL_LDLIBS := \
	-llog \
	-lc \

LOCAL_SRC_FILES := \
	process.cpp \

LOCAL_C_INCLUDES += ../jni
LOCAL_SHARED_LIBRARIES := python3.8
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := python3.8
LOCAL_LDFLAGS := -Wl,--build-id
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := $(PYTHON_PATH)/android-build/libpython3.8.so
LOCAL_EXPORT_CFLAGS := -I $(PYTHON_PATH)/Include
include $(PREBUILT_SHARED_LIBRARY)