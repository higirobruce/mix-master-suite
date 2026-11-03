mm_add_plugin(NAME imager PRODUCT_NAME "Stereo Imager" CODE Imgr)
target_sources(imager PRIVATE ImagerProcessor.cpp ImagerEditor.cpp)
