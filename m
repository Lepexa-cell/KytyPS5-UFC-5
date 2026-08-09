@echo off
set "FILE=src/graphics/host_gpu/renderer/pipeline/descriptors.cpp"

REM Use git to generate sed-like behavior
G:\Apps\Git\usr\bin\sed.exe "s/^	const bool resource_ok = IsSupportedSampledDepthResource(resource);/	const bool depth_d24s8_packed_view =\n	    image != nullptr \&\&\n	    image->info.pixel_format == vk::Format::eD32SfloatS8Uint \&\&\n	    view_format == vk::Format::eA2B10G10R10UnormPack32 \&\&\n	    descriptor.Format() == Prospero::GpuEnumValue(Prospero::BufferFormat::k10_10_10_2UNorm);\n	if (depth_d24s8_packed_view) {\n		return;\n	}\n	const bool resource_ok = IsSupportedSampledDepthResource(resource);/" "%FILE%"
echo exit: %ERRORLEVEL%
