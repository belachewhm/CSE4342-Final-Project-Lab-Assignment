# CSE4342-Final-Project-Lab-Assignment

This project was the final lab assignment for CSE 4342 - Embedded Systems II 
at The University of Texas at Arlington. In it, we demonstrate the use of 
multi-threaded programming, network communication via windows sockets, real-time 
data acquisition on a DT9816 data acquisition module, and signal processing with 
an FIR filter. 

Two programs, a client and server, begin on two different computers. The client 
program takes in the following input from the user: a file name of filter coefficients, 
the sampling rate to use with the DT9816 real time data acquisition board, and a 
begin command. The coefficients and sampling rate are relayed to the server program 
immediately upon receiving them. Upon receiving the begin command, the client program 
sends a start command to the server program, and data acquisition begins on the 
server. The server interfaces with the DT9816 board and reads in analog input from 
input channel 0, which is hooked up to a 5v switch. When the switch is asserted to 
5v, the server stops reading the analog input on channel 0, and begins reading analog 
input on channel 1, which is hooked up to a sinusoidal wave generator.

As long as input channel 0 is asserted to 5v, the server continuously reads in 
each buffer of the wave, processes the buffer by convoluting it with the coefficients 
and values from the previous buffer, and sends the values back to the client. 
The client saves these values to a .csv file. In addition, the server sends the 
current maximum value, minimum value, average of values, variance of values, and sum 
of values from the processed data to the client. When the input channel 0 is toggled, 
the server program will pause and resume the data acquisition process as necessary. 
When the user types “STOP” on the client program, a stop command is sent to the server 
and both programs terminate.

## Contributors
- Belachew Haile-Mariam
	[(GitHub)](https://github.com/belachewhm)
	[(LinkedIn)](https://www.linkedin.com/in/belachew-haile-mariam-02259165)
- Kevin Marnell
	[(GitHub)](https://github.com/)
	[(LinkedIn)](https://www.linkedin.com/in/kevin-marnell-644925a4)
- Picard Folefack
	[(GitHub)](https://github.com/)
	[(LinkedIn)](https://www.linkedin.com/in/picard-folefack-79585b3a/en)