#!/bin/bash

# Start LibreOffice in headless mode in the background
soffice --headless --invisible --nologo --nofirststartwizard &

# Wait a bit to ensure LibreOffice has started
sleep 5

# Run the main Python script
python ./main.py

