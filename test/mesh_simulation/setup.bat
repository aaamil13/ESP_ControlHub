@echo off
REM Setup script for mesh simulation virtual environment

echo Creating virtual environment...
python -m venv venv

echo.
echo Activating virtual environment...
call venv\Scripts\activate.bat

echo.
echo Installing requirements...
pip install --upgrade pip
pip install -r requirements.txt

echo.
echo Setup complete!
echo To activate the environment, run: venv\Scripts\activate.bat
