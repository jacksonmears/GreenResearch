# Golf Green LiDAR Visualization & Slope Analysis

This project processes **LiDAR scans of a golf putting green** to generate a **3D visualization** of the terrain, computes the **slopes at each region**, and visualizes the **downhill direction**. It is intended as a research/visualization tool for studying putting green topography and can serve as the basis for further AI or simulation work.

---

## Features

- **3D Particle Visualization**  
  Renders points from the LiDAR scan in 3D using SDL3 and OpenGL.

- **Slope Computation**  
  - Computes local planar fits for each grid cell.  
  - Calculates slope magnitude as a percentage.  
  - Outputs slope direction vectors to visualize downhill flow.

- **Color Coding by Slope**  
  - Color gradient indicates slope magnitude (green → red).  
  - Steeper slopes are highlighted in red for quick visual analysis.

- **Interactive Camera**  
  - Rotate with mouse drag.  
  - Zoom in/out with mouse wheel.  

- **Efficient Data Handling**  
  - Supports millions of points with preallocated vectors.  
  - Uses spatial hashing (`cellMap`) for neighborhood-based slope calculations.

---

## Requirements

- **C++17 or higher**  
- **OpenCL 2.0** (`CL/cl.h`)  
- **OpenGL**  
- **GLEW**  
- **SDL3** with OpenGL support  

Make sure all libraries are properly installed and linked.

---

## Setup & Compilation

WILL COME LATER I'M SORRY I JUST WANT IT TO WORK FIRST!!!


## Notes

- The main/master branch is not always the latest version.
- I am developing this project step-by-step; each branch may include new experiments, features, or visualizations.
- For the most up-to-date work, check the most recently updated branches.
- This process WILL eventually lead to a YouTube video documenting the methodology.


# Project Structure (will definitely change before I update the README I'm sorry)

- main.cpp – Main program with LiDAR reading, slope computation, and visualization.
- headers/Config.h – Configuration parameters (e.g., grid resolution).
- headers/fetch_grid.h – Spatial hashing functions.
- headers/calculate_slopes.h – Plane fitting and slope calculations.
- point_clouds/ – Directory for input LiDAR scans.


## Contact / Updates

For questions, feedback, or collaboration: jackson.mears2002@gmail.com


Screenshots and visualizations are available in the `images/` folder.  


