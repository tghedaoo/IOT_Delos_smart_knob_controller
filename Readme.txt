This project was jointly executed by Mohith Shanthigodu and Tushar G. Hedaoo

# Application:
An interactive UI allows the user to check status of the controllable stoves and allows control of the knobs.
The user can set up different heat levels and cook times for the knob and schedule the cooking tasks.
The communication between the user device and the knob happens over MQTT and the user makes different control requests over different MQTT topics.
While the cooking is in progress, a thermal camera reads a thermal image of the stove and streams live temperature data onto the interactive UI.
A servo feedback is also used to determine exact position of the knob. The data can be viewed on the UI using the motor gauge.

# Additional Features:
The user can check for device details using an interactive CLI. Upon running the application, pressing SW0 triggers the entry into CLI.
The user has the provision to trigger an Over the air firmware update request from the interactive UI.
