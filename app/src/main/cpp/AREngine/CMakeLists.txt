add_library(ARENGINE_LIB INTERFACE)


add_subdirectory(Basic)
add_subdirectory(ARInput)
add_subdirectory(Location)
#add_subdirectory(CameraTracking)
add_subdirectory(HDRSwitch)
add_subdirectory(PoseEstimationRokid)
add_subdirectory(InteractiveUnderstanding)
add_subdirectory(InteractionLogUpload)

