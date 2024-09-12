This is a D3D12 graphics engine made to further my education in performant graphics programs.  
I currently have a focus on realistic PBR style graphics  
See LTS branch for working buildable codebase  

Here are some notable accomplishments I've done so far:  
  -PBR shaders based on the metalness-roughness workflow. GGX Distribution, Schlick Fresnel, Smith Geometry  
  -Irradiance Maps  
  -Cascaded Shadow Maps. 1-4 Cascades, tight fit to frusta and scene, PCF sampling with Poisson Disc using Blue noise offsets. Rotational Invariance. Multi-Viewport with GS  
  -GLTF, GLB and Wavefront Obj model loading  
  -PNG, TGA, DDS, HDR texture loading with mip map support  
  -Dear ImGui support  
  -Hot reloading for shaders  
  -Cubemap or Equirectangular Skyboxes  
  -Thin Film Interference (Bubbles/Glass)
  -Asynchronous Loading  
  -Depth Prepass w/ Alpha Testing  
  -Shader Permutations  

Controls:  
  -Use the mouse to move the camera around  
  -Hold right click to rotate the camera  
  -Use the scroll wheel to move forward/backwards  
  -Hold the scroll wheel to pan the camera  
  -G to hide the GUI  
  -V to toggle Vsync  
  -F11 or Alt+Enter to toggle fullscreen  

![image](https://github.com/user-attachments/assets/9b6bfc77-1d35-4b7c-8b4b-495e69e1ddad)
![image](https://github.com/user-attachments/assets/7fa0fdcb-b506-4afc-8f38-b24d071a49bd)
![image](https://github.com/user-attachments/assets/afea88d9-2b6c-43a4-b704-2993f7d55e26)
![image](https://github.com/user-attachments/assets/503f9e75-3649-4f22-a990-88b2b7fdd08e)

