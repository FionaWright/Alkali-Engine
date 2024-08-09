This is a D3D12 graphics engine made to further my education in performant graphics programs.  
I currently have a focus on realistic PBR style graphics  

Here are some notable accomplishments I've done so far:  
  PBR shaders based on the metalness-roughness workflow. GGX Distribution, Schlick Fresnel, Smith Geometry  
  Irradiance Maps  
  Cascaded Shadow Maps. 1-4 Cascades, tight fit to frusta and scene, PCF sampling with Poisson Disc using Blue noise offsets. Rotational Invariance  
  GLTF, GLB and Wavefront Obj model loading  
  PNG, TGA, DDS, HDR texture loading with mip map support  
  Dear ImGui support  
  Hot reloading for shaders  
  Cubemap or Equirectangular Skyboxes  
  As well as various other basic stuff like normal maps, transparency, scene management, debug/visualisation options, etc  

Controls:  
  Use the mouse to move the camera around  
  Hold right click to rotate the camera  
  Use the scroll wheel to move forward/backwards  
  Hold the scroll wheel to pan the camera  
