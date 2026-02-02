# Dynamic Menu Test Application

This is a standalone Qt/C++ application designed to test dynamic menu manipulation. It allows users to add, modify, and remove top-level menus, submenus, and actions in real-time.

## Features

- **Top-level Menu Management**: Add, modify, or remove menus from the menu bar.
- **Action Management**: Add actions to any existing menu or submenu.
- **Submenus**: Create nested menu structures.
- **Checkable & Radio Actions**: Test different action types and mutual exclusion groups.
- **Property Editing**: Edit text, icons (from theme), shortcuts, and enabled states.
- **Separators**: Add visual separators to menus.

## Requirements

- C++17 compiler
- Qt 6 (Widgets, Core, Gui components)
- CMake 3.16 or newer

## Build Instructions

To build the application, follow these steps:

1. **Create a build directory**:
   ```bash
   mkdir build
   cd build
   ```

2. **Configure the project**:
   ```bash
   cmake ..
   ```

3. **Build the application**:
   ```bash
   make
   ```

4. **Run the application**:
   ```bash
   ./DynamicMenuTest
   ```
