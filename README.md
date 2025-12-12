# Ray Tracing vs Rasterization: Shadow Accuracy Analysis

This project compares shadow accuracy between a custom C++ ray tracer and a rasterization-based shadow renderer.  
We evaluate how well each method captures occlusion under different scene configurations, number of spheres, and number of light sources.

This repository includes:
- Full C++ source code (ray tracer + rasterizer)
- Rendered images for each test case
- Shadow percentage results
- LaTeX report (SIGGRAPH style)
- Presentation slides

---

## Project Overview

Shadows are fundamental in realistic rendering.  
Rasterization approximates shadows using depth-based methods, while ray tracing computes visibility using geometric intersections.

This project analyzes:
1. How shadow area percentages change depending on technique  
2. Where rasterization fails (aliasing, missing soft shadows)  
3. Where ray tracing improves accuracy (multiple lights, complex geometry)

The scenes range from:
- 3 spheres, 1 light  
- 5 spheres, 1 light  
- 5 spheres, 2 lights  
- 1 sphere, 2 lights  

---

## Results Summary

| Case | Rasterizer | Ray Tracer | Notes |
|------|------------|------------|-------|
| 3 spheres | 7.38% | 14.09% | Ray tracing captures occlusion better |
| 5 spheres | 5.71% | 3.82% | Rasterizer overestimates due to artifacts |
| 5 spheres, 2 lights | 1.05% | 5.00% | Multiple lights show ray-tracing advantage |
| 1 sphere, 2 lights | 0% | 2.76% | Rasterizer misses subtle shadows |

Full details are in the LaTeX report.

---

## 🛠️ How to Build & Run

### **Requirements**
- C++17 or newer
- g++ or clang++
- stb_image_write (included in repo)
- No textures required (project uses solid colors)

### **Build**
```bash
g++ -O2 -std=c++17 main.cpp -o render
