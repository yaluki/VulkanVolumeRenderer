# Vulkan Volume Renderer
A small prototype using Vulkan to render a volumetric data set using ray tracing.

## Controls
The camera can be moved using WASD and rotated by left-clicking and dragging with the mouse.

## Creating a new data set
To create a new data set the script /data/scripts/datastructure_generator.py can be used:
```
python datastructure_generator.py path_to_folder
```
The script needs Python 3.x with Pillow.

*path_to_folder* should be a folder containing 2<sup>n</sup> images each 2<sup>n</sup> times 2<sup>n</sup> dimensioned. 
The script produces a text file Ouput.txt which can be specified when instancing the Application:
```
VulkanApplication* vulkanApplication = new VulkanApplication("Output.txt");
```

## Credits
The project is partially based on Sascha Willems Ray Tracing example.

