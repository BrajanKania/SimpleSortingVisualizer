# Simple Sorting Visualizer
This is a super simple sorting visualizer. It supports only iterative algorithms like bubble sort, shell sort, etc. It draws a new frame only when values are swapped.

## Showcase

<div align="center">
  <table>
    <tr>
      <td align="center">
        <img width="300" alt="ShellSort" src="https://github.com/user-attachments/assets/bcec7a71-64c4-45e9-9d34-fc681c5cfaca" /><br>
        <b>Shell sort</b>
      </td>
      <td align="center">
        <img width="300" alt="KocktailSort" src="https://github.com/user-attachments/assets/dc3ee05c-d3ef-4583-824a-ab52164a90b6" /><br>
        <b>Cocktail sort</b>
      </td>
    </tr>
    <tr>
      <td align="center">
        <img width="300" alt="SelectionSort" src="https://github.com/user-attachments/assets/fcae406a-e353-4846-9bd3-5302f8bec49a" /><br>
        <b>Selection sort</b>
      </td>
      <td align="center">
        <img width="300" alt="InsertionSort" src="https://github.com/user-attachments/assets/8b286134-daa1-4fb3-8b7d-fe2a665c9c1b" /><br>
        <b>Insertion sort</b>
      </td>
    </tr>
  </table>
</div>

## Main technologies
* **C++ 23**
* **SDL3**
* **OpenGL Core**


## Requirements
You will need: 
* **CMake 3.24** 
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
``` 
-DCMAKE_TOOLCHAIN_FILE="YOUR_PATH_TO/vcpkg.cmake"
```

