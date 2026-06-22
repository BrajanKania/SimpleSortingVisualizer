# Simple sorting visualizer
This is a super simple sorting visualizer. It supports only iterative algorithms like bubble sort, selection sort, etc. It draws a new frame only when values are swapped.

## Main technologies
* **C++ 23**
* **SDL3**
* **OpenGL Core**
* **GLAD**

## Requirements
You will need: 
* **CMake** 
* **vcpkg**

## Clone and build
```
git clone https://github.com/BrajanKania/SimpleSortingVisualizer.git
```
### Configure and build
```
cmake -B ./build
cmake --build ./build
```
If you encounter a CMake warning regarding a missing VCPKG_ROOT, you need to add: <br>
`-DCMAKE_TOOLCHAIN_FILE="YOUR_PATH_TO/vcpkg.cmake"`.

| Bubble sort | Insertion sort | Selection sort |
| :---: | :---: | :---: |
| <img width="270" alt="BubbleSort" src="https://github.com/user-attachments/assets/c29c0a18-f58f-4f85-805b-da54b35adff6" /> | <img width="270" alt="InsertionSort" src="https://github.com/user-attachments/assets/e1b6d969-f870-4144-a956-8a43e6790d6d" /> | <img width="270" alt="SelectionSort" src="https://github.com/user-attachments/assets/276bcb53-3784-409a-b498-e13d4f62138d" /> |
