# Electronic-Circuit-Analyzer-Using-Cpp-and-Raylib

A graphical Electronic Circuit Analyzer built using **C++ and raylib**, designed to visually construct and analyze **series and parallel circuits** containing resistors, capacitors, and inductors.  
The application provides **real-time impedance analysis**, **interactive circuit diagrams**, and a **modern GUI** for educational use.

---

## 📌 Features

### 🔧 Circuit Construction
- Add **Resistors**, **Capacitors**, and **Inductors**
- Choose **Series** or **Parallel** connection
- Automatic unique **Component IDs**

### 🔍 Circuit Management
- Remove components by ID
- Search components by ID
- Undo last operation (up to 20 steps)

### 📊 Electrical Analysis
- Calculates:
  - Total **Resistance (R)**
  - Total **Impedance Magnitude |Z|**
- Uses **complex impedance**:
  - Resistor → `R`
  - Inductor → `jωL`
  - Capacitor → `−j/(ωC)`
- Frequency-based analysis (default: **50 Hz**)

### 🖥️ Visual Interface
- Interactive GUI using **raylib**
- Clean **Light Teal / Orange theme**
- Visual **series and parallel circuit diagrams**
- Custom symbols for R, L, and C

---

## 🧮 Supported Circuits

- Series Circuits
- Parallel Circuits
- Combined Series + Parallel (overall analysis)

---

## 🛠️ Technologies Used

- **Language:** C++
- **Graphics Library:** raylib
- **Math:** Complex numbers (`<complex>`)
- **Data Structures:**
  - `list` – component storage
  - `stack` – undo history
  - `queue` – operation log
  - `vector` – UI selections

---

---

## ▶️ How to Run

### Prerequisites
- C++ compiler (GCC / MinGW / MSVC)
- raylib installed
- `f1.ttf` font file in the same directory

### Compile (Example – GCC)
```bash
g++ main.cpp -o circuit_analyzer -lraylib -lopengl32 -lgdi32 -lwinmm

----
----



