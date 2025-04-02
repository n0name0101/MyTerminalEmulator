# MyTerminalEmulator 💻

Welcome to the **MyTerminalEmulator** project! MyTerminalEmulator is a modern terminal emulator with a text editor interface built using GTK3. This application allows you to run commands, view terminal output, and interact with your system in a clean, responsive graphical environment.

---

## Application Overview 📄

MyTerminalEmulator provides a powerful terminal interface that:
- Displays terminal output in a text editor–like window.
- Dynamically calculates the maximum number of columns and rows based on the window size and current monospace font metrics.

---

## Key Features 🌟

- **Dynamic Window Sizing:**  
  Automatically recalculates the maximum number of text columns and rows whenever the window is resized.

- **Modern GTK3 GUI:**  
  Offers an intuitive and responsive terminal emulator interface built with GTK3.

- **Real-Time Terminal Output:**  
  Updates the terminal view in real time as commands are executed.

---

## Screenshots 📸

Below are some example screenshots of MyTerminalEmulator in action:

### Terminal Interface
![Terminal Interface Screenshot](screenshots/terminal.png)

### Terminal Running VIM
![Terminal Running VIM](screenshots/vim.png)

### Terminal Running TOP
![Terminal Running TOP](screenshots/top.png)

---

## System Requirements ⚙️

- **Platform:**  
  MyTerminalEmulator is built with GTK3 and can run on Linux, Windows (with the appropriate GTK runtime), and macOS.

- **Compiler:**  
  GCC or any compatible C compiler.

- **Dependencies:**  
  - **GTK+ 3.x**
  - **Pango**
  - **pkg-config**

---

## Compilation Guide 🛠️

### Using GCC on Linux

1. **Clone the Repository:**

   ```sh
   git clone https://github.com/n0name0101/MyTerminalEmulator.git
   cd MyTerminalEmulator
2. **Compile:**

   ```sh
   make
