set GLSLC=C:\VulkanSDK\1.3.211.0\Bin\glslc.exe

%GLSLC% GLSL\v_terrainPTN.vert -o SPIRV\v_terrainPTN.spv
%GLSLC% GLSL\f_terrainPTN.frag -o SPIRV\f_terrainPTN.spv
%GLSLC% GLSL\v_planetPTN.vert -o SPIRV\v_planetPTN.spv
%GLSLC% GLSL\f_planetPTN.frag -o SPIRV\f_planetPTN.spv
%GLSLC% GLSL\v_trianglePCT.vert -o SPIRV\v_trianglePCT.spv
%GLSLC% GLSL\f_trianglePCT.frag -o SPIRV\f_trianglePCT.spv
%GLSLC% GLSL\v_trianglePT.vert -o SPIRV\v_trianglePT.spv
%GLSLC% GLSL\f_trianglePT.frag -o SPIRV\f_trianglePT.spv
%GLSLC% GLSL\v_linePC.vert -o SPIRV\v_linePC.spv
%GLSLC% GLSL\f_linePC.frag -o SPIRV\f_linePC.spv
%GLSLC% GLSL\v_pointPC.vert -o SPIRV\v_pointPC.spv
%GLSLC% GLSL\f_pointPC.frag -o SPIRV\f_pointPC.spv
%GLSLC% GLSL\v_hudPT.vert -o SPIRV\v_hudPT.spv
%GLSLC% GLSL\f_hudPT.frag -o SPIRV\f_hudPT.spv
%GLSLC% GLSL\v_hudPT.vert -o SPIRV\v_atmosphere.spv
%GLSLC% GLSL\f_hudPT.frag -o SPIRV\f_atmosphere.spv
%GLSLC% GLSL\v_sunPT.vert -o SPIRV\v_sunPT.spv
%GLSLC% GLSL\f_sunPT.frag -o SPIRV\f_sunPT.spv
%GLSLC% GLSL\v_sea.vert -o SPIRV\v_sea.spv
%GLSLC% GLSL\f_sea.frag -o SPIRV\f_sea.spv

pause