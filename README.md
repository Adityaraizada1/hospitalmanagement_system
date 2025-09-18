Hospital Management System
A simple, modern, and easy-to-use desktop application for managing patient data, built with C and the GTK+3 toolkit. This project is a self-contained executable with a professional UI, designed to be lightweight and efficient.

🚀 Live Demo
A quick look at the application flow from start to finish.

✨ Key Features
Feature

Description

Modern UI

A clean and professional interface styled with embedded CSS for a great user experience.

Splash Screen

A beautiful loading screen with a progress bar that appears on application startup.

Secure Login

A staff login dialog protects access to sensitive patient data.

Patient Management

Full CRUD functionality: Add, Edit, Delete, Search, and Export patient records.

AI Assistant

A simple tool to suggest the correct medical department based on patient symptoms.

Self-Contained

The entire application logic and styling are packed into a single file for easy compilation.

🛠️ Technology Stack
Language: C

UI Toolkit: GTK+3

Compiler: GCC

Build System: pkg-config

🏁 Getting Started
Follow these steps to get the application running on your local machine.

1. Prerequisites

First, ensure you have a C compiler and the GTK+3 development libraries installed.

On Debian/Ubuntu Linux:

sudo apt-get install build-essential libgtk-3-dev

On Fedora/CentOS Linux:

sudo dnf install gcc gtk3-devel

On macOS (using Homebrew):

brew install gtk+3 pkg-config

2. Compile from Source

Navigate to the project directory in your terminal and run the compilation command:

gcc main.c -o hospital_mgmt $(pkg-config --cflags --libs gtk+-3.0)

This will create a single executable file named hospital_mgmt.

3. Run the Application

Before the first run, create an empty patients.txt file to store data:

touch patients.txt

Now, launch the application:

./hospital_mgmt

💻 Usage
When the login screen appears, use the default credentials:

Username: admin

Password: password

You can then proceed to manage patients or use the AI Assistant.

